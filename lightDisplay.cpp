#include "lightDisplay.h"
#include "stdint.h"
#include <Arduino.h>


#define wireClk 400e3
#define restoreClk 100e3
#define TRANSACTION_START wire->setClock(wireClk)    ///< Set before I2C transfer
#define TRANSACTION_END wire->setClock(restoreClk) ///< Restore after I2C xfer
/******************************************************************************/
#define _Swap_uint8(a, b)\
    {             \
    uint8_t t = a;\
    a = b;        \
    b = t;        \
    }                       

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

#ifdef __AVR__
// Bitmask tables of 0x80>>X and ~(0x80>>X), because X>>Y is slow on AVR
const uint8_t PROGMEM setBit[] = {0x01, 0x02, 0x04, 0x08,
                                 0x10, 0x20, 0x40, 0x80};
const uint8_t PROGMEM clrBit[] = {0xFE, 0xFD, 0xFB, 0xF7,
                                  0xEF, 0xDF, 0xBF, 0x7F};
#endif

/******************************************************************************/

void lightDisplay::drawPixel(uint8_t COORDX,uint8_t COORDY,uint8_t COLOR){
    uint8_t *ptr = &buffer[COORDX];
    if((COORDY / 8) != currentPage)return;
    else if((COORDX >= __width)||(COORDY >= __height)){
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
void lightDisplay::bresenhamLine(uint8_t X0,uint8_t Y0,uint8_t X1,uint8_t Y1,uint8_t COLOR){
    uint8_t steep = abs((Y1 - Y0)/(X1 - X0));
    if(steep){
        _Swap_uint8(X0,Y0);
        _Swap_uint8(X1,Y1);
    }

    if(X0 > X1){
        _Swap_uint8(X0,X1);
        _Swap_uint8(Y0,Y1);  
    }
    
    uint8_t dy = abs(Y1 - Y0);
    uint8_t dx = X1 - X0;
    int8_t err = dx/2;
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
void lightDisplay::Vline(uint8_t Y0,uint8_t Y1,uint8_t X,uint8_t COLOR){
    if(Y0 > Y1){_Swap_uint8(Y0,Y1);}
    for(;Y0 <= Y1;Y0++){if(Y0/8 == currentPage)break;}
    for(;Y0 <= Y1;Y0++){
        drawPixel(X,Y0,COLOR);
        if(Y0 > (currentPage + 1)*8)return;
    }
}
/******************************************************************************/
void lightDisplay::Hline(uint8_t X0,uint8_t X1,uint8_t Y,uint8_t COLOR){
    if(X0 > X1){_Swap_uint8(X0,X1);}
    if(Y/8 != currentPage)return;
    for(;X0 <= X1;X0++){
        drawPixel(X0,Y,COLOR);
    }
}
/******************************************************************************/
void lightDisplay::drawLine(uint8_t X0,uint8_t Y0,uint8_t X1,uint8_t Y1,uint8_t COLOR){
    if(X0 == X1)Vline(Y0,Y1,X0,COLOR);
    else if(Y0 == Y1)Hline(X0,X1,Y0,COLOR);
    else bresenhamLine(X0,Y0,X1,Y1,COLOR);
}
/******************************************************************************/
void lightDisplay::pageSelect(uint8_t page){
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
void lightDisplay::wholeScreenClearDisplay(){
    for(uint8_t i = 0; i < NUMOFPAGES; i++){
        pageSelect(i);
        clearPage();
        pageDisplay();
        }
    }
/******************************************************************************/
void lightDisplay::drawBitMapFullScreen(const unsigned char BITMAP[],uint8_t X0,uint8_t Y0,uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR){
    uint8_t xmax = X0 + WIDTH;
    for(;X0 < xmax;X0++){
        buffer[X0] |= pgm_read_byte(&BITMAP[X0 + currentPage*128]);
    }
}
/******************************************************************************/
void lightDisplay::drawRect(uint8_t X0,uint8_t Y0,uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR){
Vline(Y0,(Y0 + HEIGHT - 1),X0,COLOR);
Vline(Y0,(Y0 + HEIGHT - 1),(X0 + WIDTH - 1),COLOR);
Hline(X0,(X0 + WIDTH - 1),Y0,COLOR);
Hline(X0,(X0 + WIDTH - 1),(Y0 + HEIGHT - 1),COLOR);
}
/******************************************************************************/
void lightDisplay::drawFillRect(uint8_t X0,uint8_t Y0,uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR){
    uint8_t YMAX = Y0 + HEIGHT;
    for(;Y0 < YMAX;Y0++){if(Y0/8 == currentPage)break;}
    if(Y0/8 != currentPage)return;
    for(;Y0 < YMAX;Y0++){
        Hline(X0,(X0 + WIDTH - 1),Y0,COLOR);
    }
}
/******************************************************************************/
void lightDisplay::drawCircle(int16_t X0,int16_t Y0,int16_t R,uint8_t COLOR){
  int16_t d = 3 - 2 * R;
  int8_t x = 0;
  int8_t y = R;
    //TOP of the circle = Y0 - r;
  drawPixel(X0, Y0 + y, COLOR);
  drawPixel(X0, Y0 - y, COLOR);
  drawPixel(X0 + y, Y0, COLOR);
  drawPixel(X0 - y, Y0, COLOR);
    if(((Y0 + R) < (currentPage)*8) || ((Y0 - R) > (currentPage + 1)*8))return;
    Serial.println("run");
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
void lightDisplay::drawQuartCircle(int16_t X0,int16_t Y0,int16_t R,int8_t quart,uint8_t COLOR){
    int16_t d = 3 - 2 * R;
    int8_t x = 0;
    int8_t y = R;
      //TOP of the circle = Y0 - r;
    if(((Y0 + R) < (currentPage)*8) || ((Y0 - R) > (currentPage + 1)*8))return;
    Serial.println("run");
    while (x < y) {
        if (d >= 0) {
            y--;
            d = d + 4 * (x - y) + 10; 
        }else 
            d = d + 4 * x + 6; 
        x++;
        if(quart == 0){
            drawPixel(X0 - x, Y0 - y, COLOR);
            drawPixel(X0 - y, Y0 - x, COLOR);
        }
        if(quart == 1){
            drawPixel(X0 + x, Y0 - y, COLOR);
            drawPixel(X0 + y, Y0 - x, COLOR);
        }
        if(quart == 2){
            drawPixel(X0 + y, Y0 + x, COLOR);
            drawPixel(X0 + x, Y0 + y, COLOR);
        }
        if(quart == 3){
            drawPixel(X0 - y, Y0 + x, COLOR);
            drawPixel(X0 - x, Y0 + y, COLOR);
        }
    }
}
/******************************************************************************/
void lightDisplay::drawWeirdFillCircle(int16_t X0,int16_t Y0,int16_t R, uint8_t COLOR){
    for(uint8_t rx = 0; rx <= R; rx++){drawCircle(X0,Y0,rx,COLOR);}
}
/******************************************************************************/
void lightDisplay::drawFillCircle(uint8_t X0,uint8_t Y0,uint8_t R,uint8_t COLOR){
    drawFillQuartCircle(X0,Y0,R,4,COLOR);
}
/******************************************************************************/
void lightDisplay::drawFillQuartCircle(int16_t X0,int16_t Y0,int16_t R,uint8_t quart,uint8_t COLOR){
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
void lightDisplay::drawBitMap(const unsigned char BITMAP[],uint8_t X0,uint8_t Y0,uint8_t WIDTH,uint8_t HEIGHT,uint8_t COLOR,bool PGM){
    uint8_t Y = 0;                          // actual Y coordinate on the screen
    uint8_t Yrelative = 0;                  // relative Y coordinate where it is the value of Y coordinate of the buffer 
    uint8_t byte;                           // represents the byte we are currently accessing and checking for each bit
    uint8_t bit;

    for(;Y < (Y0 + HEIGHT - 1);Y++){if ((Y/8) == currentPage)break;}      //always update Y coordinates with ever function call
    if((Y/8) != currentPage)return;
                                                        //Start iterating Y coordinates and X coordinates and access bit and bytes 
    for(;Y < (currentPage + 1)*8;Y++){
        Yrelative = Y - Y0;
        if(Y < Y0){continue;}                           // Skip checking the buffer if we haven't reached start of bitmap yet
        else if(Y > (Y0 + HEIGHT -1 )){return;}             // if we have exceeded the max height
                                                        //Iterate X for full width with each Y iteration
        for(uint8_t x = 0;x < WIDTH; x++){
                                                        //Choose Byte and Bit accordingly
            if(PGM){byte = pgm_read_byte(&BITMAP[x + (Yrelative/8)*WIDTH]);}
            else{byte = BITMAP[x + (Yrelative/8)*WIDTH];}
            bit =  pgm_read_byte(&setBit[Yrelative&7]) & (byte);

            if(COLOR){
                if(bit)drawPixel( (X0 + x), Y, LIGHTDISPWHITE);}
            else{
                if(!bit)drawPixel( (X0 + x), Y, LIGHTDISPWHITE);}
        }
    }
}
/******************************************************************************/
void lightDisplay::displayFunctionGroup(uint8_t startPage,uint8_t endPage,void(*function)()){
    if(endPage  > (NUMOFPAGES - 1))endPage = (NUMOFPAGES - 1);
    for(;startPage <= endPage; startPage++){
        this->pageSelect(startPage);
        this->clearPage();
        function();
        this->pageDisplay();
        }
}
/*******************************************************************************/
void lightDisplay::drawChar(uint8_t x,uint8_t y,unsigned char C,uint8_t COLOR,uint8_t BG){
    unsigned char bytes[5];
    for(uint8_t i = 0;i < Font->charWidth();i++){
        bytes[i] =  pgm_read_byte(&Font->charBitMap()[C*5 + i]);
        Serial.println(bytes[i]);
    } 
    drawBitMap(bytes,x,y,Font->charWidth(),Font->charHeight(),COLOR,0);
}
/*******************************************************************************/





