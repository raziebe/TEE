#ifndef __TRULY_H__
#define __TRULY_H__

struct tp_cpu_context {
	unsigned long d1;
	unsigned long d2;
	unsigned long d3;
};

typedef struct tp_cpu_context tp_cpu_context_t;


#define tp_err(fmt, ...) \
	pr_err("truly [%i]: " fmt, task_pid_nr(current), ## __VA_ARGS__)
#define tp_info(fmt, ...) \
	pr_info("truly [%i]: " fmt, task_pid_nr(current), ## __VA_ARGS__)
#define tp_debug(fmt, ...) \
	pr_debug("truly [%i]: " fmt, task_pid_nr(current), ## __VA_ARGS__)

#endif
