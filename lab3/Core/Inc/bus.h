#ifndef BUS_H
#define BUS_H
#include <stdbool.h>
#include <stdint.h>
/* Foreground only: serial blocking transactions, bounded HAL timeouts. */
bool bus_init(void);
bool bus_read(uint8_t address, uint8_t reg, uint8_t *data, uint16_t length);
bool bus_write(uint8_t address, uint8_t reg, const uint8_t *data, uint16_t length);
#endif
