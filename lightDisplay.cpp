#include "lightDisplay.h"
#include <stdint.h>
#include <Arduino.h>



#define wireClk 400e3
#define restoreClk 100e3
#define TRANSACTION_START wire->setClock(wireClk)    ///< Set before I2C transfer
#define TRANSACTION_END wire->setClock(restoreClk) ///< Restore after I2C transfer
/******************************************************************************/
#define _Swap_int16(a, b)\
    {             \
    int16_t t = a;\
    a = b;        \
    b = t;        \
    }                       
/******************************************************************************/
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
/******************************************************************************/
//CONSTRUCTOR
lightDisplay::lightDisplay(uint8_t w, uint8_t h, TwoWire *twi)
:  buffer(NULL),wire(twi ? twi : &Wire),__width(w),__height(h)
{
}

/******************************************************************************/
//DECONSTRUCTOR
lightDisplay::~lightDisplay(){
      if (buffer) {
    free(buffer);
    buffer = NULL;
  }
}

/******************************************************************************/
//SEND SINGLE COMMAND BYTES
void lightDisplay::sendCommand(uint8_t command){
    wire->beginTransmission(I2Caddr);
    wire->write((uint8_t)0X00); // Co=0 , D/C = 0
    wire->write((uint8_t)command);
    wire->endTransmission();
}

/******************************************************************************/
//SEND ARRAYS OF COMMAND BYTES
void lightDisplay::sendCommandList(const uint8_t *command, uint8_t n){
    wire->beginTransmission(I2Caddr);
    wire->write((uint8_t)0x00); // Co = 0, D/C = 0
    uint16_t bytesOut = 1;
    while (n--) {
        if (bytesOut >= WIRE_MAX) {
            wire->endTransmission();
            wire->beginTransmission(I2Caddr);
            wire->write((uint8_t)0x00); // Co = 0, D/C = 0
            bytesOut = 1;
        }
    wire->write(pgm_read_byte(command++));
    bytesOut++;
    }
    wire->endTransmission();
}

/******************************************************************************/
/*!
    @brief allocate memory for buffer and initiate screen with settings 
    @param vcc Power source for the screen affects choice for contrast
    @param address I2C address 
    @param periphBegin affects if we are going to initialize a peripheral or not Default true
*/
bool lightDisplay::begin(uint8_t vcc,uint8_t address,bool periphBegin){
    uint8_t contrast;
    uint8_t comPins = 18; 
  if ((!buffer) && !(buffer = (uint8_t *)malloc(__width * ((__height + 7) / 8))))return false;
    if(vcc == SSD1306_SWITCHCAPVCC){contrast = 0x7F;}
    if(periphBegin){wire->begin();}
    I2Caddr = address;

    TRANSACTION_START;
    sendCommand(SSD1306_SETCOMPINS);
    sendCommand(comPins);
    sendCommand(SSD1306_SETCONTRAST);
    sendCommand(contrast);
    sendCommand(SSD1306_SETPRECHARGE); // 0xd9
    sendCommand((vcc == SSD1306_EXTERNALVCC) ? 0x22 : 0xF1);
    static const uint8_t PROGMEM init1[] = {SSD1306_DISPLAYOFF,
                                            SSD1306_SETMULTIPLEX,
                                            0x3F,
                                            SSD1306_SETDISPLAYOFFSET,
                                            0x00};
    sendCommandList( init1 , sizeof(init1));
    static const uint8_t PROGMEM init2[] = {SSD1306_SETDISPLAYCLOCKDIV,
                                            0x80,
                                            SSD1306_MEMORYMODE,
                                            0x02,
                                            SSD1306_SEGREMAP | 0x1,
                                            SSD1306_COMSCANDEC};
    sendCommandList(init2, sizeof(init2));
    static const uint8_t PROGMEM init3[] = {SSD1306_SETSTARTLINE | 0x00,
                                            SSD1306_CHARGEPUMP,
                                            SSD1306_SETVCOMDETECT, 
                                            0x40,
                                            };
    sendCommandList(init3, sizeof(init3)); 
    static const uint8_t PROGMEM init4[] = {SSD1306_DISPLAYALLON_RESUME,
                                            SSD1306_NORMALDISPLAY,      
                                            SSD1306_DEACTIVATE_SCROLL,
                                            SSD1306_DISPLAYON
                                            };                                          
    sendCommandList(init4, sizeof(init4));  

    TRANSACTION_END;
    return true;
}
/******************************************************************************/
void lightDisplay::setScreenRotation(){
    sendCommand(0xC0);
    sendCommand(0xA0);
};
/******************************************************************************/
#ifdef __AVR__
// Bitmask tables of 0x80>>X and ~(0x80>>X), because X>>Y is slow on AVR
const uint8_t PROGMEM setBit[] = {0x01, 0x02, 0x04, 0x08,
                                 0x10, 0x20, 0x40, 0x80};
