//
// Created by ezha210 on 5/09/2018.
//

#include <dispatchQueue.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

dispatch_queue_t *dispatch_queue_create(queue_type_t queueType) {
    dispatch_queue_t *queue;
    queue = (dispatch_queue_t *) malloc(sizeof(dispatch_queue_t)); // allocate memory (otherwise we escape local scope)
    queue->queue_type = queueType;
    return queue;
}

task_t *task_create(void (*job)(void *), void *parameters, char *name) {
    task_t *task;
    task = (task_t *) malloc(sizeof(task_t));
    strncpy(task->name, name, 64); // max length 64 chars so we can just copy it all
    task->params = parameters;
    // task->type is set when adding to queue
    task->work = job;
    return task;
}