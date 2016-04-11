#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/slab.h>
#include <linux/genhd.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#include "meta-iosched.h"

struct queue_arr {
	struct request_queue **queues;
	int *stats;
	int n_queues;
	int size;
};

static struct queue_arr all;

static struct gendisk** disks;
int n_disks = 0;
int disks_size = 0;

bool add_my_disk(struct gendisk* disk) {
	printk("aaaaa, disk %s will be added. N disks: %d\n", disk->disk_name, n_disks);
	if (disks_size > n_disks) {
		disks[n_disks++] = disk;
		return true;
	}

	disks = krealloc(disks, (n_disks + 1) * 2 * sizeof(disk), GFP_KERNEL);
	if (!disks) return false;
	disks_size = (n_disks + 1) * 2;
	disks[n_disks++] = disk;
	return true;
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

static int interval = 10000;
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
	if (interval < 100) interval = 10000;
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
		all.stats[all.n_queues] = 0;
		all.queues[all.n_queues++] = q;
		return true;
	}

	all.stats = krealloc(all.stats, (all.n_queues + 1) * 2 * sizeof(int), GFP_KERNEL);
	all.queues = krealloc(all.queues, (all.n_queues + 1) * 2 * sizeof(q), GFP_KERNEL);
	if (!all.queues || !all.stats) return false;
	all.size = (all.n_queues + 1) * 2;
	all.stats[all.n_queues] = 0;
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
	all.stats[pos] = all.stats[all.n_queues - 1];
	all.n_queues--;
	return true;
}

int whole_stat = 0;

// Trying to get shapshot from all our queue statistics.
// while(honest--) is something like while(true)
// but performance is much more important than absolute honesty
int get_snapshot(int honest) {
	int i, s = 0;
	for (i = 0; i < all.n_queues; i++) {
		all.stats[i] = all.queues[i]->stats[2];
	}
	bool success;

	while(s < honest) {
		success = true;
		for (i = 0; i < all.n_queues; i++) {
			if (all.stats[i] != all.queues[i]->stats[2]) {
				success = false;
				all.stats[i] = all.queues[i]->stats[2];
			}
		}
		if (success) return s;
		s++;
	}
	return s;
}

void print_stats(unsigned long unused) {
	int i;
	int n_try = get_snapshot(20);
	int printed = sprintf(my_stats, "aaaaa time=%u\tid\tload\tpercent\t(whole=%d, try=%d)\n",
				jiffies, whole_stat, n_try);
	for (i = 0; i < all.n_queues; i++) {
		printed += sprintf(my_stats + printed, "aaaaa \t\t%d\t%d\t%d\t%d\n",
			all.queues[i]->id, all.stats[i], all.stats[i] * 100 / (whole_stat + 1),
			all.queues[i]->queue_lock);
	}
	my_stats[printed] = 0;
	printk(my_stats);
	init_my_timer();
}

// This function returns 0 if queue must be dispatched now
// Otherwise it calculates amount of time to sleep
int calc_time_to_sleep(struct request_queue *q) {
	int i;
	if (q->has_priority) return 0;
	for (i = 0; i < all.n_queues; i++) {
		if (all.queues[i]->has_priority && all.queues[i]->nr_pending > 0)
			return interval / 100;
	}
	return 0;
}

int group = 0;

spinlock_t stat_lock;
DEFINE_SPINLOCK(stat_lock);

void clean_up(unsigned long unused) {
	int i;
	int level = (group + 1) % 2;
	for (i = 0; i < all.n_queues; i++) {
		spin_lock(&stat_lock);
		whole_stat -= all.queues[i]->stats[level];
		spin_unlock(&stat_lock);

		spin_lock(all.queues[i]->queue_lock);
		all.queues[i]->stats[2] -= all.queues[i]->stats[level];
		spin_unlock(all.queues[i]->queue_lock);
		all.queues[i]->stats[level] = 0;
	}
	group = level;
	init_my_switch_timer();
}

void update_stats(struct request_queue *q) {
	spin_lock(&stat_lock);
	whole_stat++;
	spin_unlock(&stat_lock);
	q->stats[group]++;

	q->stats[2]++;
}

struct timer_list my_timer;

void init_my_timer(void) {
	init_timer(&my_timer);
	my_timer.expires = jiffies + interval * HZ / 1000;
	my_timer.data = 0;
	my_timer.function = print_stats;
	add_timer(&my_timer);
}

struct timer_list my_switch_timer;

void init_my_switch_timer(void) {
	init_timer(&my_switch_timer);
	// We have 2 buffers with partial statistics. We want to have statistics from the last
	// interval of time. Imagine interval = 1.5 * x. We are able to get statistics from x to 2 * x
	// moment. So we need to switch our timer every interval * 2 / 3 / 1000 seconds
	// (interval storing time in msecs)
	my_switch_timer.expires = jiffies + interval * HZ * 2 / 3000;
	my_switch_timer.data = 0;
	my_switch_timer.function = clean_up;
	add_timer(&my_switch_timer);
}
