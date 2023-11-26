#ifndef FUNCYOBJECTS_H
#define FUNCYOBJECTS_H

#include "lightDisplay.h"

class FUNKYSquare{
private :
lightDisplay *dispo;
int X0,Y0,width,height; 

public :
FUNKYSquare(uint8_t x,uint8_t y,uint8_t w,uint8_t h,lightDisplay *dr);
float angle;
void DisplayFunction();
};





#endif