const uint8_t PROGMEM clrBit[] = {0xFE, 0xFD, 0xFB, 0xF7,
                                  0xEF, 0xDF, 0xBF, 0x7F};
#endif

/******************************************************************************/

inline void lightDisplay::drawPixel(int16_t COORDX,int16_t COORDY,uint8_t COLOR){

    uint8_t *ptr = &buffer[COORDX];
    if((COORDY / 8) != currentPage)return;
    else if((COORDX >= __width)||(COORDY >= __height))return;

    if(bufferOptimization){
        if(COORDX < minX)minX = COORDX;
        if(COORDX > maxX)maxX = COORDX;
        if(COORDY/8 < minPage_toEdit)minPage_toEdit = COORDY/8;
        if(COORDY/8 > maxPage_toEdit)maxPage_toEdit = COORDY/8;
        return;
    }
    if((COORDY / 8) == this->currentPage){
    #ifdef __AVR__
        if (COLOR){
            *ptr |= pgm_read_byte(&setBit[COORDY & 7]);}
    else
        *ptr &= pgm_read_byte(&clrBit[COORDY & 7]);
    #else
    if (COLOR)
      *ptr |= 1 << (COORDY & 7);
    else
      *ptr &= ~(1 << (COORDY & 7));
    #endif
    }
}

