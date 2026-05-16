/* queue.c — Offline queue: ring buffer of envelopes.
 * Push appends; pop drains from oldest; peek(i) reads i-th oldest.
 * On overflow push returns TA2A_ERR_BUFFER_FULL (caller decides drop policy). */
#include "tinya2a.h"
#include <stdlib.h>
#include <string.h>

struct ta2a_queue {
    size_t           cap;
    size_t           count;
    size_t           head;       /* next pop index */
    size_t           tail;       /* next push index */
    ta2a_envelope_t *buf;
};

ta2a_queue_t *ta2a_queue_create(size_t capacity)
{
    if (capacity == 0) return NULL;
    ta2a_queue_t *q = (ta2a_queue_t *)calloc(1, sizeof(*q));
    if (!q) return NULL;
    q->buf = (ta2a_envelope_t *)calloc(capacity, sizeof(ta2a_envelope_t));
    if (!q->buf) { free(q); return NULL; }
    q->cap = capacity;
    return q;
}

void ta2a_queue_destroy(ta2a_queue_t *q)
{
    if (!q) return;
    free(q->buf);
    free(q);
}

ta2a_result_t ta2a_queue_push(ta2a_queue_t *q, const ta2a_envelope_t *env)
{
    if (!q || !env) return TA2A_ERR_INVALID_ARG;
    if (q->count == q->cap) return TA2A_ERR_BUFFER_FULL;
    q->buf[q->tail] = *env;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;
    return TA2A_OK;
}

ta2a_result_t ta2a_queue_pop(ta2a_queue_t *q, ta2a_envelope_t *out)
{
    if (!q || !out) return TA2A_ERR_INVALID_ARG;
    if (q->count == 0) return TA2A_ERR_NOT_FOUND;
    *out = q->buf[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;
    return TA2A_OK;
}

ta2a_result_t ta2a_queue_peek(const ta2a_queue_t *q, size_t i, ta2a_envelope_t *out)
{
    if (!q || !out) return TA2A_ERR_INVALID_ARG;
    if (i >= q->count) return TA2A_ERR_NOT_FOUND;
    size_t idx = (q->head + i) % q->cap;
    *out = q->buf[idx];
    return TA2A_OK;
}

size_t ta2a_queue_size(const ta2a_queue_t *q)     { return q ? q->count : 0; }
size_t ta2a_queue_capacity(const ta2a_queue_t *q) { return q ? q->cap   : 0; }

void ta2a_queue_clear(ta2a_queue_t *q)
{
    if (!q) return;
    q->count = q->head = q->tail = 0;
}
