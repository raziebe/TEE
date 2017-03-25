#ifndef __TP_TYPES_H__
#define __TP_TYPES_H__

#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/kthread.h>
#include <linux/sched.h>
/*
#include <linux/wait.h>
#include <linux/path.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/notifier.h>
#include <linux/syscalls.h>
#include <linux/kmod.h>
*/
// these aren't standard @ osx
#if defined(_OSX) || defined(_LINUX) || defined (__KERNEL__)
    #define __int64 long long
    #define __int32 int
    #define __int16 short
    #define __int8  char
    #define __cdecl
	#define  wchar_t int
#endif

#if defined(_MSC_VER)
#define ALIGNED_(x) __declspec(align(x))
#else
#if defined(__GNUC__)
#define ALIGNED_(x) __attribute__ ((aligned(x)))
#endif
#endif


typedef unsigned long ULONG, *PULONG;
typedef unsigned short USHORT, *PUSHORT;
typedef unsigned char UCHAR, *PUCHAR;
typedef void* PVOID;
typedef char* PCHAR;
typedef UCHAR BOOLEAN;


typedef unsigned long long UINT64, *PUINT64;
typedef unsigned int UINT32, *PUINT32;
typedef unsigned short UINT16, *PUINT16;
typedef unsigned char UINT8, *PUINT8;

#if defined(_X86_64)
typedef __int64 INT_PTR, *PINT_PTR;
#else
typedef int INT_PTR, *PINT_PTR;
#endif

typedef long OSSTATUS, *POSSTATUS;

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifndef _LINUX
    #define UNREFERENCED_PARAMETER(P)          (P)
#else
    #define UNREFERENCED_PARAMETER(P)          (P=P)
#endif

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

#ifndef NULL
#define NULL 0UL
#endif

//#pragma warning(disable : 4201)

typedef union
{
    struct
    {
        unsigned int LowPart;
        int HighPart;
    };
    long long QuadPart;
} LARGE_INT, *PLARGE_INT;

#endif