/******************************************************************************/
/*!
    @brief THE WORKING PRINCIPLE IS AS FOLLOWS first we checK for the screen pages we gonna NEED
    THEN we check if current page we are pointing to is a page we will need to write and if yes
    we write accordingly and Set YStart and YEnd for the needed page
    @param COORDX X coordinate for the first point on line
    @param COORDY Y coordinate for the first point on line
    @param ENDX   X coordinate for the last point on line
    @param ENDY   Y coordinate for the last point on line
    @param COLOR  Color of the drawn line WHITE/BLACK supported
*/
void lightDisplay::bresenhamLine(int16_t X0,int16_t Y0,int16_t X1,int16_t Y1,uint8_t COLOR){
    uint8_t steep = abs((Y1 - Y0)/(X1 - X0));
    if(steep){
        _Swap_int16(X0,Y0);
        _Swap_int16(X1,Y1);
    }

    if(X0 > X1){
        _Swap_int16(X0,X1);
        _Swap_int16(Y0,Y1);  
    }
    
    int16_t dy = abs(Y1 - Y0);
    int16_t dx = X1 - X0;
    int16_t err = dx/2;
    int8_t ystep;

    if (Y0 < Y1) {
        ystep = 1;
    } else {
        ystep = -1;
    }

    for(;X0 <= X1; X0 += 1){
        err -= dy;
        if (err < 0) {
            Y0 += ystep;
            err += dx;
        }
        if (steep) {
            if(X0/8 == currentPage)break;
        } else {
            if(Y0/8 == currentPage)break;
        }
    }

    for (;X0 <= X1; X0++) {
        if (steep) {
            drawPixel(Y0, X0, COLOR);
        } else {
            drawPixel(X0, Y0, COLOR);
        }
    err -= dy;
        if (err < 0) {
            Y0 += ystep;
            err += dx;
        }
  }
}
/******************************************************************************/
void lightDisplay::Vline(int16_t Y0,int16_t Y1,int16_t X,uint8_t COLOR){
    if(Y0 > Y1){_Swap_int16(Y0,Y1);}
    for(;Y0 <= Y1;Y0++){if(Y0/8 == currentPage)break;}
    for(;Y0 <= Y1;Y0++){
        drawPixel(X,Y0,COLOR);
        if(Y0 > (currentPage + 1)*8)return;
    }
}
/******************************************************************************/
void lightDisplay::Hline(int16_t X0,int16_t X1,int16_t Y,uint8_t COLOR){
    if(X0 > X1){_Swap_int16(X0,X1);}
    if(Y/8 != currentPage)return;
    for(;X0 <= X1;X0++){
        drawPixel(X0,Y,COLOR);
    }
}
/******************************************************************************/
void lightDisplay::drawLine(int16_t X0,int16_t Y0,int16_t X1,int16_t Y1,uint8_t COLOR){
    if(X0 == X1)Vline(Y0,Y1,X0,COLOR);
    else if(Y0 == Y1)Hline(X0,X1,Y0,COLOR);
    else bresenhamLine(X0,Y0,X1,Y1,COLOR);
}
/******************************************************************************/
void lightDisplay::pageSelect(uint8_t page){
if(page < 0)return;
else if(page > (NUMOFPAGES - 1))return;
this->currentPage = page;
}
/******************************************************************************/
void lightDisplay::pageDisplay(){
    uint8_t count = __width;
    uint8_t *ptr = buffer;
    TRANSACTION_START;
    sendCommand(0 & 0b00001111);
    sendCommand( ((0>>4) & 0b00001111) | (0x10));
    sendCommand(0xB0 | currentPage);

    wire->beginTransmission(I2Caddr);
    wire->write((uint8_t)0x40); // Co = 0, D/C = 1
    uint16_t bytesOut = 1;
    while (count--) {
        if (bytesOut >= WIRE_MAX) {
            wire->endTransmission();
            wire->beginTransmission(I2Caddr);
            wire->write((uint8_t)0x40); // Co = 0, D/C = 1
            bytesOut = 1;
        }
        wire->write(*ptr++);
        bytesOut++;
    }
    wire->endTransmission();
    TRANSACTION_END;
}
/******************************************************************************/
void lightDisplay::clearPage(){
        for(uint8_t j = 0; j < __width; j++){
            buffer[j] = 0;   
        }
}

