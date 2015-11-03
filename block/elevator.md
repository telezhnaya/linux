### `elevator.c`
Elevator is an I/O scheduler that helps to make hard disk requests fast. Best performance is reached by operations with requests called merging and sorting.

Each request is very expensive for the system so the main idea is to minimize their number. So, let's explain the main idea of an algorithm. 

We have an queue of requests to one specific disk and we have one fresh request to that disk. At first, we must check whether it is possible to merge it to one of old requests (it must follow or precede in physical memory). If it is possible - great! We can merge it and there is no need to do it separately. If not - put it to the end of an queue.

### Elements of realization
In fact, queue is a RB tree. Main functions:

`int elevator_init(struct request_queue *q, char *name)` initializes queue (returns code of error)

`void elevator_exit(struct elevator_queue *e)` ends all processes and clean memory

`void elv_add_request(struct request_queue *q, struct request *rq, int where)`

`void elv_merge_requests(struct request_queue *, struct request *, struct request *)`

`int elevator_change(struct request_queue *q, const char *name)` helps to switch to another scheduler

### Main structures:
1. [request_queue](http://lxr.free-electrons.com/source/include/linux/blkdev.h#L286 "request_queue") is used for storing all requests to one disk
2. [request](http://lxr.free-electrons.com/source/include/linux/blkdev.h#L87 "request") contains a pointer to request_queue and bio
3. [bio](http://lxr.free-electrons.com/source/include/linux/blk_types.h#L46 "bio") represents block I/O operations that are in flight (active) as a list of segments
4. [elevator_type](http://lxr.free-electrons.com/source/include/linux/elevator.h#L86 "elevator_type") contains a set of functions and fields which helps to make your own scheduler (this is like an interface of schedulers)
