#ifndef QUEUE_H
#define QUEUE_H

typedef struct node_queue_t{
    void *data;
    struct node_queue_t *next;
    struct node_queue_t *prev;
} node_queue_t;

typedef struct{
    node_queue_t *front;
    node_queue_t *rear;
} queue_t;

/**
 * Creates a new queue and returns a pointer to it.
 */
queue_t *new_queue(void);

/**
 * Adds an element to the end of the queue.
 */
void enqueue(queue_t *q, void *data);

/**
 * Removes and returns the element at the front of the queue.
 * Returns NULL if the queue is empty.
 */
void *dequeue(queue_t *q);

/**
 * Creates a deep copy of the queue, using _copy_data to copy each element.
 */
queue_t *copy_queue(queue_t *q, void *(*_copy_data)(void *));

/**
 * Tells if queue is empty
 */
int queue_is_empty(queue_t *q);

/**
 * Function which remove a concrete node
 */
void queue_remove(queue_t *q, node_queue_t *node);

#endif