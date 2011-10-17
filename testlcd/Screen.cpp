#include "Screen.h"
#include <string.h>



// The following protect the buffer's "Ready" states and interruptState
#define ISR_ENTER if(Screen::ops != 0) return // Don't process interrupt if in critical block
#define CODE_ENTER     ++Screen::ops          // Enter critical block
#define CODE_LEAVE     --Screen::ops          // Leave critical block

enum{
  INTERRUPTSTATE_IDLE            /* Do nothing at a slow rate*/  , 
  INTERRUPTSTATE_CMD_HI1         /* Home cursor, send first nibble - RS Low, E High, fast rate*/  , 
  INTERRUPTSTATE_CMD_LO1         /* Home cursor, end first nibble - E Low*/  , 
  INTERRUPTSTATE_CMD_HI2         /* Home cursor, send second nibble - E High*/   , 
  INTERRUPTSTATE_CMD_LO2         /* Home cursor, end second nibble - E Low*/  , 
  INTERRUPTSTATE_CMD_END         /* Home cursor, end the command - RS High*/  , 
  INTERRUPTSTATE_E_GOHI          /* Data loop, start send data - E High*/  , 
  INTERRUPTSTATE_E_GOLO          /* Data loop, end send data - E Low*/
};

#define INITIALIZE_CMD         0x03
#define SET_4BIT_CMD           0x02

#define CLEAR_CMD              0x01 // Clear the display
#define ENTRY_MODE_CMD         0x04
#define DISPLAY_CMD            0x08
#define FUNC_SET_CMD           0x20
#define WRITE_CGRAM_CMD        0x40
#define HOME_CURSOR_CMD        0x80 // Set the LCD to Addr 0

#define TIMER_A_VALUE          0
#define TIMER_B_VALUE          _BV(CS32) | _BV(WGM32); //1/128th, CTC = 16 microsecond timer
#define INTERRUPT_IDLE         128  // Slower rate when we aren't updating
#define INTERRUPT_BUSY         8    // Faster rate while we update (must be > 37 microseconds)


#define FUNCTION_8BIT        0x10
#define FUNCTION_4BIT        0x00
#define FUNCTION_MULTILINE   0x08
#define FUNCTION_ONELINE     0x00
#define FUNCTION_5x10        0x04
#define FUNCTION_5x8         0x00

#define DISPLAY_DISPLAY_ON     0x04
#define DISPLAY_DISPLAY_OFF    0x00
#define DISPLAY_CURSOR_ON      0x02
#define DISPLAY_CURSOR_OFF     0x00
#define DISPLAY_BLINK_ON       0x01
#define DISPLAY_BLINK_OFF      0x00

#define ENTRY_LEFTTORIGHT      0x02
#define ENTRY_RIGHTTOLEFT      0x00
#define ENTRY_SHIFT_ON         0x01    // Shift display on write
#define ENTRY_SHIFT_OFF        0x00    // Do not shift display on writeByte

// The Timer interrupt we'll use need to update ISR definition if this changes
// everything else should be macro hackery
#define LcdInterruptNumber 4

#define MkTimerRegisterA(x)      TCCR ## x ## A
#define MkTimerRegisterB(x)      TCCR ## x ## B
#define MkOutCmpRegisterA(x)     OCR ## x ## A
#define MkTimerMaskRegister(x)   TIMSK ## x
#define MkTimerMaskBit(x)        OCIE ## x ## A

#define TimerA(x)    MkTimerRegisterA(x)
#define TimerB(x)    MkTimerRegisterB(x)
#define OutCmpA(x)   MkOutCmpRegisterA(x)
#define TimerMask(x) MkTimerMaskRegister(x)
#define TimerBit(x)  MkTimerMaskBit(x)



  uint8_t _data_pins[4];
  uint8_t _rs_pin; // LOW: command.  HIGH: character.
  //uint8_t _rw_pin; // LOW: write to LCD.  HIGH: read from LCD.
  uint8_t _enable_pin; // activated by a HIGH pulse.



