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

#endif
