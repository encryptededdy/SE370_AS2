//
// Created by ezha210 on 5/09/2018.
//

#include <dispatchQueue.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Goes to the end of a linkedlist (used for inserts)
node_t* endof_linkedlist_tasks(node_t *head) {
    while (head->next != NULL) {
        head = head->next;
    }
    return head;
}

// As above, but for the semaphore linkedlist
node_ts* endof_linkedlist_tasksem(node_ts *head) {
    while (head->next != NULL) {
        head = head->next;
    }
    return head;
}

task_t* pop_head(dispatch_queue_t * queue) {
    node_t * currentNode = queue->tasks_linked_list;
    // technically may be undefined, but doesn't matter because semaphore limits the amount of times we call this
    node_t * nextNode = currentNode->next;
    task_t * task = currentNode->task;
    queue->tasks_linked_list = nextNode;
    free(currentNode);
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
        sem_post(&task->task_complete_semaphore_sync);
        sem_post(task->task_complete_semaphore_wait);
        // Destroy task
        task_destroy(task);
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
    }
    queue->tasks_linked_list = NULL;
    queue->tasks_sem_linked_list = NULL;
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
    sem_init(&task->task_complete_semaphore_sync, 0, 0); // setup task complete sems
    // The other semaphore is inited when we add this task to a queue
    return task;
}

void task_destroy(task_t * task) {
    // Destroy a task and everything inside.
    sem_destroy(&task->task_complete_semaphore_sync);
    // Don't destroy the other semaphore as it's technically part of the queue
    free(task);
}

void dispatch_sync(dispatch_queue_t * queue, task_t * task) {
    dispatch_async(queue, task);
    // Since we're SYNC, wait for the task to complete before we return
    sem_wait(&task->task_complete_semaphore_sync);
}

void dispatch_async(dispatch_queue_t * queue, task_t * task) {
    task->type = ASYNC;
    // Create the node that we'll add to the linked list
    node_t *node;
    node = (node_t *) malloc(sizeof(node_t));
    node->task = task;
    node->next = NULL;
    if (queue->tasks_linked_list == NULL) {
        // Add new linked task node
        queue->tasks_linked_list = node;
    } else {
        // Insert at end of LinkedList!
        node_t *end = endof_linkedlist_tasks(queue->tasks_linked_list);
        end->next = node;
    }
    // Generate completion semaphore, add to linkedlist
    node_ts *semnode;
    semnode = (node_ts *) malloc(sizeof(node_ts));
    sem_init(&semnode->task_complete_semaphore_wait, 0, 0);
    task->task_complete_semaphore_wait = &semnode->task_complete_semaphore_wait;
    semnode->next = NULL;
    // Add to linkedlist
    if (queue->tasks_sem_linked_list == NULL) {
        queue->tasks_sem_linked_list = semnode;
    } else {
        node_ts* end = endof_linkedlist_tasksem(queue->tasks_sem_linked_list);
        end->next = semnode;
    }

    // Increment semaphore to get a thread to pick it up
    sem_post(&queue->queue_count_semaphore);
}

void perform_wait(node_ts* head) {
    if (head->next != NULL) {
        perform_wait(head->next);
    }
    sem_wait(&head->task_complete_semaphore_wait);
    sem_destroy(&head->task_complete_semaphore_wait);
    free(head);
}

void dispatch_queue_wait(dispatch_queue_t * queue) {
    node_ts* head = queue->tasks_sem_linked_list;
    perform_wait(head);
}

void dispatch_queue_destroy(dispatch_queue_t * queue) {
    sem_destroy(&queue->queue_count_semaphore);
    free(queue);
}

