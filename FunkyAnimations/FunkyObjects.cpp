#include "FunkyObjects.h"
#include <math.h>






#define rotateFunc(a , b , ang , cx , cy)\
{                                        \
  a = a - cx;                            \
  b = b - cy;                            \
  int t = a;                             \
  a = a*cos(ang) - b*sin(ang);           \
  b = b*cos(ang) + t*sin(ang);           \
  a = a + cx;                            \
  b = b + cy;                            \
}              

FUNKYSquare::FUNKYSquare(uint8_t x,uint8_t y,uint8_t w,uint8_t h,lightDisplay *dr){
      X0 = x;Y0 = y;width = w;height = h;dispo = dr;
}

void FUNKYSquare::DisplayFunction(){
    int8_t x0 = X0,y0 = Y0,
    x1 = X0 + width,y1 = Y0,
    x2 = X0 + width,y2 = Y0 + height,
    x3 = X0,y3 = Y0 + height;
    int8_t midX = X0 + (width/2);
    int8_t midY = Y0 + (height/2);
    rotateFunc(x0,y0,angle,midX,midY);
    rotateFunc(x1,y1,angle,midX,midY);
    rotateFunc(x2,y2,angle,midX,midY);
    rotateFunc(x3,y3,angle,midX,midY);
    if(angle > 2*M_PI)angle = 0;
    dispo->drawLine(x0,y0,x1,y1,LIGHTDISPWHITE);
    dispo->drawLine(x0,y0,x3,y3,LIGHTDISPWHITE);
    dispo->drawLine(x2,y2,x1,y1,LIGHTDISPWHITE);
    dispo->drawLine(x2,y2,x3,y3,LIGHTDISPWHITE);
}