#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>
#include "WProgram.h"
#include "fastio.h"

/*
    LCD pin configuration currently only supports 4bit and no R/W pin
    Set the defines below to match your configuration
*/
#define LCD_RS_PIN     12    // Register Select
#define LCD_E_PIN      10    // Enable pin

#define LCD_DB4_PIN    5     // Data pin 4
#define LCD_DB5_PIN    4     // Data pin 5
#define LCD_DB6_PIN    3     // Data pin 6
#define LCD_DB7_PIN    2     // Data pin 7

/*
    Currently only suports 20x4 displays
*/
#define LCD_COLS       20    
#define LCD_ROWS       4

#define FUNCTION_ARGS  FUNCTION_4BIT | FUNCTION_MULTILINE | FUNCTION_5x8
#define DISPLAY_ARGS   DISPLAY_DISPLAY_ON | DISPLAY_CURSOR_OFF | DISPLAY_BLINK_OFF
#define ENTRY_CMD_ARGS ENTRY_LEFTTORIGHT | ENTRY_SHIFT_OFF


struct LCD_BUFFER
{
    volatile boolean ReadReady;
    volatile boolean WriteReady;
    uint8_t Buffer[LCD_ROWS * LCD_COLS];
    uint8_t *pEnd;
    LCD_BUFFER *pNext;
};

class Screen
{
public:
    Screen();
    Screen(char* baseScreen);

    char* getCursor() const { return pCurrent; }
    void setCursor(char *pCursor) { pCurrent = pCursor; }
    void setCursor(int col, int row);
    void setCursorRow(int row);

    void print(char ch);
    void print(int value);
    void print(char *text);
    void print(char *text, int count);
    void printRow(int row, char *text);

    void printFloat31(float value);
    void printFloat41(float value);

    bool display();
    
    
    void begin(uint8_t cols, uint8_t rows);
//replicate LiquidCrystal.cpp
  void clear();
  void home();

  void noDisplay();
  void noBlink();
  void blink();
  void noCursor();
  void cursor();
  void scrollDisplayLeft();
  void scrollDisplayRight();
  void leftToRight();
  void rightToLeft();
  void autoscroll();
  void noAutoscroll();

  void createChar(uint8_t, uint8_t[]);
  void setCursor(uint8_t, uint8_t); 
  void write(uint8_t);
  void command(uint8_t);

private:
    char buffer[LCD_ROWS * LCD_COLS];
    char* pCurrent;
    
  
public:
    
    // lcdLockBuffer - Called to setup a buffer to write to returns true on success and should
    // be followed by a matching call to lcdWriteBuffer(). A return value of false means there
    // isn't a buffer free to write to.
    bool lcdLockBuffer();

    // lcdWriteBuffer - Called at the end of an update to prepare the result for display.
    void lcdWriteBuffer();




    // Print a byte to the buffer and advance the current write postition.
    void lcdPrint(uint8_t value);

    // Lock buffer and write it in one call returns true on success, false if no buffer
    // was available
    bool lcdWriteBuffer(uint8_t *pBuffer);

    //internal routines
    void lcdCommandNibble(uint8_t value);
    void lcdCommand(uint8_t value);
    void lcdSyncWrite(uint8_t value);
    void lcdSyncWriteNibble(uint8_t value);
   
    static inline void lcdSetDataBits(uint8_t nibble)
    {         
        WRITE(LCD_DB4_PIN, (nibble & _BV(0)) ? HIGH : LOW );
        WRITE(LCD_DB5_PIN, (nibble & _BV(1)) ? HIGH : LOW );
        WRITE(LCD_DB6_PIN, (nibble & _BV(2)) ? HIGH : LOW );
        WRITE(LCD_DB7_PIN, (nibble & _BV(3)) ? HIGH : LOW );
    }
};






#endif

