#include "queue.h"

void queue_init(Queue* queue)
{
	queue->size = 0;
	queue->head = NULL;
	queue->tail = NULL;
}

void queue_enqueue(Queue* q, QueueNode* node)
{
	node->next = NULL;

	if (q->size == 0)
	{
		q->head = q->tail = node;
	}
	else
	{
		q->tail->next = node;
		q->tail = node;
	}

	++q->size;
}

QueueNode* queue_dequeue(Queue* q)
{
	if (q->size == 0)
	{
		return NULL;
	}

	QueueNode* returnVal = q->head;
	q->head = q->head->next;

	--q->size;

	return returnVal;
}

uint8_t queue_empty(const Queue* q)
{
	return q->size == 0;
}

uint64_t queue_size(const Queue* q)
{
	return q->size;
}
