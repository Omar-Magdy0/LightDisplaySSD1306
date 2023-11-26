#ifndef FONTS_H
#define FONTS_H
#include <stdint.h>
#include <avr/pgmspace.h>


class font{
const unsigned char *bitMap;
uint8_t cWidth ;
uint8_t cHeight;
public :
font(const unsigned char *mb,uint8_t w ,uint8_t h){
    bitMap = mb;cWidth = w;cHeight = h;
}
uint8_t charWidth(){return cWidth;}
uint8_t charHeight(){return cHeight;}
const unsigned char *charBitMap(){return bitMap;} 
};




#endif