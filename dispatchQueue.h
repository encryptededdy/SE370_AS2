/* 
 * File:   dispatchQueue.h
 * Author: robert
 *
 * Modified by: ezha210
 */

#ifndef DISPATCHQUEUE_H
#define	DISPATCHQUEUE_H

#include <pthread.h>
#include <semaphore.h>
    
#define error_exit(MESSAGE)     perror(MESSAGE), exit(EXIT_FAILURE)

    typedef enum { // whether dispatching a task synchronously or asynchronously
        ASYNC, SYNC
    } task_dispatch_type_t;
    
    typedef enum { // The type of dispatch queue.
        CONCURRENT, SERIAL
    } queue_type_t;

    typedef struct task {
        char name[64];              // to identify it when debugging
        void (*work)(void *);       // the function to perform
        void *params;               // parameters to pass to the function
        task_dispatch_type_t type;  // asynchronous or synchronous
        sem_t task_complete_semaphore_sync; // notifies when task is complete (for sync)
        sem_t * task_complete_semaphore_wait; // notifies when task is complete (for queue wait)
    } task_t;

    typedef struct linked_task_node {
        struct task * task;
        struct linked_task_node * next;
    } node_t;

    typedef struct linked_task_semaphore_node {
        sem_t task_complete_semaphore_wait;
        struct linked_task_semaphore_node * next;
    } node_ts;

    typedef struct dispatch_queue_t dispatch_queue_t; // the dispatch queue type
    typedef struct dispatch_queue_thread_t dispatch_queue_thread_t; // the dispatch queue thread type

    struct dispatch_queue_thread_t {
        dispatch_queue_t *queue;// the queue this thread is associated with
        pthread_t thread;       // the thread which runs the task
    };

    struct dispatch_queue_t {
        queue_type_t queue_type;            // the type of queue - serial or concurrent
        node_t* tasks_linked_list;
        node_ts* tasks_sem_linked_list;
        dispatch_queue_thread_t* threads;
        sem_t queue_count_semaphore;
    };
    
    task_t *task_create(void (*)(void *), void *, char*);
    
    void task_destroy(task_t *);

    dispatch_queue_t *dispatch_queue_create(queue_type_t);
    
    void dispatch_queue_destroy(dispatch_queue_t *);
    
    void dispatch_async(dispatch_queue_t *, task_t *);
    
    void dispatch_sync(dispatch_queue_t *, task_t *);
    
    void dispatch_for(dispatch_queue_t *, long, void (*)(long));
    
    void dispatch_queue_wait(dispatch_queue_t *);

#endif	/* DISPATCHQUEUE_H */