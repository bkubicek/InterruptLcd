#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>
#include "WProgram.h"

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

private:
    char buffer[LCD_ROWS * LCD_COLS];
    char* pCurrent;
};


// lcdInit - Needs to be called once to setup the LCD display and interrupt before the 
// display can be used.
void lcdInit();

// Create a custom character in one of the first 8 places
// NOTE: must be called after lcdInit and before anyother methods
void lcdCreateChar(uint8_t location, uint8_t charmap[]);

// lcdLockBuffer - Called to setup a buffer to write to returns true on success and should
// be followed by a matching call to lcdWriteBuffer(). A return value of false means there
// isn't a buffer free to write to.
bool lcdLockBuffer();

// lcdWriteBuffer - Called at the end of an update to prepare the result for display.
void lcdWriteBuffer();

// Set the current write position in the buffer.
void lcdSetCursor(int col, int row);

// Print a nul treminated string of characters caller should limit to the remaining length 
// of the current row. This method advances the current write postition.
void lcdPrint(char* pString);

// Print a byte to the buffer and advance the current write postition.
void lcdPrint(uint8_t value);

// Lock buffer and write it in one call returns true on success, false if no buffer
// was available
bool lcdWriteBuffer(uint8_t *pBuffer);

#endif
