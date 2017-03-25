#include <linux/mm.h>
#include <linux/vmalloc.h>
#include "tp_types.h"
#include "osal.h"
#include "common.h"

UCHAR       __inbyte(USHORT port);
void        __outbyte(USHORT port, UCHAR data);
void		__debugbreak(void);

#define DUMP_DATA_MAX_CHUNKS 8192
#define DUMP_DATA_CHUNK 256
#define DUMP_DATA_SIZE (DUMP_DATA_CHUNK * DUMP_DATA_MAX_CHUNKS)

#define NO_SECONDS_YEAR 31536000
#define NO_SECONDS_DAY 86400
#define NO_SECONDS_HOUR 3600
#define NO_SECONDS_MIN 60

static char *dump_data;
static volatile long dump_entries;

#define O_CREAT         00000100        /* not fcntl */
#define O_RDWR          00000002

struct file;

struct file* file_open(const char* path, int flags, int rights);
int file_close(struct file* file);
int file_read(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size);
int file_write(struct file* file, unsigned char* data, unsigned int size);

unsigned char get_cur_apic_id(void);


void init_log(void)
{
	dump_data = (char*)tp_alloc(ROUND_TO_PAGES(DUMP_DATA_SIZE));
	if (dump_data == NULL)
		return;

    TPmemset(dump_data, 0, DUMP_DATA_SIZE);

	dump_entries = 0;
}

void release_log(void)
{
	int amount;
	struct file* file = file_open("/home/roee/Desktop/tplog", O_RDWR | O_CREAT, 0777);
	if (file == NULL) goto free_resources;
	printk("dump_data=%p\n",dump_data);
	amount = file_write(file, dump_data, DUMP_DATA_SIZE);
	printk("amount=%d\n",amount);
	printk("file_close(file)=%x\n",file_close(file));
free_resources:
	vfree(dump_data);
    dump_data = NULL;
}

void write_log(char *msg, UINT64 arg)
{
	long entry;
    UINT32 status;

	if (dump_data == NULL)
		return;

	entry = TP_ATOMIC_INC(&dump_entries) - 1;

//	if (entry >= DUMP_DATA_MAX_CHUNKS)
	//	__debugbreak();

//    time_fields = seconds_to_time(secs);

	status = snprintf(
		dump_data + entry * DUMP_DATA_CHUNK,
		DUMP_DATA_CHUNK,
		"%d> %s %llx",
		0,
		msg,
		arg);

//	KdPrint(("%d> %s %llx\n",
//		get_cur_apic_id(),
//		msg,
//		arg));
	dump_data[(entry + 1) * DUMP_DATA_CHUNK - 2] = '\r';
	dump_data[(entry + 1) * DUMP_DATA_CHUNK - 1] = '\n';
}
