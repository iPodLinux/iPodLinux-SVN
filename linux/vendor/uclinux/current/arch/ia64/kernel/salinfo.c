/*
 * salinfo.c
 *
 * Creates entries in /proc/sal for various system features.
 *
 * Copyright (c) 2001 Silicon Graphics, Inc.  All rights reserved.
 * Copyright (c) 2003 Hewlett-Packard Co
 *	Bjorn Helgaas <bjorn.helgaas@hp.com>
 *
 * 10/30/2001	jbarnes@sgi.com		copied much of Stephane's palinfo
 *					code to create this file
 */

#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/smp.h>

#include <asm/sal.h>
#include <asm/uaccess.h>

MODULE_AUTHOR("Jesse Barnes <jbarnes@sgi.com>");
MODULE_DESCRIPTION("/proc interface to IA-64 SAL features");
MODULE_LICENSE("GPL");

static int salinfo_read(char *page, char **start, off_t off, int count, int *eof, void *data);

typedef struct {
	const char		*name;		/* name of the proc entry */
	unsigned long           feature;        /* feature bit */
	struct proc_dir_entry	*entry;		/* registered entry (removal) */
} salinfo_entry_t;

/*
 * List {name,feature} pairs for every entry in /proc/sal/<feature>
 * that this module exports
 */
static salinfo_entry_t salinfo_entries[]={
	{ "bus_lock",           IA64_SAL_PLATFORM_FEATURE_BUS_LOCK, },
	{ "irq_redirection",	IA64_SAL_PLATFORM_FEATURE_IRQ_REDIR_HINT, },
	{ "ipi_redirection",	IA64_SAL_PLATFORM_FEATURE_IPI_REDIR_HINT, },
	{ "itc_drift",		IA64_SAL_PLATFORM_FEATURE_ITC_DRIFT, },
};

#define NR_SALINFO_ENTRIES ARRAY_SIZE(salinfo_entries)

static char *salinfo_log_name[] = {
	"mca",
	"init",
	"cmc",
	"cpe",
};

static struct proc_dir_entry *salinfo_proc_entries[
	ARRAY_SIZE(salinfo_entries) +			/* /proc/sal/bus_lock */
	ARRAY_SIZE(salinfo_log_name) +			/* /proc/sal/{mca,...} */
	(2 * ARRAY_SIZE(salinfo_log_name)) +		/* /proc/sal/mca/{event,data} */
	1];						/* /proc/sal */

struct salinfo_log_data {
	int	type;
	u8	*log_buffer;
	u64	log_size;
};

struct salinfo_event {
	int			type;
	int			cpu;		/* next CPU to check */
	volatile unsigned long	cpu_mask;
	wait_queue_head_t	queue;
};

static struct salinfo_event *salinfo_event[ARRAY_SIZE(salinfo_log_name)];

struct salinfo_data {
	int	open;		/* single-open to prevent races */
	int	type;
	int	cpu;		/* "current" cpu for reads */
};

static struct salinfo_data salinfo_data[ARRAY_SIZE(salinfo_log_name)];

static spinlock_t data_lock;

void
salinfo_log_wakeup(int type)
{
	if (type < ARRAY_SIZE(salinfo_log_name)) {
		struct salinfo_event *event = salinfo_event[type];

		if (event) {
			set_bit(smp_processor_id(), &event->cpu_mask);
			wake_up_interruptible(&event->queue);
		}
	}
}

static int
salinfo_event_open(struct inode *inode, struct file *file)
{
	if (!suser())
		return -EPERM;
	return 0;
}

static ssize_t
salinfo_event_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_dir_entry *entry = (struct proc_dir_entry *) inode->u.generic_ip;
	struct salinfo_event *event = entry->data;
	char cmd[32];
	size_t size;
	int i, n, cpu = -1;

retry:
	if (!event->cpu_mask) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		interruptible_sleep_on(&event->queue);
		if (signal_pending(current))
			return -EINTR;
	}

	n = event->cpu;
	for (i = 0; i < NR_CPUS; i++) {
		if (event->cpu_mask & 1UL << n) {
			cpu = n;
			break;
		}
		if (++n == NR_CPUS)
			n = 0;
	}

	if (cpu == -1)
		goto retry;

	/* for next read, start checking at next CPU */
	event->cpu = cpu;
	if (++event->cpu == NR_CPUS)
		event->cpu = 0;

	snprintf(cmd, sizeof(cmd), "read %d\n", cpu);

	size = strlen(cmd);
	if (size > count)
		size = count;
	if (copy_to_user(buffer, cmd, size))
		return -EFAULT;

	return size;
}

static struct file_operations salinfo_event_fops = {
	.open  = salinfo_event_open,
	.read  = salinfo_event_read,
};

static int
salinfo_log_open(struct inode *inode, struct file *file)
{
	struct proc_dir_entry *entry = (struct proc_dir_entry *) inode->u.generic_ip;
	struct salinfo_data *data = entry->data;

	if (!suser())
		return -EPERM;

	spin_lock(&data_lock);
	if (data->open) {
		spin_unlock(&data_lock);
		return -EBUSY;
	}
	data->open = 1;
	spin_unlock(&data_lock);

	return 0;
}

static int
salinfo_log_release(struct inode *inode, struct file *file)
{
	struct proc_dir_entry *entry = (struct proc_dir_entry *) inode->u.generic_ip;
	struct salinfo_data *data = entry->data;

	spin_lock(&data_lock);
	data->open = 0;
	spin_unlock(&data_lock);
	return 0;
}

static void
call_on_cpu(int cpu, void (*fn)(void *), void *arg)
{
	if (cpu == smp_processor_id())
		(*fn)(arg);
#ifdef CONFIG_SMP
	else if (cpu_online(cpu))	/* cpu may not have been validated */
		smp_call_function_single(cpu, fn, arg, 0, 1);
#endif
}

