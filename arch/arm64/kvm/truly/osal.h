
#include "tp_types.h"
#include "tp_string.h"

#ifndef __OSAL_H__
#define __OSAL_H__

void InitOsal(void);

BOOLEAN find_section_in_cur_image(char *name, void **base, size_t *length);

// Returns true if and only of accessing the given address will not generate a #PF
BOOLEAN is_memory_mapped(size_t addr);

typedef void(*for_each_virtual_page_callback)(size_t va, UINT64 pa, void *context);
void for_each_page_in_range(for_each_virtual_page_callback callback, void *context, void *base, UINT32 length);

typedef struct
{
	void* platform_dependent_name;
	void *module_base;
	UINT32 module_size;
	void *module_text;
	UINT32 text_size;
} KERNEL_MODULE, *PKERNEL_MODULE;

typedef void(*KERNEL_MODULE_CALLBACK)(PKERNEL_MODULE module, void *context);
void for_each_kernel_module(void *driver_context, KERNEL_MODULE_CALLBACK callback, void *context);

typedef void (*SCHEDULED_FUNC)(void *context);

typedef int (*HIGH_IRQL_FUNC)(void *context);

typedef void (*BROADCAST_FUNC)(void);

typedef void(*BROADCAST_FUNC_EX)(void*);

void broadcast_func(BROADCAST_FUNC func);
void broadcast_func_ex(BROADCAST_FUNC_EX func, void* context);

void*  p2v(UINT64 pa);
UINT64 v2p(void* virt);

void* map_physical_region(UINT64 base, size_t size);
void unmap_physical_region(void* base, size_t size);

void* map_physical_region_cached(UINT64 base, size_t size);
void* map_physical_kernel_region(UINT64 addr);

PVOID map_physical_kernel_region(UINT64 addr);

BOOLEAN get_acpi_table(char* signature, void** acpi_table_header, UINT32* table_length);

void* tp_alloc(size_t size);
void tp_free(void* p);
#define TP_NEW(X) ((X*)tp_alloc(sizeof(X)))

size_t get_process_cr3(UINT64 pid);
size_t get_cur_pid(void);
size_t get_cur_ppid(void);

void keep_os_alive(void);

char raise_irql(char irql);
char raise_irql_to_highest(void);
void lower_irql(char irql);

void tp_pte_set_x (size_t address, size_t size);

BOOLEAN read_stored_key(char *reg_key);

BOOLEAN read_file(wchar_t *name, void **content, size_t *content_size);
BOOLEAN write_file(wchar_t *name, void *content, size_t content_size);

void* get_file_handler(wchar_t* path, int flags);
void  file_handler_close(void* file_handler);
size_t file_write_next(void* file_handler, void *in_data, size_t data_size);
size_t file_read_next(void* file_handler, void *out_data, size_t data_size);

BOOLEAN read_system_data_ex(wchar_t *dir_name, wchar_t *entry_name, char **buffer, size_t *max);
BOOLEAN read_system_data(wchar_t *dir_name, wchar_t *entry_name, char *buffer, size_t max);

void get_syscall_arg_filename(char *dst, size_t *length, size_t cr3, size_t rsp, PVOID registers);

void clear_pages_dirty_bit(size_t startAddr, size_t endAddr);

UINT64 tp_set_pte(size_t address, UINT64 new_page_paddr);

BOOLEAN MessageBox(wchar_t *title, wchar_t *message);
void KillProcess(size_t pid, OSSTATUS ExitStatus);

#ifdef WINNT
    // A process ID of an immortal Windows process
    #define ALWAYS_ALIVE_PID 4

    // Visual C++ intrinsics
    #define TP_ATOMIC_INC(X) _InterlockedIncrement(X)
    #define TP_ATOMIC_DEC(X) _InterlockedDecrement(X)

    #define __attribute__(X)
    ULONG __cdecl DbgPrint(const char* Format, ... );
    #ifdef DISABLE_DEBUGGER
        #define KdPrint(_x_)
    #else
        #define KdPrint(_x_) DbgPrint _x_
    #endif
#else
    // A process ID of an immortal mac/linux process
#define ALWAYS_ALIVE_PID 1
// GCC intrinsics
#define TP_ATOMIC_INC(X) __sync_add_and_fetch(X, 1)
#define TP_ATOMIC_DEC(X) __sync_add_and_fetch(X, -1)

#ifdef DISABLE_DEBUGGER
    #define KdPrint(_x_)
#elif defined(_OSX)
    #define KdPrint(_x_) printf _x_
#else // Linux
    extern int printk(const char*, ...);
    #define KdPrint(_x_) printk _x_
#endif

#endif

#define PROT_READ       0x01    /* pages can be read */
#define PROT_WRITE      0x02    /* pages can be written */
#define PROT_EXEC       0x04    /* pages can be executed */
#define PROT_ALL PROT_READ | PROT_WRITE | PROT_EXEC
#define DEF_PROT PROT_READ | PROT_WRITE

#ifdef WINNT
#ifndef ASSERT
__declspec(dllimport) void RtlAssert(void*, void*, unsigned long, char*);
#define ASSERT( exp ) \
    ((!(exp)) ? \
        (RtlAssert( #exp, __FILE__, __LINE__, NULL ),FALSE) : \
        TRUE)
#endif
#else
#define ASSERT( exp )
#endif

#if !defined(_LINUX) && !defined(_OSX)
	int __cdecl sprintf_s(char *, size_t, const char *, ...);
	int __cdecl swprintf_s(wchar_t*, size_t, const wchar_t *format, ...);
#else
	extern int snprintf(char*, size_t, const char*, ...);
	#define sprintf_s snprintf
	#define snprintf_s snprintf
	//extern int swprintf(wchar_t*, size_t, const wchar_t *format, ...);
	//#define swprintf_s swprintf
#endif

#define TP_FILE_READ       0
#define TP_FILE_WRITE      1
#define TP_FILE_RW         2
#define TP_FILE_CREATE     4

#endif // HEADER
