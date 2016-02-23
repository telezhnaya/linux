#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/slab.h>
#include <linux/genhd.h>

#include "meta-iosched.h"

struct queue_arr {
	struct request_queue **queues;
	int n_queues;
	int size;
};

static struct queue_arr all;

struct kobject *group_kobj;

static int interval;
static int ratio;
static int low_prio; // need these variables only for making kobj_attributes with good names
static int high_prio;

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

static ssize_t names_show(struct kobject *kobj, struct kobj_attribute *attr,
                        char *buf)
{
	int i;
	int buf_ptr = 0;
	/*for (i = 0; i < all.n_queues; i++)
		if (all.queues[i]->last_merge) {
			buf_ptr += sprintf(buf + buf_ptr, "%s\n",
				all.queues[i]->last_merge->rq_disk->disk_name);
			printk("success, %s", buf);
		}
	printk("names_show %s\n", buf);*/
	buf_ptr += sprintf(buf + buf_ptr, "%s\n", "hello!");
        return buf_ptr;
}

static ssize_t names_store(struct kobject *kobj, struct kobj_attribute *attr,
                         const char *buf, size_t count)
{
        printk("names_store trying to add %s\n", buf);
        return count;
}


static struct kobj_attribute interval_attribute =
	__ATTR(interval, 0664, interval_show, interval_store);

static struct kobj_attribute ratio_attribute =
	__ATTR(ratio, 0664, ratio_show, ratio_store);

static struct kobj_attribute low_prio_attribute =
	__ATTR(low_prio, 0664, names_show, names_store);

static struct kobj_attribute high_prio_attribute =
	__ATTR(high_prio, 0664, names_show, names_store);

static struct attribute *attrs[] = {
	&interval_attribute.attr,
	&ratio_attribute.attr,
	&low_prio_attribute.attr,
	&high_prio_attribute.attr,
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

void print_stats(void) {
	int i;
	printk("Stats! ");
	for (i = 0; i < all.n_queues; i++) {
		int t = all.queues[i]->stats[0] + all.queues[i]->stats[1] + all.queues[i]->stats[2];
		printk("id=%d: load %d or %d%%, ", all.queues[i]->id, t, t * 100 / whole_stat);
	}
	printk("whole = %d\n", whole_stat);
	printk("stats interval=%d, ratio=%d\n", interval, ratio);
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

void clean_up(int level) {
	int i;
	for (i = 0; i < all.n_queues; i++) {
		whole_stat -= all.queues[i]->stats[level];
		all.queues[i]->stats[level] = 0;
	}
}

void update_stats(struct request_queue *q) {
	whole_stat++;
	q->stats[group]++;
	if (q->stats[group] > 1000) {
		group = (group + 1) % 3;
		clean_up(group);
	}
}
