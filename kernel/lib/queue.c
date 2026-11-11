#include "queue.h"
#include "lib.h"
#include "../mm/pmm.h"

queue_t *new_queue(void) {
    queue_t *q = (queue_t *)vmalloc(sizeof(queue_t));
    q->front = NULL;
    q->rear = NULL;
    return q;
}

void enqueue(queue_t *q, void *data) {
    node_queue_t *new_node = (node_queue_t *)vmalloc(sizeof(node_queue_t));
    new_node->data = data;
    new_node->next = NULL;
    new_node->prev = NULL;

    if (q->rear == NULL) {
        q->front = new_node;
        q->rear = new_node;
    } else {
        new_node->prev = q->rear;
        q->rear->next = new_node;
        q->rear = new_node;
    }
}

void *dequeue(queue_t *q) {
    if (q->front == NULL) {
        return NULL;
    }

    node_queue_t *node = q->front;
    void *data = node->data;
    q->front = q->front->next;

    if (q->front == NULL) {
        q->rear = NULL;
    }
    else{
        q->front->prev=NULL;
    }

    //vfree(node); TODO: To implemented
    return data;
}

int queue_is_empty(queue_t *q){
    return q->front == NULL;
}

void queue_remove(queue_t *q, node_queue_t *node){
    if(node != NULL){
        if(node->prev != NULL){
            node->prev->next = node->next;
        }
        if(node->next != NULL){
            node->next->prev = node->prev;
        }
        if(q->front == node){
            q->front = node->next;
        }
        if(q->rear == node){
            q->rear = node->prev;
        }
        //vfree(node) TODO: To implement
    }
}