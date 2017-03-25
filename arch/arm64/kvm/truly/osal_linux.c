#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/page-flags.h>
#include <linux/pagemap.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/tlbflush.h>
#include <asm/io.h>
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <linux/kmod.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kallsyms.h>
#include <linux/stat.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/namei.h>

#include "common.h"
#include "linux_kernfile.h"
#include "osal.h"

static  BOOLEAN is_vmalloc_address(PVOID addr);

#define IOMEM_FILE_PATH "/proc/iomem"
#define IOMEM_FILE_PERMS 444

#define MAX_LOW_PAGES 20
#define HIGHEST_IRQL 0xF
#define TP_PTE_PFN_MASK (((signed long)~(PAGE_SIZE-1)) & ((1ULL<<46) - 1))


__attribute((section(".mod1_end"))) static void mod1_end(void) {}
__attribute((section(".text_begin"))) static void text_begin(void) {}
__attribute((section(".mod1_begin"))) static void mod1_begin(void) {}

ATTR_SEC_MOD1 BOOLEAN find_section_in_cur_image(char *name, void **base, size_t *length)
{
    size_t begin_mod1, end_mod1;
    size_t begin_text, end_text;

    begin_text = ((size_t)text_begin + PAGE_SIZE) & ~0xFFF;
    end_text = ((size_t)mod1_end + PAGE_SIZE) & ~0xFFF;

    begin_mod1 = ((size_t)mod1_begin + PAGE_SIZE) & ~0xFFF;
    end_mod1 = (size_t)mod1_end & ~0xFFF;

    *base = (!TPmemcmp(name, ".text", sizeof(".text") - 1)) ? (void*)begin_text
            : (!TPmemcmp(name, ".mod1", sizeof(".mod1") - 1)) ? (void*)begin_mod1
            : NULL;

    *length = (!TPmemcmp(name, ".text", sizeof(".text") - 1)) ? begin_mod1 - begin_text
            : (!TPmemcmp(name, ".mod1", sizeof(".text") - 1)) ? end_mod1 - begin_mod1
            : 0;

    return *base && *length;
}



/*
* Allocates a physically contiguous memory chunk for up to PAGE_SIZE*MAX_LOWPAGES
* Otherwise allocates a virtually contiguous memory chunk.
*
*
*/
PVOID tp_alloc(size_t size)
{
	PVOID allocation;

	if ((allocation = (size <= PAGE_SIZE*MAX_LOW_PAGES)
		? kmalloc(size, GFP_ATOMIC) : vmalloc(size)) == NULL)
		return NULL;

	memset(allocation, 0, size);
	return allocation;
}


size_t get_cur_pid(void)
{
	return current->pid;
}

size_t get_cur_ppid(void)
{
    return current->real_parent != NULL ? current->real_parent->pid : 0UL;
}

static unsigned long get_file_size(const char* path)
{
	struct path p;
	struct kstat ks;
	kern_path(path, 0, &p);
	vfs_getattr(&p, &ks);

	return ks.size;
}

BOOLEAN read_file(wchar_t *name, void **content, size_t *content_size)
{
	struct file* file;
	BOOLEAN status = TRUE;
	void* p;
	size_t size = 0;

	if ((file = file_open((char*)name, O_RDONLY, IOMEM_FILE_PERMS)) == NULL)
		return FALSE;

	size = get_file_size((const char*)name);

	if ((p = tp_alloc(size)) == NULL)
		goto clean;

	if (file_read(file, 0, p, size) != size)
	{
		tp_free(p);
		status = FALSE;
	}
	else
	{
		*content = p;
		*content_size = size;
	}

	clean:
	file_close(file);
	return status;
}

static BOOLEAN is_vmalloc_address(PVOID addr)
{
	return (size_t)addr >= VMALLOC_START && (size_t)addr < VMALLOC_END;
}

void tp_free(PVOID p)
{
	if (is_vmalloc_address(p))
		vfree(p);
	else
		kfree(p);
}

void* map_physical_region_cached(UINT64 base, size_t size)
{
    return ioremap_cache(base, size);
}

PVOID map_physical_region(UINT64 base, size_t size)
{
	return ioremap_nocache(base, size);
}

PVOID map_physical_kernel_region(UINT64 addr) {
	return phys_to_virt(addr);
}

UINT64 io_read64(PVOID addr) {
	return readl(addr);
}

void unmap_physical_region(void* base, size_t size)
{
	return iounmap(base);
}

void vm_range_protect(PVOID startaddr, size_t size, UINT32 protection)
{
	if (!(protection & PROT_EXEC))
		set_memory_x((size_t)startaddr, size);
	else if (protection & PROT_EXEC)
		set_memory_nx((size_t)startaddr, size);
}


size_t get_process_cr3(UINT64 pid)
{
	struct task_struct* task = pid_task(find_vpid(pid), PIDTYPE_PID);

	return task ? (task->mm ? (UINT64)task->mm->pgd & ~0xFFFFF80000000000 : 0) : 0;
}


static BOOLEAN read_character(struct file* f, size_t offset, unsigned char* c)
{
	return file_read(f, offset, c, 1) == 1;
}

PVOID get_file_handler(wchar_t *path, int flags)
{
	int access_flags = 0;
	access_flags |= (flags & TP_FILE_READ) ? O_RDONLY : access_flags;
	access_flags |= (flags & TP_FILE_WRITE) ? O_WRONLY : access_flags;
	access_flags |= (flags & TP_FILE_RW) ? O_RDWR : access_flags;
	access_flags |= (flags & TP_FILE_CREATE) ? O_CREAT : access_flags;
	return file_open((const char*)path, access_flags, IOMEM_FILE_PERMS);
}