/******************************************************************************/
void lightDisplay::wholeScreenClearDisplay()
{
    for(uint8_t i = 0; i < NUMOFPAGES; i++){
        pageSelect(i);
        clearPage();
        pageDisplay();
        }
}
/******************************************************************************/
void lightDisplay::drawBitMapFullScreen(const unsigned char BITMAP[],uint8_t X0,
                                            uint8_t Y0,uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR)
{
    uint8_t xmax = X0 + WIDTH;
    for(;X0 < xmax;X0++){
        buffer[X0] |= pgm_read_byte(&BITMAP[X0 + currentPage*128]);
    }
}
/******************************************************************************/
void lightDisplay::drawRect(int16_t X0,int16_t Y0,uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR)
{
    Vline(Y0,(Y0 + HEIGHT - 1),X0,COLOR);
    Vline(Y0,(Y0 + HEIGHT - 1),(X0 + WIDTH - 1),COLOR);
    Hline(X0,(X0 + WIDTH - 1),Y0,COLOR);
    Hline(X0,(X0 + WIDTH - 1),(Y0 + HEIGHT - 1),COLOR);
}
/******************************************************************************/
void lightDisplay::drawFillRect(int16_t X0,int16_t Y0,uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR)
{
    uint8_t YMAX = Y0 + HEIGHT;
    for(;Y0 < YMAX;Y0++){if(Y0/8 == currentPage)break;}
    if(Y0/8 != currentPage)return;
    for(;Y0 < YMAX;Y0++){
        Hline(X0,(X0 + WIDTH - 1),Y0,COLOR);
    }
}
/******************************************************************************/
void lightDisplay::drawCircle(int16_t X0,int16_t Y0,int16_t R,uint8_t COLOR)
{
    int16_t d = 3 - 2 * R;
    int8_t x = 0;
    int8_t y = R;
    //TOP of the circle = Y0 - r;
    drawPixel(X0, Y0 + y, COLOR);
    drawPixel(X0, Y0 - y, COLOR);
    drawPixel(X0 + y, Y0, COLOR);
    drawPixel(X0 - y, Y0, COLOR);
    if(((Y0 + R) < (currentPage)*8) || ((Y0 - R) > (currentPage + 1)*8))return;

    while (x < y) {
        if (d >= 0) {
            y--;
            d = d + 4 * (x - y) + 10; 
        }else 
            d = d + 4 * x + 6; 
        x++;
        drawPixel(X0 + x, Y0 + y, COLOR);
        drawPixel(X0 - x, Y0 + y, COLOR);
        drawPixel(X0 + x, Y0 - y, COLOR);
        drawPixel(X0 - x, Y0 - y, COLOR);
        drawPixel(X0 + y, Y0 + x, COLOR);
        drawPixel(X0 - y, Y0 + x, COLOR);
        drawPixel(X0 + y, Y0 - x, COLOR);
        drawPixel(X0 - y, Y0 - x, COLOR);
    }
}
/******************************************************************************/
void lightDisplay::drawQuartCircle(int16_t X0,int16_t Y0,int16_t R,int8_t quart,uint8_t COLOR)
{
    int16_t d = 3 - 2 * R;
    int8_t x = 0;
    int8_t y = R;
      //TOP of the circle = Y0 - r;
    if(((Y0 + R) < (currentPage)*8) || ((Y0 - R) > (currentPage + 1)*8))return;

    while (x < y) {
        if (d >= 0) {
            y--;
            d = d + 4 * (x - y) + 10; 
        }else 
            d = d + 4 * x + 6; 
        x++;

        switch (quart)
        {
        case 0:
            drawPixel(X0 - x, Y0 - y, COLOR);
            drawPixel(X0 - y, Y0 - x, COLOR);
            break;
        case 1:
            drawPixel(X0 + x, Y0 - y, COLOR);
            drawPixel(X0 + y, Y0 - x, COLOR);
            break;
        case 2:
            drawPixel(X0 + y, Y0 + x, COLOR);
            drawPixel(X0 + x, Y0 + y, COLOR);
            break;
        case 3:
            drawPixel(X0 - y, Y0 + x, COLOR);
            drawPixel(X0 - x, Y0 + y, COLOR);
            break;     
        default:
            break;
        }
    }
}
/******************************************************************************/
void lightDisplay::drawWeirdFillCircle(int16_t X0,int16_t Y0,uint8_t R, uint8_t COLOR)
{
    for(uint8_t rx = 0; rx <= R; rx++){drawCircle(X0,Y0,rx,COLOR);}
}
/******************************************************************************/
void lightDisplay::drawFillCircle(int16_t X0,int16_t Y0,uint8_t R,uint8_t COLOR)
{
    drawFillQuartCircle(X0,Y0,R,4,COLOR);
}
/******************************************************************************/
void lightDisplay::drawFillQuartCircle(int16_t X0,int16_t Y0,uint8_t R,uint8_t quart,uint8_t COLOR)
{
    int16_t d = 3 - 2 * R;
    int8_t x = 0;
    int8_t y = R;
    if(((Y0 + R) < (currentPage)*8) || ((Y0 - R) > (currentPage + 1)*8))return;

    if(quart == 4){Hline(X0 + R,X0 - R,Y0,COLOR);}

    while (x < y) {
        if (d >= 0) {
            y--;
            d = d + 4 * (x - y) + 10; 
        }else 
            d = d + 4 * x + 6; 
        x++;
        switch (quart)
        {
        case 0:
            Hline(X0 - x,X0 - 1,Y0 - y,COLOR);
            Hline(X0 - y,X0 - 1,Y0 - x,COLOR);
            break;

        case 1:
            Hline(X0 + x,X0 + 1,Y0 - y,COLOR);
            Hline(X0 + y,X0 + 1,Y0 - x,COLOR);
            break;

        case 2:
            Hline(X0 + x,X0 + 1,Y0 + y,COLOR);
            Hline(X0 + y,X0 + 1,Y0 + x,COLOR);
            break;

        case 3:
            Hline(X0 - x,X0 - 1,Y0 + y,COLOR);
            Hline(X0 - y,X0 - 1,Y0 + x,COLOR);
            break;

        case 4:
            Hline(X0 + x,X0 - x,Y0 + y,COLOR);
            Hline(X0 + x,X0 - x,Y0 - y,COLOR);
            Hline(X0 + y,X0 - y,Y0 + x,COLOR);
            Hline(X0 + y,X0 - y,Y0 - x,COLOR);
            break;
        default:
            break;
        }
    }
}
/******************************************************************************/
void lightDisplay::drawBitMap_V(const unsigned char BITMAP[],int16_t X0,int16_t Y0,
                                uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR,bool PGM)
{
    uint8_t Y = 0;                          // actual Y coordinate on the screen
    uint8_t Yrelative = 0;                  // relative Y coordinate where it is the value of Y coordinate of the buffer 
    uint8_t byte;                           // represents the byte we are currently accessing and checking for each bit
    uint8_t bit;
    if(Y0/8 > currentPage)return;                                           // check if current page has Y0 in it or not
    for(;Y < (Y0 + HEIGHT - 1);Y++){if ((Y/8) == currentPage)break;}      //always update Y coordinates with ever function call
    if((Y/8) != currentPage)return;                                        //Seem like current page doesnt include the Ymax
                                                                      //Start iterating Y coordinates and X coordinates and access bit and bytes 
    for(;Y < (currentPage + 1)*8;Y++){
        Yrelative = Y - Y0;
        if(Y < Y0){continue;}                           // Skip checking the buffer if we haven't reached start of bitmap yet
        else if(Y > (Y0 + HEIGHT -1 )){return;}             // if we have exceeded the max height
                                                        //Iterate X for full width with each Y iteration
        for(uint8_t x = 0;x < WIDTH; x++){
                            //Choose Byte and Bit accordingly
            if(PGM)
                byte = pgm_read_byte(&BITMAP[x + (Yrelative/8)*WIDTH]);
            else
                byte = BITMAP[x + (Yrelative/8)*WIDTH];

            bit =  pgm_read_byte(&setBit[Yrelative&7]) & (byte);

            if(bit)drawPixel( (X0 + x), Y, COLOR);
        }
    }
}

