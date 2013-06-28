#ifndef __DATA_STRUCTURES_QUEUE_H__
#define __DATA_STRUCTURES_QUEUE_H__

#include "inttypes.h"

/* Stores the data of the Queue and points to the next node
 */
typedef struct _QueueNode
{
	struct _QueueNode* next;
	void* data;
} QueueNode;

/* The Queue object itself. Has size, head, and tail.
 */
typedef struct
{
	uint64_t size;
	QueueNode* head;
	QueueNode* tail;
} Queue;

/* Initialize a Queue object. Sets size to 0 and the head and
 * tail pointers to NULL.
 *
 * Parameters:
 *    queue - The Queue object to initialize
 */
void queue_init(Queue* queue);

/* Put an item on the end of the queue. Also increments the size.
 *
 * Parameters:
 *    queue - The Queue to add the node to
 *    node - The node to add to the Queue
 */
void queue_enqueue(Queue* queue, QueueNode* node);

/* Add to the queue, but use a comparison function. This allows the queue
 * to be a priority queue.
 *
 * Parameters:
 *    queue - The Queue to add the node to
 *    node - The node to add
 *    cmp - The comparison function, this function will be given the passed
 *          in queue node's data and the node to compare. This function should
 *          return -1 if d1 < d2, 0 if d1 == d2, and 1 if d1 > d2
 */
void queue_enqueue_prio(Queue* queue, QueueNode* node, 
		int8_t cmp(const void* d1, const void* d2));

/* Take an item off the front of the queue. Returns NULL if there
 * are no more items in the queue.
 *
 * Parameters:
 *    queue - The Queue object to take an element from
 *
 * Returns:
 *    A QueueNode object that was the front of the queue, or NULL
 *    if the Queue object is empty
 */
QueueNode* queue_dequeue(Queue* queue);

/* Check if the queue is empty.
 *
 * Parameters:
 *    queue - The Queue object to check
 *
 * Returns:
 *    1 if the Queue object is empty, 0 otherwise
 */
uint8_t queue_empty(const Queue* queue);

/* Get the size of the Queue
 *
 * Parameters:
 *    queue - The queue object to find the size of
 *
 * Returns:
 *    The size of the Queue
 */
uint64_t queue_size(const Queue* queue);

static inline
QueueNode* queue_peek(Queue* queue)
{
	if (queue->size == 0)
	{
		return NULL;
	}

	return queue->head;
}

#endif