void file_handler_close(void* file_handler)
{
	file_close(file_handler);
}

size_t file_write_next(PVOID file_handler, void *in_data, size_t data_size)
{
	return file_write(file_handler, in_data, data_size);
}
size_t file_read_next(PVOID file_handler, void *out_data, size_t data_size)
{
	return file_write_foffset(file_handler, out_data, data_size);
}

UINT64 get_physical_memory_size(void)
{
#if 0
	struct sysinfo sysinf;
	si_meminfo(&sysinf);
	return sysinf.totalram*sysinf.mem_unit;
#endif
	struct file* f;
    size_t offset = 0;
    char *lastLine, *start, *end;
    int charCount = 0;
    UINT64 result = 0;
    unsigned char dummy;

    if ((f = file_open(IOMEM_FILE_PATH, O_RDONLY, IOMEM_FILE_PERMS)) == NULL)
        return 0;

    while (read_character(f, offset++, &dummy));

    for (offset-=2; read_character(f, --offset, &dummy) && dummy != '\n'; ++charCount);

    ++offset;
    lastLine = start = end = (char*)tp_alloc(charCount);
    file_read(f, offset, lastLine, charCount);
    while (*start++ != '-');
    while(*end++ != ' ');
    *(end-1) = '\0';
    kstrtoull(start, 16, &result);
    tp_free(lastLine);
    file_close(f);
    return result;
}

typedef struct
{
    volatile UINT32* ready_cnt;
    volatile UINT32* go_cnt;
    volatile UINT32* done_cnt;
	BOOLEAN done_event;
} DEFFERED_ROUTINE_ARG, *PDEFFERED_ROUTINE_ARG;

struct function_data
{
	SCHEDULED_FUNC  	func;
	struct task_struct* waiting_task;
	void* 				context;
	volatile BOOLEAN*	condition;
	PDEFFERED_ROUTINE_ARG arg;
};

int sched_func_wrap(void* d)
{
	struct function_data* data = (struct function_data*)d;
	if (data->arg->ready_cnt)
        TP_ATOMIC_INC(data->arg->ready_cnt);

    if (data->arg->go_cnt)
		while (!(*data->arg->go_cnt));

    data->func(data->context);
	if (data->arg->done_event)
	{
		wake_up_process(data->waiting_task);
		*(data->condition) = TRUE;
	}
	do_exit(0);
}

void schedule_func(SCHEDULED_FUNC func, void *context, int processor)
{
	static struct function_data data[8];
	static struct task_struct* th[8];
	volatile BOOLEAN condition = FALSE;
	static wait_queue_head_t queue;

	DEFFERED_ROUTINE_ARG arg = { 0 };
	arg.done_event = TRUE;

	init_waitqueue_head(&queue);

	data[processor].func = func;
	data[processor].waiting_task = current;
	data[processor].context = context;
	data[processor].condition = &condition;
	data[processor].arg = &arg;
	th[processor] = kthread_create(sched_func_wrap, &data[processor], "thp%d",processor);
	if (IS_ERR(th[processor]))
		return;
	kthread_bind(th[processor], processor);
	wake_up_process(th[processor]);
	wait_event(queue, condition);
}

volatile BOOLEAN do_nothing = FALSE;
static volatile BOOLEAN condition = FALSE;
static wait_queue_head_t global_queue;

int broadcast_wrap(void* d)
{
	struct function_data* data = (struct function_data*)d;

	if (do_nothing)
	{
		do_nothing = FALSE;
		wake_up(&global_queue);
		do_exit(0);
	}

    if (data->arg->ready_cnt)
        TP_ATOMIC_INC(data->arg->ready_cnt);

    if (data->arg->go_cnt) {
        while (!(*data->arg->go_cnt));
    }
	data->func(data->context);

	if (data->arg->done_cnt)
        TP_ATOMIC_INC(data->arg->done_cnt);

	do_exit(0);
}


BOOLEAN read_stored_key(char *reg_key)
{
	struct file* f;
	unsigned char c; int index=0;
	if ((f = file_open( "/etc/TrulyProtect/key", O_RDONLY, IOMEM_FILE_PERMS)) == NULL)
		return FALSE;
	while (read_character(f,index,&c))
	{
		reg_key[index++] = c;
	}
	file_close(f);
	return TRUE;
}


BOOLEAN read_system_data(wchar_t *dir_name, wchar_t *entry_name, char *buffer, size_t max)
{
	struct file* f;
	unsigned char c;
	int index=0;
	char dir_and_entry[strlen((char*)dir_name)+strlen((char*)entry_name)+1];

	strcpy(dir_and_entry, (const char*)dir_name);
	strcpy(dir_and_entry+strlen((char*)dir_name), (char*)entry_name);
	if ((f = file_open(dir_and_entry, O_RDONLY, IOMEM_FILE_PERMS)) == NULL)
		return FALSE;

	while (read_character(f,index,&c) && c != '\n')
	{
		buffer[index++] = c;
	}
    buffer[index] = 0;
	file_close(f);
	return TRUE;
}

pte_t* walk_page_table(struct mm_struct* mm, size_t addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep;

	pgd = pgd_offset(mm, addr);
	if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
		return NULL;

	pud = pud_offset(pgd, addr);
	if (pud_none(*pud) || unlikely(pud_bad(*pud)))
		return NULL;

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd))
		return NULL;

	ptep = pte_offset_map(pmd, addr);

	return ptep;
}


/*static BOOLEAN is_kernel_address(size_t linear_address)
{
	return linear_address >= 0xFFFF000000000000ULL;
}
*/
BOOLEAN MessageBox(wchar_t *title, wchar_t *message)
{
	return FALSE;
}

void KillProcess(size_t pid, OSSTATUS ExitStatus)
{
}

