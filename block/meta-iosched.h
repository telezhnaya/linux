#ifndef _META_IOSCHED_H
#define _META_IOSCHED_H

bool add_queue(struct request_queue *q);
bool del_queue(struct request_queue *q);

void print_stats(void);

int calc_time_to_sleep(struct request_queue *q);

static inline void update_stats(struct request_queue *q) {
	//q->abc++;
	//need to fill it with good stats
}

#endif
