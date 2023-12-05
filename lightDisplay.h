#ifndef LIGHTDISPLAY_H
#define LIGHTDISPLAY_H


#include <Wire.h>
#include <Print.h>
#include "SSD1306.h"

#define BASICFONT
#ifdef BASICFONT
#include "Fonts/basicFont.h"
#else
#define BASICFONT_WIDTH  0
#define BASICFONT_HEIGHT  0
extern unsigned char *basicFontBitmap;
#endif




// FUNCTIONS AND THE WHOLE LIBRARY IS WORKING WITH 128BYTE buffer allocated with the begin() method
// SUITABLE FOR LOW RAM SITUATIOONS
// THE IMPLEMENTATION AND GRAPHIC WORK AS FOLOWS YOU HAVE A ROW IN THE FORM YOU WANT THEN SEND
// THEN CHECK FOR THE NEXT ROW AND SEND WHERE ROW IS A PAGE(CHECK DATASHEET) 8BITS HEIGHT AND ROW WIDTH 128BYTE
// SO THINGS GO AS FOLLOWS YOU HAVE THE FIRST ROW READY THEN SEND THEN CHECK FOR NEXT ROW THEN READY AND SOO ON

// LATER ON I AM ADDING FONTS AND ROTATION




//define HORIZONTALBITMAPS

#define WIRE_MAX 32
#define LIGHTDISPBLACK 0
#define LIGHTDISPWHITE 1
#define NUMOFPAGES 8
#define WHITE 1
#define BLACK 0

class lightDisplay : public Print{
private:


public:
lightDisplay(uint8_t w, uint8_t h, TwoWire *twi = &Wire);
~lightDisplay();

//BEGIN allocate buffer and Initialize screen with setting
bool begin(uint8_t vcc = SSD1306_SWITCHCAPVCC,uint8_t address = 0x3C,bool periphBegin = true);

//basic screen functions
void pageSelect(uint8_t page);
void pageDisplay();
void clearPage(uint8_t COLOR);
void wholeScreenClearDisplay();
void setScreenRotation(uint8_t r = 0);
/******************************************************************************/
// CORE trig and shape drawing functions
/******************************************************************************/

void drawPixel(int16_t COORDX,int16_t COORDY,uint8_t COLOR);
void readAtomicBounds(int16_t COORDX,int16_t COORDY,uint8_t COLOR);
void bresenhamLine(int16_t X0,int16_t Y0,int16_t X1,int16_t Y1,uint8_t COLOR);
void Hline(int16_t X0,int16_t X1,int16_t Y,uint8_t COLOR);
void Vline(int16_t Y0,int16_t Y1,int16_t X,uint8_t COLOR);
void drawLine(int16_t X0,int16_t Y0,int16_t X1,int16_t Y1,uint8_t COLOR);
void drawRect(int16_t X0,int16_t Y0,uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR);
void drawFillRect(int16_t X0,int16_t Y0,uint8_t WIDTH,
                    uint8_t HEIGHT,uint8_t COLOR);
void drawCircle(int16_t X0,int16_t Y0,int16_t R, uint8_t COLOR);
void drawQuartCircle(int16_t X0,int16_t Y0,int16_t R,int8_t quart,uint8_t COLOR);
void drawWeirdFillCircle(int16_t X0,int16_t Y0,uint8_t R, uint8_t COLOR);
void drawFillQuartCircle(int16_t X0,int16_t Y0,uint8_t R,
                          uint8_t quart,uint8_t COLOR);
void drawFillCircle(int16_t X0,int16_t Y0,uint8_t R,uint8_t COLOR);

/******************************************************************************/
// Displaying and SENDING BUFFER PART
/******************************************************************************/

void displayFunctionGroup(uint8_t startPage,uint8_t endPage,void(*function)());
void displayFunctionGroupOpt(void(*function)());
void atomicDisplay(int8_t x,uint8_t WIDTH);
void drawAtomicArea(uint8_t x,uint8_t WIDTH,uint8_t startPage,uint8_t endPage,void (*function)());

/******************************************************************************/
// DISPLAYING TEXT FUNCTIONS PART
/******************************************************************************/

void setCursor(uint8_t x,uint8_t y);
void setTextColor(uint8_t COLOR);
void setWrap(uint8_t c);
void drawChar(int16_t x,int16_t y,unsigned char C,uint8_t COLOR);

  using Print::write;
virtual size_t write(uint8_t);
uint8_t getCursorX();
uint8_t getCursorY();
void charBounds(unsigned char c,int16_t *x,int16_t *y,int16_t *minX,
                  int16_t *minY,int16_t *maxX,int16_t *maxY);
void getTextBounds(const char *str, int16_t X0, int16_t Y0, int16_t *X1,
                    int16_t *Y1,uint8_t *W,uint8_t *H);
void getTextBounds(const String &str, int16_t x, int16_t y, int16_t *x1,
                   int16_t *y1, uint8_t *w, uint8_t *h);
void getTextBounds(const __FlashStringHelper *s, int16_t x, int16_t y,
                    int16_t *x1, int16_t *y1, uint8_t *w, uint8_t *h);

#ifdef EXTERNAL_FONTS
void setFont(font *f);// Here we select the font object
#endif
            
/******************************************************************************/
// BITMAP FUNCTIONS
/******************************************************************************/

void drawBitMap(const unsigned char BITMAP[],int16_t X0,int16_t Y0,
                  uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR,bool PGM);
void drawBitMapFullScreen(const unsigned char BITMAP[],uint8_t X0,uint8_t Y0,
                  uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR);
void drawBitMap_V(const unsigned char BITMAP[],int16_t X0,int16_t Y0,
                  uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR,bool PGM);
#ifdef HORIZONTALBITMAPS
void drawBitMap_H(const unsigned char BITMAP[],int16_t X0,int16_t Y0,
                  uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR,bool PGM);
#endif

protected:
uint8_t *buffer;
TwoWire *wire;
#ifdef EXTERNAL_FONTS
font *Font = &basicFont;
#endif
uint8_t rotation = 0;
uint8_t cursorX = 0;
uint8_t cursorY = 0;
uint8_t minX = 255;
uint8_t maxX = 0;
uint8_t minPage_toEdit = 255;
uint8_t maxPage_toEdit = 0;
uint8_t textWrap = 0;
#ifdef HORIZONTALBITMAPS
uint8_t bitMapRotation = 1;
#endif
uint8_t textColor = LIGHTDISPWHITE;
bool bufferOptimization = false;
uint8_t __width__; // THE ACTUAL WIDTH OF THE DISPLAY MODULE (THE BIGGER SIDE)
uint8_t __height__; // THE ACTUAL HEIGHT OF THE DISPLAY MODULE (THE SMALLER SIDE)
uint8_t I2Caddr;
int8_t currentPage;
void sendCommand(uint8_t command);
void sendCommandList(const uint8_t *command, uint8_t n);
};



#endif