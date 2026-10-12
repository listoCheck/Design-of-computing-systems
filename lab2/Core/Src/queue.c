#include "queue.h"
void queue_init(ByteQueue *q, uint8_t *storage, size_t capacity)
{ *q = (ByteQueue){.data = storage, .capacity = capacity}; }
bool queue_push(ByteQueue *q, uint8_t byte)
{
    if (q->count == q->capacity) return false;
    q->data[q->head] = byte;
    q->head = (q->head + 1U) % q->capacity;
    ++q->count;
    return true;
}
bool queue_pop(ByteQueue *q, uint8_t *byte)
{
    if (!queue_peek(q, byte)) return false;
    q->tail = (q->tail + 1U) % q->capacity;
    --q->count;
    return true;
}
bool queue_peek(const ByteQueue *q, uint8_t *byte)
{
    if (!q->count) return false;
    *byte = q->data[q->tail];
    return true;
}
