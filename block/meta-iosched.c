#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/slab.h>

#include "meta-iosched.h"

struct queue_arr {
	struct request_queue **queues;
	int n_queues;
	int size;
};

struct queue_arr all = {0};
struct queue_arr major = {0};

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

bool del_queue2(struct queue_arr arr, struct request_queue *q) {
	int pos;
	pos = find_queue(arr, q);
	if (pos == -1) return false;
	arr.queues[pos] = arr.queues[arr.n_queues - 1];
	arr.n_queues--;
	return true;
}

bool del_queue(struct request_queue *q) {
	del_queue2(major, q);
	return del_queue2(all, q);
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
}

// This function returns 0 if queue must be dispatched now
// Otherwise it calculates amount of time to sleep
int calc_time_to_sleep(struct request_queue *q) {
	int i;
	if (find_queue(major, q) != -1) return 0; // This queue has priority
	for (i = 0; i < major.n_queues; i++) {
		if (major.queues[i]->nr_pending > 0)
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
