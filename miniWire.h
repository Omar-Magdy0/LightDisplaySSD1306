#ifndef MINIWIRE_H
#define MINIWIRE_H
#include <stdint.h>
#include <avr/io.h>

#define SCLFREQUENCY 400000u
#define BITRATEVAL (((F_CPU/SCLFREQUENCY) - 16)/2)

class miniWire{
miniWire();
void begin();
void end();
void write(uint8_t byte);
};


#endif