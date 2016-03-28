#ifndef _META_IOSCHED_H
#define _META_IOSCHED_H

void add_my_disk(struct gendisk* disk);
int kobj_init(void);

bool add_queue(struct request_queue *q);
bool del_queue(struct request_queue *q);

int calc_time_to_sleep(struct request_queue *q);

void update_stats(struct request_queue *q);

void init_my_timer(void);
#endif
