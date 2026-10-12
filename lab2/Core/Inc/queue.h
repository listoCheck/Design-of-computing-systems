#ifndef QUEUE_H
#define QUEUE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Generic byte FIFO. Caller synchronizes main/ISR access. All capacity is usable. */
typedef struct { uint8_t *data; size_t capacity, head, tail, count; } ByteQueue;
void queue_init(ByteQueue *q, uint8_t *storage, size_t capacity);
bool queue_push(ByteQueue *q, uint8_t byte);
bool queue_pop(ByteQueue *q, uint8_t *byte);
bool queue_peek(const ByteQueue *q, uint8_t *byte);
#endif
