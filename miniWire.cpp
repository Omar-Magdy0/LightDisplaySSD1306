#include "miniWire.h"



void miniWire::write(uint8_t byte){
}

void miniWire::begin(){
TWBR = BITRATEVAL;
PORTC |= (1<<PORTC5) | (1<<PORTC4);
TWCR |= (1<<TWEN) | (1<<TWIE) | (1<<TWEA);
}

void miniWire::end();