volatile uint8_t Screen::ops=0;
volatile uint8_t Screen::interruptState=INTERRUPT_IDLE;
uint8_t Screen::readTick=0;

struct LCD_BUFFER Screen::lcdBuffers[2];

LCD_BUFFER *Screen::pRead=0;
uint8_t *Screen::pReadCurrent=0;


LCD_BUFFER *Screen::pWriteNext=0;
LCD_BUFFER *Screen::pWrite=0;
uint8_t *Screen::pWriteCurrent=0;




Screen::Screen(uint8_t rs, uint8_t enable,
    uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
    _rs_pin = rs;
  _enable_pin = enable;
  
  _data_pins[0] = d0;
  _data_pins[1] = d1;
  _data_pins[2] = d2;
  _data_pins[3] = d3;
  
    memset(buffer, ' ', LCD_ROWS * LCD_COLS);
    pCurrent = buffer;
}

Screen::Screen(char* baseScreen)
{
    for (int row = 0; row < LCD_ROWS; ++row)
    {
        printRow(row, baseScreen + row * LCD_COLS);
    }
    pCurrent = buffer;
}

void Screen::begin(uint8_t cols, uint8_t rows)
{
  pinMode(_rs_pin, OUTPUT);
  pinMode(_enable_pin, OUTPUT);

  pinMode(_data_pins[0], OUTPUT);
  pinMode(_data_pins[1], OUTPUT);
  pinMode(_data_pins[2], OUTPUT);
  pinMode(_data_pins[3], OUTPUT);

  delayMicroseconds(50000);            // 15ms after 4.5V or 40ms after 2.7V
  lcdCommandNibble(INITIALIZE_CMD);
  delayMicroseconds(4500);             // >4.1ms
  lcdCommandNibble(INITIALIZE_CMD);
  delayMicroseconds(150);              // > 100
  lcdCommandNibble(INITIALIZE_CMD);
  
  lcdCommandNibble(SET_4BIT_CMD); // Set 4 bit interface
  
  lcdCommand(FUNC_SET_CMD | FUNCTION_ARGS); 
  lcdCommand(DISPLAY_CMD | DISPLAY_ARGS); 
  lcdCommand(CLEAR_CMD); 
  lcdCommand(ENTRY_MODE_CMD | ENTRY_CMD_ARGS); 
  
  ops = 0; // Used for critical sections
  
  lcdBuffers[0].ReadReady = false;
  lcdBuffers[0].WriteReady = true;
  lcdBuffers[0].pEnd = lcdBuffers[0].Buffer + (LCD_ROWS * LCD_COLS);
  lcdBuffers[0].pNext = &lcdBuffers[1];
  
  lcdBuffers[1].ReadReady = false;
  lcdBuffers[1].WriteReady = true;
  lcdBuffers[1].pEnd = lcdBuffers[1].Buffer + (LCD_ROWS * LCD_COLS);
  lcdBuffers[1].pNext = &lcdBuffers[0];
  
  interruptState = INTERRUPTSTATE_IDLE;
  
  pRead = &lcdBuffers[0];
  pReadCurrent = pRead->Buffer;
  readTick = 0x01;
  //readTicks initialized in the interrupt
  
  pWriteNext = &lcdBuffers[0];
  pWrite = 0;
  pWriteCurrent = 0;
    
  TimerMask(LcdInterruptNumber) |= _BV(TimerBit(LcdInterruptNumber));
  TimerA(LcdInterruptNumber) = TIMER_A_VALUE;
  TimerB(LcdInterruptNumber) = TIMER_B_VALUE;
  OutCmpA(LcdInterruptNumber) = INTERRUPT_IDLE;  
}


void Screen::setCursorRow(int row)
{
    
    switch (row)
    {
    case 0:
        pCurrent = buffer;
        break;

    case 1:
        pCurrent = buffer + 2 * LCD_COLS;
        break;

    case 2:
        pCurrent = buffer + LCD_COLS;
        break;

    case 3:
        pCurrent = buffer + 3 * LCD_COLS;
        break;
    }
}

void Screen::print(char ch)
{
    *pCurrent++ = ch;
}

