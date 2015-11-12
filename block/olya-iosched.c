/*
 * elevator olya
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

struct noop_data {
	struct list_head queue;
};

static void noop_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

static int noop_dispatch(struct request_queue *q, int force)
{
	struct noop_data *nd = q->elevator->elevator_data;

	if (!list_empty(&nd->queue)) {
		struct request *rq;
		rq = list_entry(nd->queue.next, struct request, queuelist);
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		return 1;
	}
	return 0;
}

int const buffer_size = 20;
int statistics[20];
bool first_add = true;

static void noop_add_request(struct request_queue *q, struct request *rq)
{
	int id = q->id;
	if (first_add)
	{
		memset(statistics, 0, sizeof(int)*buffer_size);
		first_add = false;
	}
	if (id >= buffer_size)
		printk("PANIC, id=%d", id);
	else
	{
		statistics[id]++;
		if (statistics[id] % 1000 == 1)
			printk("request!, disk_name=%s, queue_id=%d, count=%d\n", 
				rq->rq_disk->disk_name, id, statistics[id]);
	}

	struct noop_data *nd = q->elevator->elevator_data;
	list_add_tail(&rq->queuelist, &nd->queue);
}

static struct request *
noop_former_request(struct request_queue *q, struct request *rq)
{
	struct noop_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
noop_latter_request(struct request_queue *q, struct request *rq)
{
	struct noop_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

bool first_init = true;
struct elevator_queue *eq;

static int noop_init_queue(struct request_queue *q, struct elevator_type *e)
{
	if (first_init)
	{
		struct noop_data *nd;
		//first_init = false; // does not work with it
		eq = elevator_alloc(q, e);
		if (!eq)
			return -ENOMEM;

		nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
		if (!nd) {
			kobject_put(&eq->kobj);
			return -ENOMEM;
		}
		eq->elevator_data = nd;

		INIT_LIST_HEAD(&nd->queue);
	}

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);

	printk("hello %s, id=%d, node=%d\n", e->elevator_name, q->id, q->node);
	return 0;
}

static void noop_exit_queue(struct elevator_queue *e)
{
	struct noop_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_noop = {
	.ops = {
		.elevator_merge_req_fn		= noop_merged_requests,
		.elevator_dispatch_fn		= noop_dispatch,
		.elevator_add_req_fn		= noop_add_request,
		.elevator_former_req_fn		= noop_former_request,
		.elevator_latter_req_fn		= noop_latter_request,
		.elevator_init_fn		= noop_init_queue,
		.elevator_exit_fn		= noop_exit_queue,
	},
	.elevator_name = "olya",
	.elevator_owner = THIS_MODULE,
};

static int __init noop_init(void)
{
	return elv_register(&elevator_noop);
}

static void __exit noop_exit(void)
{
	elv_unregister(&elevator_noop);
}

module_init(noop_init);
module_exit(noop_exit);


MODULE_AUTHOR("Jens Axboe");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Olya IO scheduler");