static void
salinfo_log_read_cpu(void *context)
{
	struct salinfo_log_data *info = context;
	struct salinfo_event *event = salinfo_event[info->type];
	u64 size;

	size = ia64_sal_get_state_info_size(info->type);
	info->log_buffer = kmalloc(size, GFP_ATOMIC);
	if (!info->log_buffer)
		return;

	clear_bit(smp_processor_id(), &event->cpu_mask);
	info->log_size = ia64_sal_get_state_info(info->type, (u64 *) info->log_buffer);
	if (info->log_size)
		salinfo_log_wakeup(info->type);
}

static ssize_t
salinfo_log_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_dir_entry *entry = (struct proc_dir_entry *) inode->u.generic_ip;
	struct salinfo_data *data = entry->data;
	struct salinfo_log_data info;
	int ret;
	void *saldata;
	size_t size;

	info.type = data->type;
	info.log_buffer = 0;
	call_on_cpu(data->cpu, salinfo_log_read_cpu, &info);
	if (!info.log_buffer || *ppos >= info.log_size) {
		ret = 0;
		goto out;
	}

	saldata = info.log_buffer + file->f_pos;
	size = info.log_size - file->f_pos;
	if (size > count)
		size = count;
	if (copy_to_user(buffer, saldata, size)) {
		ret = -EFAULT;
		goto out;
	}

	*ppos += size;
	ret = size;

out:
	kfree(info.log_buffer);
	return ret;
}

static void
salinfo_log_clear_cpu(void *context)
{
	struct salinfo_data *data = context;
	struct salinfo_event *event = salinfo_event[data->type];
	struct salinfo_log_data info;

	clear_bit(smp_processor_id(), &event->cpu_mask);
	ia64_sal_clear_state_info(data->type);

	/* clearing one record may make another visible */
	info.type = data->type;
	salinfo_log_read_cpu(&info);
	if (info.log_buffer && info.log_size)
		salinfo_log_wakeup(data->type);

	kfree(info.log_buffer);
}

static ssize_t
salinfo_log_write(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_dir_entry *entry = (struct proc_dir_entry *) inode->u.generic_ip;
	struct salinfo_data *data = entry->data;
	char cmd[32];
	size_t size;
	int cpu;

	size = sizeof(cmd);
	if (count < size)
		size = count;
	if (copy_from_user(cmd, buffer, size))
		return -EFAULT;

	if (sscanf(cmd, "read %d", &cpu) == 1)
		data->cpu = cpu;
	else if (sscanf(cmd, "clear %d", &cpu) == 1)
		call_on_cpu(cpu, salinfo_log_clear_cpu, data);

	return count;
}

static struct file_operations salinfo_data_fops = {
	.open    = salinfo_log_open,
	.release = salinfo_log_release,
	.read    = salinfo_log_read,
	.write   = salinfo_log_write,
};

static int __init
salinfo_init(void)
{
	struct proc_dir_entry *salinfo_dir; /* /proc/sal dir entry */
	struct proc_dir_entry **sdir = salinfo_proc_entries; /* keeps track of every entry */
	struct proc_dir_entry *dir, *entry;
	struct salinfo_event *event;
	struct salinfo_data *data;
	int i, j;

	salinfo_dir = proc_mkdir("sal", NULL);
	if (!salinfo_dir)
		return 0;

	for (i=0; i < NR_SALINFO_ENTRIES; i++) {
		/* pass the feature bit in question as misc data */
		*sdir++ = create_proc_read_entry (salinfo_entries[i].name, 0, salinfo_dir,
						  salinfo_read, (void *)salinfo_entries[i].feature);
	}

	for (i = 0; i < ARRAY_SIZE(salinfo_log_name); i++) {
		dir = proc_mkdir(salinfo_log_name[i], salinfo_dir);
		if (!dir)
			continue;

		entry = create_proc_entry("event", S_IRUSR, dir);
		if (!entry)
			continue;

		event = kmalloc(sizeof(*event), GFP_KERNEL);
		if (!event)
			continue;
		memset(event, 0, sizeof(*event));
		event->type = i;
		init_waitqueue_head(&event->queue);
		salinfo_event[i] = event;
		/* we missed any events before now */
		for (j = 0; j < NR_CPUS; j++)
			if (cpu_online(j))
				set_bit(j, &event->cpu_mask);
		entry->data = event;
		entry->proc_fops = &salinfo_event_fops;
		*sdir++ = entry;

		entry = create_proc_entry("data", S_IRUSR | S_IWUSR, dir);
		if (!entry)
			continue;

		data = &salinfo_data[i];
		data->type = i;
		entry->data = data;
		entry->proc_fops = &salinfo_data_fops;
		*sdir++ = entry;

		*sdir++ = dir;
	}

	*sdir++ = salinfo_dir;

	return 0;
}

static void __exit
salinfo_exit(void)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(salinfo_proc_entries); i++) {
		if (salinfo_proc_entries[i])
			remove_proc_entry (salinfo_proc_entries[i]->name, NULL);
	}
}

/*
 * 'data' contains an integer that corresponds to the feature we're
 * testing
 */
static int
salinfo_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;

	MOD_INC_USE_COUNT;

	len = sprintf(page, (sal_platform_features & (unsigned long)data) ? "1\n" : "0\n");

	if (len <= off+count) *eof = 1;

	*start = page + off;
	len   -= off;

	if (len>count) len = count;
	if (len<0) len = 0;

	MOD_DEC_USE_COUNT;

	return len;
}

module_init(salinfo_init);
module_exit(salinfo_exit);
