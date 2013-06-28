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

void queue_enqueue_prio(Queue* queue, QueueNode* node, 
		int8_t cmp(const void* d1, const void* d2))
{
	if (queue->size == 0)
	{
		queue_enqueue(queue, node);
	}
	else
	{
		const void* node_data = node->data;
		QueueNode* other = queue->head;
		while (other != NULL && cmp(node_data, other->data) > 0)
		{
			other = other->next;
		}

		if (other == NULL)
		{
			// We fell off the list, so node should be the last element
			node->next = NULL;
			queue->tail->next = node;
			queue->tail = node;
		}
		else
		{
			node->next = other->next;
			other->next = node;
		}
	}
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
