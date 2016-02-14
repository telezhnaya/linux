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
	struct request_queue **temp;

	if (all.size > all.n_queues) {
		all.queues[all.n_queues++] = q;
		return true;
	}

	temp = kmalloc((all.n_queues + 1) * 2 * sizeof(q), GFP_KERNEL);
	if (!temp) return false;
	all.size = (all.n_queues + 1) * 2;
	memcpy(temp, all.queues, all.n_queues * sizeof(q));
	kfree(all.queues);
	all.queues = temp;
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

// Here must be smth smart :-)
void print_stats(void) {
	int i;
	printk("Stats! ");
	for (i = 0; i < all.n_queues; i++) {
		printk("id=%d, ", all.queues[i]->id);
	}
	printk("thats all.\n");
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
