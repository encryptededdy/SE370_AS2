//
// Created by ezha210 on 5/09/2018.
//

#include <dispatchQueue.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Goes to the end of a linkedlist (used for inserts)
node_t* endof_linkedlist(node_t * head) {
    while (head->next != NULL) {
        head = head->next;
    }
    return head;
}

task_t* pop_head(dispatch_queue_t * queue) {
    node_t * nextNode = queue->tasks_linked_list->next;
    task_t * task = queue->tasks_linked_list->task;
    queue->tasks_linked_list = nextNode;
    return task;
}

int getNumCores() {
    return (int) sysconf(_SC_NPROCESSORS_CONF);
}

// Ignore warnings for infinite loop
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
void* thread_helper(void * arg) {
    dispatch_queue_thread_t * thread = (dispatch_queue_thread_t *) arg;
    while (1) {
        sem_wait(&thread->queue->queue_count_semaphore);
        // run task
        task_t * task = pop_head(thread->queue);
        task->work(task->params);
        // notify semaphore that we're done
        sem_post(&task->task_complete_semaphore);
    }
}
#pragma clang diagnostic pop

dispatch_queue_t *dispatch_queue_create(queue_type_t queueType) {
    dispatch_queue_t *queue;
    // setup queue struct
    queue = (dispatch_queue_t *) malloc(sizeof(dispatch_queue_t)); // allocate memory (otherwise we escape local scope)
    queue->queue_type = queueType; // store queue type
    sem_init(&queue->queue_count_semaphore, 0, 0); // init semaphore
    dispatch_queue_thread_t *threads;
    // store thread array
    if (queueType == CONCURRENT) {
        threads = (dispatch_queue_thread_t *) malloc(sizeof(dispatch_queue_thread_t) * getNumCores());
    } else {
        threads = (dispatch_queue_thread_t *) malloc(sizeof(dispatch_queue_thread_t));
    }
    // populate thread array
    for (int i = 0; i < getNumCores(); i++) {
        threads[i].queue = queue;
        pthread_create(&threads[i].thread, NULL, thread_helper, &threads[i]);
        // Thread Semaphore not used... I think...
        // Thread task not allocated yet
    }

    queue->threads = threads;
    return queue;
}

task_t *task_create(void (*job)(void *), void *parameters, char *name) {
    task_t *task;
    task = (task_t *) malloc(sizeof(task_t));
    strncpy(task->name, name, 64); // max length 64 chars so we can just copy it all
    task->params = parameters;
    // task->type is set when adding to queue
    task->work = job;
    sem_init(&task->task_complete_semaphore, 0, 0); // setup task complete sem
    return task;
}

int dispatch_sync(dispatch_queue_t * queue, task_t * task) {
    task->type = SYNC;
    // Create the node that we'll add to the linked list
    node_t *node;
    node = (node_t *) malloc(sizeof(node_t));
    node->task = task;
    if (queue->tasks_linked_list == NULL) {
        // Add new linked task node
        queue->tasks_linked_list = node;
    } else {
        // Insert at end of LinkedList!
        node_t *end = endof_linkedlist(queue->tasks_linked_list);
        end->next = node;
    }
    // TODO: How to sync?
}

void dispatch_queue_destroy(dispatch_queue_t * queue) {
    free(queue);
}