void Screen::print(char *pString)
{
    while (*pString)
    {
        *pWriteCurrent++ = *pString++;
    }
}

void Screen::print(char *text, int count)
{
    while (*text && count)
    {
        *pCurrent++ = *text++;
        --count;
    }
    
    while (count)
    {
        *pCurrent++ = ' ';
        --count;
    }
}

void Screen::createChar(uint8_t location, uint8_t charmap[])   
{
    int i;
    
    lcdCommand(WRITE_CGRAM_CMD | ((location & 0x07) << 3)); // Lock location to 0-7 and multiple by 8
    
    for (i = 0; i < 8; ++i)
    {
        lcdSyncWrite(charmap[i]);
    }
}

void Screen::printRow(int row, char *text)
{
    setCursorRow(row);
    print(text, LCD_COLS);
}

bool Screen::display()
{
    return lcdWriteBuffer((uint8_t*)buffer);
}

// Print float with +123.4 format
void Screen::printFloat31(float value)
{
    int total;
    int digit;
    if (value >= 0)
    {
        *pCurrent++ = '+';
        value *= 0.01;
    }
    else
    {
        *pCurrent++ = '-';
        value *= -0.01;
    }

    digit = value;  

    *pCurrent++ = digit + '0'; 
    total = digit * 10; 
    value *= 10.0;
    digit = (int)value - total;

    *pCurrent++ = digit + '0';
    total += digit;
    total *= 10; 
    value *= 10.0;
    digit = (int)value - total;

    *pCurrent++ = digit + '0'; 
    total += digit;
    total *= 10; 
    value *= 10.0;
    digit = (int)value - total;

    *pCurrent++ = '.';

    *pCurrent++ = digit + '0';
}

//  Print float with +1234.5 format
void Screen::printFloat41(float value)
{
    int total;
    int digit;
    if (value >= 0)
    {
        *pCurrent++ = '+';
        value *= 0.001;
    }
    else
    {
        *pCurrent++ = '-';
        value *= -0.001;
    }

    digit = value;  

    *pCurrent++ = digit + '0'; 
    total = digit * 10; 
    value *= 10.0;
    digit = (int)value - total;

    *pCurrent++ = digit + '0';
    total += digit;
    total *= 10; 
    value *= 10.0;
    digit = (int)value - total;

    *pCurrent++ = digit + '0';
    total += digit;
    total *= 10; 
    value *= 10.0;
    digit = (int)value - total;

    *pCurrent++ = digit + '0'; 
    total += digit;
    total *= 10; 
    value *= 10.0;
    digit = (int)value - total;

    *pCurrent++ = '.';

    *pCurrent++ = digit + '0';
}

void Screen::print(int value)
{
    int digit;
    bool printing = false;
    
    if (value < 0)
    {
        *pCurrent++ = '-';
        value *= -1;
    }
    
    if (value > 10000 || printing)
    {
        digit = value / 10000;
        *pCurrent++ = digit + '0';
        value -= digit * 10000;
        printing = true;
    }
    if (value > 1000 || printing)
    {
        digit = value / 1000;
        *pCurrent++ = digit + '0';
        value -= digit * 1000;
        printing = true;
    }
    if (value > 100 || printing)
    {
        digit = value / 100;
        *pCurrent++ = digit + '0';
        value -= digit * 100;
        printing = true;
    }
    if (value > 10 || printing)
    {
        digit = value / 10;
        *pCurrent++ = digit + '0';
        value -= digit * 10;
        printing = true;
    }

    *pCurrent++ = value + '0';
}



void Screen::setCursor(int col, int row)
{
  pCurrent += col;
    switch (row)
    {
        case 0:
            pWriteCurrent = pWrite->Buffer + col;
            break;
            
        case 1:
            pWriteCurrent = pWrite->Buffer + 2 * LCD_COLS + col;
            break;
            
        case 2:
            pWriteCurrent = pWrite->Buffer + LCD_COLS + col;
            break;
            
        case 3:
            pWriteCurrent = pWrite->Buffer + 3 * LCD_COLS + col;
            break;
    }
}



