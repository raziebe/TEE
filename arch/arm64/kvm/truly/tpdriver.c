#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/path.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/notifier.h>
#include <linux/syscalls.h>
#include <linux/kmod.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/elf.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/kallsyms.h>

#include "tp_types.h"
#include "linux_kernfile.h"
#include "funcCipherStruct.h"
#include "elf.h"
#include "exec_prot_db_linux.h"
#include "ImageFile.h"
#include "ImageManager.h"

#define     MAX_PERMS 777
PVOID tp_alloc(size_t size);
void tp_free(PVOID p);

struct img_layout_info
{
    size_t pid;
    size_t base;
    size_t per_cpu_base;
    size_t length;
    char*  path;
    enum ep_module_group grp_type;
};

IMAGE_MANAGER         image_manager;
static struct mutex protected_image_mutex;

static void fill_img_layout_info (struct img_layout_info* info,
                                  size_t pid,
                                  size_t base,
                                  size_t per_cpu_base,
                                  size_t length,
                                  char*  path,
                                  enum ep_module_group grp_type)
{
    info->pid = pid;
    info->base = base;
    info->per_cpu_base = per_cpu_base;
    info->length = length;
    info->path = path;
    info->grp_type = grp_type;
}


static char* get_path_from_file(struct file* file, char** path_to_free)
{
    struct path* path;
    char* the_path, *path_name;

    if (!file)
        return NULL;

    path_name = the_path = NULL;
    path = &file->f_path;
    path_get(path);

    if ((the_path = tp_alloc(PAGE_SIZE*2)) == NULL)
    {
        path_put(path);
        goto end;
    }

    path_name = d_path(path, the_path, PAGE_SIZE);
    path_put(path);

    if (IS_ERR(path_name))
    {
        tp_free(the_path);
        path_name = NULL;
        goto end;
    }
    *path_to_free = the_path;
    end:
    return path_name;
}

static enum ep_module_group get_group_by_arch_and_type (char arch, char type)
{
    if (arch == ELF_ARCH_64)
        return type == ELF_TYPE_EXEC ? EP_MOD_EXEC64 : EP_MOD_SO64;

    return type == ELF_TYPE_EXEC ? EP_MOD_EXEC32 : EP_MOD_SO32;
}


static void vma_tp_load(struct vm_area_struct* vma, void* context)
{
    char* path, *path_to_free;

    // Ignore non-executable vm areas
    if (!(vma->vm_flags & VM_EXEC)){
        return;
    }

    path = get_path_from_file(vma->vm_file, &path_to_free);

    if (path != NULL)
    {
        struct img_layout_info info;
        struct file* file;
        char arch, type;
        enum ep_module_group grp_type;

        if ((file = file_open(path, O_RDONLY, MAX_PERMS)) == NULL)
        {
            file = vma->vm_file;
        }

        if (!get_elf_data(file, &arch, &type))
            goto clean;

        grp_type = get_group_by_arch_and_type(arch, type);
        fill_img_layout_info (&info,
                              current->pid,
                              vma->vm_start,
                              0,
                              vma->vm_end - vma->vm_start,
                              path,
                              grp_type);


  //      tp_handle_image_loading(&info);

clean:
        if (file != NULL && file != vma->vm_file)
            file_close(file);
        tp_free(path_to_free);
    }
}

void for_each_vma(struct task_struct* task, void* context, void (*callback)(struct vm_area_struct*, void*))
{
    struct vm_area_struct* p;
    if (!(task && task->mm && task->mm->mmap))
        return;
    for (p = task->mm->mmap; p ; p = p->vm_next)
        callback(p, context);
}

static char* executable_path(struct task_struct* process, char** path_to_free)
{
    #define PATH_MAX 4096
    char* p = NULL, *pathname;
    struct mm_struct* mm = current->mm;
    if (mm)
    {
        //down_read(&mm->mmap_sem);
        if (mm->exe_file)
        {
            pathname = kmalloc(PATH_MAX*2, GFP_ATOMIC);
            *path_to_free = pathname;
            if (pathname)
                p = d_path(&mm->exe_file->f_path, pathname, PATH_MAX);
        }
        //up_read(&mm->mmap_sem);
    }
    #undef PATH_MAX
    return IS_ERR(p) ? NULL : p;
}


/*
    This function should be called from tp_stub_execve.
    ret_value represents the return value of do_execve.
    see http://lxr.free-electrons.com/source/fs/exec.c
*/
void tp_execve_handler(unsigned long ret_value)
{
    char* exec_path, *path_to_free = NULL;
    PIMAGE_FILE image_file;
  //  OSSTATUS status = TP_SUCCESS;
    struct file* file;
    BOOLEAN is_protected;
    unsigned long base, size;

// Ignore if execve failed
    if (current->in_execve)
        goto clean_2;

    if ((exec_path = executable_path(current, &path_to_free)) == NULL)
        goto clean_2;

//    if (MemoryLayoutIsClonedProcess(image_manager.memory_layout, current->pid))
  //      MemoryLayoutRemoveProcess(image_manager.memory_layout, current->pid);
    // the first virtual-map is the actual base
    base = current->mm->mmap->vm_start;
    size = current->mm->mmap->vm_end - base;

    for_each_vma(current, (void*)&base, vma_tp_load);

    // The first virtual-map is the actual base
    base = current->mm->mmap->vm_start;

    if ((file = file_open(exec_path, O_RDONLY, MAX_PERMS)) == NULL)
        goto clean_2;

    mutex_lock(&protected_image_mutex);
    image_file = image_file_init(file);

    image_file_init_bases(image_file, base);
    is_protected = im_handle_image(&image_manager, image_file, (UINT64)current->pid, base);
    image_file_free(image_file);
/*
    if (is_protected && global_state == NULL)
    {
        KdPrint(("Launching TPVISOR..pid = %d\n", current->pid));
        status = launch_tpvisor();
    }
    */
    mutex_unlock(&protected_image_mutex);
/*
   if (status != TP_SUCCESS)
   {
      struct files_struct* fs = current->files;
      iterate_fd(fs, 0, redirect_error_to_process_stderr, describe_status(status));
      goto clean_1;
   }
*/
//clean_1:
    file_close(file);
clean_2:
   if (path_to_free)
      tp_free(path_to_free);
}

