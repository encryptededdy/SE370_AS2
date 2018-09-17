//
// Created by ezha210 on 5/09/2018.
//

#include <dispatchQueue.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

pthread_mutex_t lock;

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
    free(currentNode); // free this!
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
        pthread_mutex_lock(&lock); // lock before accessing linkedlist
        task_t * task = pop_head(thread->queue);
        pthread_mutex_unlock(&lock); // unlock when we're done
        task->work(task->params); // run the task!
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
    queue = malloc(sizeof(dispatch_queue_t)); // allocate memory (otherwise we escape local scope)
    queue->queue_type = queueType; // store queue type
    sem_init(&queue->queue_count_semaphore, 0, 0); // init semaphore
    dispatch_queue_thread_t *threads;

    // thread array is size 1 if serial, else is equal to numcores
    int numCores;
    if (queueType == CONCURRENT) {
        numCores = getNumCores();
    } else {
        numCores = 1;
    }

    // store thread array
    threads = malloc(sizeof(dispatch_queue_thread_t) * numCores);
    // populate thread array
    for (int i = 0; i < numCores; i++) {
        threads[i].queue = queue;
        pthread_create(&threads[i].thread, NULL, thread_helper, &threads[i]);
    }
    queue->tasks_linked_list = NULL;
    queue->tasks_sem_linked_list = NULL;
    queue->threads = threads;
    return queue;
}

task_t *task_create(void (*job)(void *), void *parameters, char *name) {
    task_t *task;
    task = malloc(sizeof(task_t));
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
    // Don't destroy the other semaphore as it's part of the queue, and is used for dispatch_queue_wait
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
    node = malloc(sizeof(node_t));
    node->task = task;
    node->next = NULL;
    pthread_mutex_lock(&lock); // lock before accessing linkedlist
    if (queue->tasks_linked_list == NULL) {
        // Add new linked task node
        queue->tasks_linked_list = node;
    } else {
        // Insert at end of LinkedList!
        node_t *end = endof_linkedlist_tasks(queue->tasks_linked_list);
        end->next = node;
    }
    pthread_mutex_unlock(&lock); // unlock after accessing linkedlist
    // Generate completion semaphore, add to linkedlist
    node_ts *semnode;
    semnode = malloc(sizeof(node_ts));
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

void dispatch_for(dispatch_queue_t * queue, long count, void (*job)(long)) {
    // Setup semarray to store the semaphores we need to use to wait
    sem_t **semarray;
    semarray = malloc(sizeof(sem_t*) * count);

    for (long i = 0; i < count; i++) {
        task_t *task;
        task = task_create((void (*)(void *)) job, (void *) i, "ForDispatchTask");
        dispatch_async(queue, task);
        semarray[i] = &task->task_complete_semaphore_sync; // get semaphore
    }

    // now all tasks are dispatched, we just need to wait for all the semaphores
    for (long i = 0; i < count; i++) {
        sem_wait(semarray[i]);
    }

    // Free the memory we allocated to the array
    free(semarray);

}

// Recursively waits for all tasks to finish. destroyOnly means we don't wait, just dismantle the linkedlist.
void perform_wait(node_ts* head, int destroyOnly) {
    if (head->next != NULL) {
        perform_wait(head->next, destroyOnly);
    }
    // Wait for sem to be ready
    if (destroyOnly == 0) {
        sem_wait(&head->task_complete_semaphore_wait);
    }
    // Free everything before returning
    sem_destroy(&head->task_complete_semaphore_wait);
    free(head);
}

void dispatch_queue_wait(dispatch_queue_t * queue) {
    node_ts* head = queue->tasks_sem_linked_list;
    perform_wait(head, 0);
    // Set to NULL so we won't try to clear it later
    queue->tasks_sem_linked_list = NULL;
}

void dispatch_queue_destroy(dispatch_queue_t * queue) {
    sem_destroy(&queue->queue_count_semaphore);
    // Destroy threads
    if (queue->queue_type == CONCURRENT) {
        // kill entire thread array
        for (int i = 0; i < getNumCores(); i++) {
            queue->threads[i].queue = queue;
            pthread_cancel(queue->threads[i].thread);
        }

    } else {
        queue->threads->queue = queue;
        pthread_cancel(queue->threads->thread);
    }
    free(queue->threads);
    // Tasks Linked Lists dismantles itself in pop_head so we don't need to free anything

    // destroy the dispatch_queue_wait if we didn't use it
    if (queue->tasks_sem_linked_list != NULL) {
        perform_wait(queue->tasks_sem_linked_list, 1);
    }
    free(queue);
}