void Screen::lcdPrint(uint8_t value)
{
    *pWriteCurrent++ = value;
}

bool Screen::lcdLockBuffer()
{
    pWrite = pWriteNext;

    CODE_ENTER; // Syncronize WriteReady with ISR
    
    if (!pWrite->WriteReady)
    {
        //Serial.print((int)interruptState);
        CODE_LEAVE;
        pWrite = 0;
        return false;
    }
    pWrite->WriteReady = false;
    
    CODE_LEAVE; // End syncronize WriteReady with ISR
    
    pWriteCurrent = pWrite->Buffer;
    memset(pWriteCurrent, 0x20, LCD_ROWS * LCD_COLS); 
    return true;
}

void Screen::lcdWriteBuffer()
{
    CODE_ENTER; // Synchronize ReadReady and writeState with ISR
    
    pWrite->ReadReady = true;
    if (interruptState == INTERRUPTSTATE_IDLE)
    {
        interruptState = INTERRUPTSTATE_CMD_HI1;
    }
    
    CODE_LEAVE; // End synchronize ReadReady and writeState with ISR

    pWriteNext = pWrite->pNext;
    pWrite = 0;
    pWriteCurrent = 0;
}

bool Screen::lcdWriteBuffer(uint8_t *pBuffer)
{
    CODE_ENTER; // Syncronize WriteReady with ISR
    
    if (!pWriteNext->WriteReady)
    {
        //Serial.print((int)interruptState);
        CODE_LEAVE;
        return false;
    }
    pWriteNext->WriteReady = false;
    
    CODE_LEAVE; // End syncronize WriteReady with ISR
    
    memcpy(pWriteNext->Buffer, pBuffer, LCD_ROWS * LCD_COLS); 
    
    CODE_ENTER; // Synchronize ReadReady and writeState with ISR
    
    pWriteNext->ReadReady = true;
    if (interruptState == INTERRUPTSTATE_IDLE)
    {
        interruptState = INTERRUPTSTATE_CMD_HI1;
    }
    
    CODE_LEAVE; // End synchronize ReadReady and writeState with ISR

    pWriteNext = pWriteNext->pNext;
    
    return true;
}

/************************************************************************

Internal functions

************************************************************************/

#ifdef LCD_DEBUG

void Screen::DebugState(struct LCD_BUFFER *pBuffer)
{
    int i,j, k;
    Serial.print("Buffer=");
    Serial.println((int)pBuffer);
    if (pBuffer == 0)
    {
        return;
    }
    Serial.print("WR=");
    Serial.println((int)pBuffer->WriteReady);
    Serial.print("RR=");
    Serial.println((int)pBuffer->ReadReady);
    for (i = 0, k = 0; i < LCD_ROWS; ++i)
    {
        for (j = 0; j <  LCD_COLS; ++j)
        {
            Serial.print(pBuffer->Buffer[k],HEX);
            Serial.print(' ');
            ++k;
        }
        Serial.println();
    }
    Serial.print("e=");
    Serial.println((int)pBuffer->pEnd);
    Serial.print("n=");
    Serial.println((int)pBuffer->pNext);
}

void Screen::DebugState()
{
    cli();
    Serial.println();
    Serial.print("pRead ");
    DebugState(pRead);
    Serial.print("pReadCurrent=");
    Serial.println((int)pReadCurrent);
    Serial.print("readTick=");
    Serial.println((int)readTick);
    
    Serial.print("pWrite ");
    DebugState(pWrite);
    Serial.print("pWriteNext ");
    DebugState(pWriteNext);
    Serial.print("pWriteCurrent=");
    Serial.println((int)pWriteCurrent);
    
    Serial.print("ops=");
    Serial.println((int)ops);

    Serial.print("interruptState=");
    Serial.println((int)interruptState);
    sei();
}

#endif

void Screen::lcdCommand(uint8_t value)
{
    digitalWrite(_rs_pin, LOW);
    lcdSyncWrite(value);
    digitalWrite(_rs_pin, HIGH);
}

