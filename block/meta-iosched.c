#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/slab.h>
#include <linux/genhd.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#include "meta-iosched.h"

struct queue_arr {
	struct request_queue **queues;
	int n_queues;
	int size;
};

static struct queue_arr all;

static struct gendisk* disks[20];
int n_disks = 0;

void add_my_disk(struct gendisk* disk) {
	if (n_disks < 20)
		disks[n_disks++] = disk;
	printk("aaaaa, disk %s added. N disks: %d\n", disk->disk_name, n_disks);
}

char* get_disk_name(struct request_queue* q) {
	int i;
	for(i = 0; i < n_disks; i++)
		if (q == disks[i]->queue)
			return disks[i]->disk_name;
	return "";
}

struct request_queue* get_queue_by_name(char *name) {
	int i, k;
	for(i = 0; i < n_disks; i++) {
		bool equals = true;
		k = 0;
		while (name[k] != 0 && name[k] != '\n' && disks[i]->disk_name[k] != 0) {
			if (name[k] != disks[i]->disk_name[k]) {
				equals = false;
				break;
			}
			k++;
		}
		if (equals)
			return disks[i]->queue;
	}
	return NULL;
}

struct kobject *group_kobj;

static int interval;
static int ratio;
static int low_prio; // need these variables only for making kobj_attributes with good names
static int high_prio;
static int stats;

static ssize_t interval_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	printk("interval_show %d\n", interval);
	return sprintf(buf, "%d\n", interval);
}

static ssize_t interval_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int ret;
	printk("interval_store was %d\n", interval);

	ret = kstrtoint(buf, 10, &interval);
	printk("interval_store is now %d\n", interval);

	if (ret < 0)
		return ret;

	return count;
}

static ssize_t ratio_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", ratio);
}

static ssize_t ratio_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int ret;

	ret = kstrtoint(buf, 10, &ratio);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t high_prio_show(struct kobject *kobj, struct kobj_attribute *attr,
                        char *buf)
{
	int i;
	int buf_ptr = 0;
	for (i = 0; i < all.n_queues; i++)
		if (all.queues[i]->has_priority) {
			buf_ptr += sprintf(buf + buf_ptr, "%s\n", get_disk_name(all.queues[i]));
		}
	return buf_ptr;
}

static ssize_t high_prio_store(struct kobject *kobj, struct kobj_attribute *attr,
                         const char *buf, size_t count)
{
	struct request_queue *q = get_queue_by_name(buf);
	if (q)
		q->has_priority = true;
	else
		printk("aaaaa wtf high cannot understand %s!!!\n", buf);
        printk("aaaaa high trying to add %s\n", buf);
        return count;
}


static ssize_t low_prio_show(struct kobject *kobj, struct kobj_attribute *attr,
                        char *buf)
{
	int i;
	int buf_ptr = 0;
	for (i = 0; i < all.n_queues; i++)
		if (!all.queues[i]->has_priority) {
			buf_ptr += sprintf(buf + buf_ptr, "%s\n", get_disk_name(all.queues[i]));
		}
	return buf_ptr;
}

static ssize_t low_prio_store(struct kobject *kobj, struct kobj_attribute *attr,
                         const char *buf, size_t count)
{
	struct request_queue *q = get_queue_by_name(buf);
	if (q)
		q->has_priority = false;
	else
		printk("aaaaa wtf low cannot understand %s!!!\n", buf);
        printk("aaaaa low trying to add %s\n", buf);
        return count;
}

char my_stats[1000];

static ssize_t stats_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%s\n", my_stats);
}

static ssize_t stats_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	return count;
}


static struct kobj_attribute interval_attribute =
	__ATTR(interval, 0664, interval_show, interval_store);

static struct kobj_attribute ratio_attribute =
	__ATTR(ratio, 0664, ratio_show, ratio_store);

static struct kobj_attribute low_prio_attribute =
	__ATTR(low_prio, 0664, low_prio_show, low_prio_store);

static struct kobj_attribute high_prio_attribute =
	__ATTR(high_prio, 0664, high_prio_show, high_prio_store);