/******************************************************************************/


/* ROTATED BITMAP DRAWING SHOULD GO HERE  SAME LOGIC HOWEVER WRITING TO BUFFER DIFFERS
    SINCE ALSO THE X COORDINATE 
*/
// VALUABLE SOLUTION FOR HAVING SCREEN ROTATION FEATURE HERE 
/*
    SINCE ALREADY MOST FUNCTIONS SUPPORT ROTATION BY THEMSELVES 
    LINES / (filled/notfilled)RECTANCLES / (filled/notfilled)quarter circles
    and ONLY BITMAPS ARE THE ONE constraint for rotation
    I can Have two functions for drawing bitmaps 
    one that draws vertical bitmaps and one for horizontal bitmaps
    where each are encoded vertically (1-bit - 1-pixel)
*/

#ifdef HORIZONTALBITMAPS
void lightDisplay::drawBitMap_H(const unsigned char BITMAP[],int16_t X0,int16_t Y0,
                                uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR,bool PGM)
{                                           // X coords of the Bitmap 
    uint8_t byte;                           // represents the byte we are currently accessing and checking for each bit
    uint8_t bit;
    uint8_t x = WIDTH - 1;
    uint8_t ymax = Y0 + WIDTH - 1;


    if(Y0/8 > currentPage)return;
    for(;x > 0;x--,Y0++){if(Y0/8 == currentPage)break;}
    if((Y0/8) != currentPage)return; 

    for(;x > 0;Y0++,x--){  
        if(Y0 > ymax + 1)return;
        if(Y0/8 > currentPage)return;
        for(int i = 0; i < HEIGHT; i++){
            if(PGM)
                byte = pgm_read_byte(&BITMAP[x + (i/8)*WIDTH]);
            else 
                byte = BITMAP[x + (i/8)*WIDTH];

            bit =  pgm_read_byte(&setBit[i&7]) & (byte);
            if(bit)drawPixel( X0 + i, Y0, COLOR);
        }                              
    }
}
#endif
/******************************************************************************/
void lightDisplay::drawBitMap(const unsigned char BITMAP[],int16_t X0,int16_t Y0,
                                uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR,bool PGM)
{   
    #ifdef HORIZONTALBITMAPS
    if(bitMapRotation == 0){
    drawBitMap_V(BITMAP,X0,Y0,WIDTH,HEIGHT,COLOR,PGM);
    }
    else{
    drawBitMap_H(BITMAP,X0,Y0,WIDTH,HEIGHT,COLOR,PGM);
    }
    #else
    drawBitMap_V(BITMAP,X0,Y0,WIDTH,HEIGHT,COLOR,PGM);
    #endif
}


