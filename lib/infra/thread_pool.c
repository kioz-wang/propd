/**
 * @file thread_pool.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-03
 *
 * @copyright MIT License
 *
 *  Copyright (c) 2025 kioz.wang
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include "thread_pool.h"
#include "logger/logger.h"
#include "timestamp.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

struct task {
    int (*routine)(void *);
    void          *arg;
    timestamp_t    created;
    int           *result;
    sem_t         *done;
    unsigned short _id;
};
typedef struct task task_t;

struct task_queue {
    unsigned short  num;
    task_t         *queue;
    unsigned short  head, tail, count;
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
};
typedef struct task_queue task_queue_t;

static int task_queue_init(task_queue_t *queue, unsigned short num) {
    task_t *_queue = (task_t *)calloc(num, sizeof(task_t));
    if (!_queue) return errno;

    queue->num   = num;
    queue->queue = _queue;
    queue->head = queue->tail = queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
    return 0;
}

static void task_queue_deinit(task_queue_t *queue) {
    if (queue->num) {
        pthread_mutex_destroy(&queue->mutex);    /* TODO EBUSY */
        pthread_cond_destroy(&queue->not_empty); /* TODO EBUSY */
        pthread_cond_destroy(&queue->not_full);  /* TODO EBUSY */
        queue->num = 0;
        free(queue->queue);
    }
}

static void task_queue_push(task_queue_t *queue, task_t task) {
    pthread_mutex_lock(&queue->mutex);

    while (queue->count >= queue->num) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    task._id                  = queue->tail;
    queue->queue[queue->tail] = task;
    queue->tail               = (queue->tail + 1) % queue->num;
    queue->count++;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    logfD("[thread_pool] task%d@%lx ready", task._id, task.created);
}

static task_t task_queue_pop(task_queue_t *queue) {
    pthread_mutex_lock(&queue->mutex);

    while (queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    task_t task = queue->queue[queue->head];
    queue->head = (queue->head + 1) % queue->num;
    queue->count--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    return task;
}

static void *thread_pool_worker(task_queue_t *queue) {
    while (true) {
        task_t task = task_queue_pop(queue);
        logfD("[thread_pool] task%d@%lx running", task._id, task.created);

        int result = task.routine(task.arg);
        logfD("[thread_pool] task%d@%lx done with result %d", task._id, task.created, result);

        if (task.result) {
            *task.result = result;
            sem_post(task.done);
        }
    }
    return NULL;
}

struct thread_pool {
    unsigned short num;
    void          *pool;
    void          *task_queue;
};
typedef struct thread_pool thread_pool_t;

void *thread_pool_create(unsigned short thread_num, unsigned short min_if_auto, unsigned short max_if_auto,
                         unsigned short task_num) {
    int ret = 0;

    if (thread_num == 0) {
        long           _ncpu = sysconf(_SC_NPROCESSORS_ONLN);
        unsigned short ncpu  = _ncpu < 0 ? 0 : (unsigned short)_ncpu;

        thread_num = ncpu < min_if_auto ? min_if_auto : (max_if_auto < ncpu ? max_if_auto : ncpu);
        logfV("[thread_pool] select %d as thread_num automatically", thread_num);
    }
    if (task_num == 0) {
        task_num = thread_num;
        logfV("[thread_pool] select %d as task_num automatically", task_num);
    }

    thread_pool_t *tpool = (thread_pool_t *)calloc(1, sizeof(thread_pool_t));
    if (!tpool) goto exit;
    tpool->pool = (pthread_t *)calloc(thread_num, sizeof(pthread_t));
    if (!tpool->pool) goto exit;
    tpool->task_queue = (task_queue_t *)calloc(1, sizeof(task_queue_t));
    if (!tpool->task_queue) {
        goto exit;
    }
    ret = task_queue_init(tpool->task_queue, task_num);
    if (ret) goto exit;

    for (int i = 0; i < thread_num; i++) {
        ret = pthread_create(&((pthread_t *)tpool->pool)[i], NULL, (void *(*)(void *))thread_pool_worker,
                             tpool->task_queue);
        if (ret) {
            tpool->num = i;
            errno      = ret;
            logfE("[thread_pool] fail to create thread[%d] (%d)", i, ret);
            goto exit;
        }
    }
    tpool->num = thread_num;

    logfI("[thread_pool] created %d threads and a task_queue with depth %d", thread_num, task_num);
    return tpool;

exit:
    thread_pool_destroy(tpool);
    return NULL;
}

void thread_pool_destroy(void *_tpool) {
    thread_pool_t *tpool = (thread_pool_t *)_tpool;
    if (!tpool) return;

    for (int i = 0; i < tpool->num; i++) {
        pthread_cancel(((pthread_t *)tpool->pool)[i]);
    }
    tpool->num = 0;

    if (tpool->task_queue) {
        task_queue_deinit(tpool->task_queue);
        free(tpool->task_queue);
    }

    free(tpool->pool);

    free(tpool);
    logfI("[thread_pool] destroyed");
}

int thread_pool_submit(void *tpool, int (*routine)(void *), void *arg, bool sync) {
    assert(tpool);
    assert(routine);
    int    result = 0;
    sem_t  done;
    task_t task = {
        .routine = routine,
        .arg     = arg,
        .created = timestamp(true),
    };

    if (sync) {
        sem_init(&done, 0, 0);
        task.result = &result;
        task.done   = &done;
    }
    task_queue_push(((thread_pool_t *)tpool)->task_queue, task);
    if (sync) {
        sem_wait(&done);
        sem_destroy(&done);
        return result;
    }
    return 0;
}