static struct kobj_attribute stats_attribute =
	__ATTR(stats, 0664, stats_show, stats_store);

static struct attribute *attrs[] = {
	&interval_attribute.attr,
	&ratio_attribute.attr,
	&low_prio_attribute.attr,
	&high_prio_attribute.attr,
	&stats_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

int kobj_init(void) {
	group_kobj = kobject_create_and_add("group-iosched", block_depr);
	if (!group_kobj)
		return -ENOMEM;
	return sysfs_create_group(group_kobj, &attr_group);
}

bool add_queue(struct request_queue *q) {
	if (all.size > all.n_queues) {
		all.queues[all.n_queues++] = q;
		return true;
	}

	all.queues = krealloc(all.queues, (all.n_queues + 1) * 2 * sizeof(q), GFP_KERNEL);
	if (!all.queues) return false;
	all.size = (all.n_queues + 1) * 2;
	all.queues[all.n_queues++] = q;
	return true;
}

int find_queue(struct queue_arr arr, struct request_queue *q) {
	int i;
	for (i = 0; i < arr.n_queues; i++)
		if (arr.queues[i] == q) return i;
	return -1;
}

bool del_queue(struct request_queue *q) {
	int pos;
	pos = find_queue(all, q);
	if (pos == -1) return false;
	all.queues[pos] = all.queues[all.n_queues - 1];
	all.n_queues--;
	return true;
}

int whole_stat = 0;

void print_stats(unsigned long unused) {
	int i;
	int printed = sprintf(my_stats, "aaaaa time=%u\tid\tload\tpercent\t(whole=%d)\n",
				jiffies, whole_stat);
	// all numbers will not be matched because of continuing of IO processes
	// there need a synchronization for an ideal statistics
	for (i = 0; i < all.n_queues; i++) {
		int t = all.queues[i]->stats[0] + all.queues[i]->stats[1] + all.queues[i]->stats[2];
		printed += sprintf(my_stats + printed, "aaaaa \t\t%d\t%d\t%d\n",
			all.queues[i]->id, t, t * 100 / (whole_stat + 1));
	}
	my_stats[printed] = 0;
	printk(KERN_DEBUG my_stats);
	init_my_timer();
}

// This function returns 0 if queue must be dispatched now
// Otherwise it calculates amount of time to sleep
int calc_time_to_sleep(struct request_queue *q) {
	int i;
	if (q->has_priority) return 0; // This queue has priority
	for (i = 0; i < all.n_queues; i++) {
		if (all.queues[i]->has_priority && all.queues[i]->nr_pending > 0)
			return 10; //Bad magician constant
	}
	return 0;
}

int group = 0;

spinlock_t *stat_lock;
spin_lock_init(stat_lock);

void clean_up(unsigned long unused) {
	int i;
	int level = (group + 1) % 3;
	for (i = 0; i < all.n_queues; i++) {
		spin_lock(stat_lock);
		whole_stat -= all.queues[i]->stats[level];
		spin_unlock(stat_lock);
		all.queues[i]->stats[level] = 0;
	}
	group = level;
	init_my_switch_timer();
}

void update_stats(struct request_queue *q) {
	spin_lock(stat_lock);
	whole_stat++;
	spin_unlock(stat_lock);
	q->stats[group]++;
}

struct timer_list my_timer;

void init_my_timer(void) {
	init_timer(&my_timer);
	my_timer.expires = jiffies + 10 * HZ; // after 10 seconds
	my_timer.data = 0;
	my_timer.function = print_stats;
	add_timer(&my_timer);
}

struct timer_list my_switch_timer;

void init_my_switch_timer(void) {
	init_timer(&my_switch_timer);
	my_switch_timer.expires = jiffies + 4 * HZ; // 3 buffers. Current stat is 10 seconds
	// 4 means piece of stat from 8 to 12 seconds.
	my_switch_timer.data = 0;
	my_switch_timer.function = clean_up;
	add_timer(&my_switch_timer);
}
