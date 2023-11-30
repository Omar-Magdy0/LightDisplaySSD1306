#ifndef FUNCYOBJECTS_H
#define FUNCYOBJECTS_H

#include "../lightDisplay.h"

class FUNKYSquare{
private :
lightDisplay *dispo;
int16_t X0,Y0,width,height; 

public :
FUNKYSquare(int16_t x,int16_t y,uint8_t w,uint8_t h,lightDisplay *dr);
float angle;
void DisplayFunction();
};





#endif