/******************************************************************************/
void lightDisplay::displayFunctionGroup(uint8_t startPage,uint8_t endPage,void(*function)())
{
    for(;startPage <= endPage; startPage++){    
        this->pageSelect(startPage);
        this->clearPage();
        function();
        this->pageDisplay();
    }
}
/******************************************************************************/
void lightDisplay::displayFunctionGroupOpt(void(*function)())
{
    bufferOptimization = true;
    if(bufferOptimization){
        for(int i = 0;i < NUMOFPAGES;i++){  
            this->pageSelect(i); 
            function();
        }
    }
    bufferOptimization = false;
    drawAtomicArea(minX,(maxX - minX + 1),minPage_toEdit,maxPage_toEdit,function);

    minX = 255;
    maxX = 0;
    minPage_toEdit = 255;
    maxPage_toEdit = 0;
}
/*******************************************************************************/
void lightDisplay::atomicDisplay(int8_t x,uint8_t WIDTH){
    TRANSACTION_START;
    sendCommand(x & 0b00001111);                        //SET higher and lower nibble of the column address
    sendCommand( ((x >> 4) & 0b00001111) | (0x10) );
    sendCommand(0xB0 | currentPage);
    uint8_t count = WIDTH;
    uint8_t *ptr = &buffer[x];
    wire->beginTransmission(I2Caddr);
    wire->write((uint8_t)0x40); // Co = 0, D/C = 1
    uint16_t bytesOut = 1;
    while (count--) {
        if (bytesOut >= WIRE_MAX) {
            wire->endTransmission();
            wire->beginTransmission(I2Caddr);
            wire->write((uint8_t)0x40); // Co = 0, D/C = 1
            bytesOut = 1;
        }
        wire->write(*ptr++);
        bytesOut++;
    }
    wire->endTransmission();
    TRANSACTION_END;
}
/*******************************************************************************/
void lightDisplay::drawAtomicArea(uint8_t x,uint8_t WIDTH,uint8_t startPage,uint8_t endPage,void (*function)())
{
    for(;startPage <= endPage; startPage++){    
        this->pageSelect(startPage);
        this->clearPage();
        function();
        this->atomicDisplay(x,WIDTH);
    }   
}
/*******************************************************************************/
void lightDisplay::drawChar(int16_t x,int16_t y,unsigned char C,uint8_t COLOR)
{
    unsigned char bytes[5];
    for(uint8_t i = 0;i < BASICFONT_WIDTH ; i++){
        bytes[i] =  pgm_read_byte(&basicFontBitmap[C*BASICFONT_WIDTH + i]);
    } 
    drawBitMap(bytes,x,y,BASICFONT_WIDTH,BASICFONT_HEIGHT,COLOR,0);
}
/*******************************************************************************/
#ifdef EXTERNAL_FONTS
void lightDisplay::setFont(font *f){
    Font = f;
}
#endif
/*******************************************************************************/
void lightDisplay::setTextColor(uint8_t COLOR)
{
    textColor = COLOR;
}
/*******************************************************************************/
void lightDisplay::setWrap(uint8_t c)
{
    textWrap = c;
}
/*******************************************************************************/
void lightDisplay::setCursor(uint8_t x,uint8_t y)
{
    cursorX = x;
    cursorY = y;
}
/*******************************************************************************/
uint8_t lightDisplay::getCursorX()
{
    return cursorX;
}
/*******************************************************************************/
uint8_t lightDisplay::getCursorY()
{
    return cursorY;
}
/*******************************************************************************/
void lightDisplay::charBounds(unsigned char c,int16_t *x,int16_t *y,
                                int16_t *minX,int16_t *minY,int16_t *maxX,int16_t *maxY)
{
    if(c == '\n'){
        *y += BASICFONT_HEIGHT + 1;
        *x = 0;
    }
    else if(c != '\r'){
        if(textWrap && ((BASICFONT_WIDTH + *x) > __width)){
            *x = 0;
            *y += BASICFONT_HEIGHT + 1;
        }
  
    int16_t x2 = *x + BASICFONT_WIDTH, // TOP - right pixel of char
            y2 = *y - BASICFONT_HEIGHT;
    if (x2 > *maxX)
        *maxX = x2; // Track max x, y
    if (*y > *maxY)
        *maxY = *y;
    if (*x < *minX)
        *minX = *x; // Track min x, y
    if (y2 < *minY)
        *minY = y2;
    *x += BASICFONT_WIDTH + 1; // Advance x one char
    }
}
/*******************************************************************************/
void lightDisplay::getTextBounds(const char *str,int16_t X0,int16_t Y0,
                                    int16_t *X1,int16_t *Y1,uint8_t *W,uint8_t *H)
{
  uint8_t c; // Current character
  int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1; // Bound rect
  // Bound rect is intentionally initialized inverted, so 1st char sets it

  *X1 = X0; // Initial position is value passed in
  *Y1 = Y0;
  *W = *H = 0; // Initial size is zero

  while ((c = *str++)) {
    // charBounds() modifies x/y to advance for each character,
    // and min/max x/y are updated to incrementally build bounding rect.
    charBounds(c, &X0, &Y0, &minx, &miny, &maxx, &maxy);
  }

  if (maxx >= minx) {     // If legit string bounds were found...
    *X1 = minx;           // Update x1 to least X coord,
    *W = maxx - minx; // And w to bound rect width
  }
  if (maxy >= miny) { // Same for height
    *Y1 = miny + 1;
    *H = maxy - miny;
  }
}
/*******************************************************************************/
size_t lightDisplay::write(uint8_t c)
{
    if(c == '\n'){
        cursorY += BASICFONT_HEIGHT + 1;
        cursorX = 0;
    }
    else if(c != '\r'){
        if(textWrap && ((BASICFONT_WIDTH + cursorX) > __width)){
            cursorX = 0;
            cursorY += BASICFONT_HEIGHT + 1;
        }
    drawChar(cursorX,cursorY - BASICFONT_HEIGHT + 1,c,textColor);
    cursorX += (BASICFONT_WIDTH + 1);
    }
    return 1;
 }
/*******************************************************************************/
void lightDisplay::getTextBounds(const String &str, int16_t x, int16_t y,
                                 int16_t *x1, int16_t *y1, uint8_t *w,
                                 uint8_t *h) {
  if (str.length() != 0) {
    getTextBounds(const_cast<char *>(str.c_str()), x, y, x1, y1, w, h);
  }
}
/*******************************************************************************/
void lightDisplay::getTextBounds(const __FlashStringHelper *str, int16_t x,
                                 int16_t y, int16_t *x1, int16_t *y1,
                                 uint8_t *w, uint8_t *h) {
  uint8_t *s = (uint8_t *)str, c;

  *x1 = x;
  *y1 = y;
  *w = *h = 0;

  int16_t minx = __width, miny = __height, maxx = -1, maxy = -1;

  while ((c = pgm_read_byte(s++)))
    charBounds(c, &x, &y, &minx, &miny, &maxx, &maxy);

  if (maxx >= minx) {
    *x1 = minx;
    *w = maxx - minx;
  }
  if (maxy >= miny) {
    *y1 = miny + 1;
    *h = maxy - miny;
  }
}
/*******************************************************************************/


