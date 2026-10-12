#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdbool.h>
bool scheduler_init(void);
bool scheduler_start(void);
bool scheduler_take(void);
void scheduler_irq(void);
#endif