void Screen::lcdCommandNibble(uint8_t value)
{
    digitalWrite(_rs_pin, LOW);
    lcdSyncWriteNibble(value);
    digitalWrite(_rs_pin, HIGH);
}

void Screen::lcdSyncWrite(uint8_t value)
{
    lcdSyncWriteNibble(value >> 4);
    lcdSyncWriteNibble(value);
}

void Screen::lcdSyncWriteNibble(uint8_t value)
{
    lcdSetDataBits(value);        
    
    digitalWrite(_enable_pin, HIGH);
    
    delayMicroseconds(1);
    
    digitalWrite(_enable_pin, LOW);
    
    delayMicroseconds(50);
}






void interruptTransmit()
{
    uint8_t writeByte;
    ISR_ENTER;
    
    // We know the user isn't altering buffer state
    // and we will complete before they execute again
    if(Screen::interruptState == INTERRUPTSTATE_IDLE)
    {
        OutCmpA(LcdInterruptNumber) = INTERRUPT_IDLE; // Minimize overhead while still reponsive
        return;
    }
    
    switch (Screen::interruptState)
    {
        case INTERRUPTSTATE_CMD_HI1:
            OutCmpA(LcdInterruptNumber) = INTERRUPT_BUSY; // Fire faster while updating
            digitalWrite(_rs_pin, LOW);
            Screen::lcdSetDataBits(HOME_CURSOR_CMD >> 4);
            digitalWrite(_enable_pin, HIGH);
            
            Screen::interruptState = INTERRUPTSTATE_CMD_LO1;
            return;
            
        case INTERRUPTSTATE_CMD_LO1:
            digitalWrite(_enable_pin, LOW);
            
            Screen::interruptState = INTERRUPTSTATE_CMD_HI2;
            return;
            
        case INTERRUPTSTATE_CMD_HI2:
            Screen::lcdSetDataBits(HOME_CURSOR_CMD);
            digitalWrite(_enable_pin, HIGH);
            
            Screen::interruptState = INTERRUPTSTATE_CMD_LO2;
            return;
           
        case INTERRUPTSTATE_CMD_LO2:
            digitalWrite(_enable_pin, LOW);
            
            Screen::interruptState = INTERRUPTSTATE_CMD_END;
            return;
            
        case INTERRUPTSTATE_CMD_END:
            digitalWrite(_rs_pin, HIGH);
            
            Screen::interruptState = INTERRUPTSTATE_E_GOHI;
            return;
            
        case INTERRUPTSTATE_E_GOHI:
            if (Screen::readTick & 0x01)
            {
                writeByte = (*Screen::pReadCurrent >> 4);
            }
            else
            {
                writeByte = *Screen::pReadCurrent;
                ++Screen::pReadCurrent;
            }
            ++Screen::readTick;
            Screen::lcdSetDataBits(writeByte);
            digitalWrite(_enable_pin, HIGH);
            
            Screen::interruptState = INTERRUPTSTATE_E_GOLO;
            return;
            
        case INTERRUPTSTATE_E_GOLO:
            digitalWrite(_enable_pin, LOW);
            
            if(Screen::pReadCurrent >= Screen::pRead->pEnd || Screen::pRead->pNext->ReadReady)
            {
                if (!(Screen::readTick & 0x01))
                {
                    // Write a full byte before switching
                    Screen::interruptState = INTERRUPTSTATE_E_GOHI;
                    return;
                }
                Screen::pRead->WriteReady = true;
                Screen::pRead->ReadReady = false;
                
                Screen::pRead = Screen::pRead->pNext;
                Screen::pReadCurrent = Screen::pRead->Buffer;
                Screen::readTick = 0x01;
              
                // If we have data write it else go to idle state
                Screen::interruptState = (Screen::pRead->ReadReady) ? INTERRUPTSTATE_CMD_HI1 : INTERRUPTSTATE_IDLE;
            }
            else
            {
                Screen::interruptState = INTERRUPTSTATE_E_GOHI;
            }  
            return;
    }
}



ISR(TIMER4_COMPA_vect)
{
    interruptTransmit();
}