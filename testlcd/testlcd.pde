#include "LcdDriver.h"
#include "Screen.h"

int count;

byte Degree[8] =
{
    B01100,
    B10010,
    B10010,
    B01100,
    B00000,
    B00000,
    B00000,
    B00000
};

byte Thermometer[8] =
{
    B00100,
    B01010,
    B01010,
    B01010,
    B01010,
    B10001,
    B10001,
    B01110
};

byte Screen1[] = 
{
    '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', 
    '2', '2', '2', '2', '2', '2', '2', '2', '2', '2', '2', '2', '2', '2', '2', '2', '2', '2', '2', '2', 
    '1', '1', '1', '1', '1', '1', '1', '1', '1', '1', '1', '1', '1', '1', '1', '1', '1', '1', '1', '1', 
    '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', 
};

byte Screen2[] = 
{
    '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', 
    '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', 
    '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', 
    '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', 
};

//Screen welcomeScreen;

Screen welcomeScreen(
       //12345678901234567890
        "Welcome to Screens.."
        "Second row          "
        "Third row           "
        "Forth row           "
    );
//Screen welcomeScreen2(
//       //12345678901234567890
//        "Welcome to Screens. "
//        "Second row          "
//        "Third row           "
//        "Forth row           "
//    );

//#define DBG_STATE 

#ifndef DBG_STATE
#define DebugState() 
#endif

#define LCD_CMD_COUNT    5
#define LCD_BIT_COUNT    2
#define LCD_SCR_COUNT    (LCD_ROWS * LCD_COLS * LCD_BIT_COUNT + LCD_CMD_COUNT)

void TestTwoBuffersWaiting();
void TestCustomChar();
void TestWriteBuffer();
void TestScreenDisplay();

void setup()
{
    count = 0;
    Serial.begin(28800);
    Serial.print("Startup...\n");

    pinMode(13, OUTPUT); 

    delay(1000);
    
    TestScreenDisplay();
    while(1);
   
    DebugState();
    while(!lcdLockBuffer())
    {
        delay(1);
    }
}

void loop()
{
    int i;
    unsigned long entry;
    ++count;
    if (count >= 10) count = 0;
    entry = millis();

    if(lcdLockBuffer())
    {
        uint8_t val = 0x30 + count;
        welcomeScreen.setCursor(0,0);
        for (i = 0; i < 20; ++i)
        {
            lcdPrint(val);
        }
        welcomeScreen.setCursor(0,1);
        for (i = 0; i < 20; ++i)
        {
            lcdPrint(0x30 + count);
        }
        welcomeScreen.setCursor(0,2);
        for (i = 0; i < 20; ++i)
        {
            lcdPrint(0x30 + count);
        }
        welcomeScreen.setCursor(0,3);
        for (i = 0; i < 20; ++i)
        {
            lcdPrint(0x30 + count);
        }
        lcdWriteBuffer();
    }
    else
    {
        Serial.println("Skipped..");
    }

    entry = millis() - entry;
    
    delay(150 - entry);
}



void TestCustomChar()
{
  welcomeScreen.createChar(1, Degree);
  welcomeScreen.createChar(2, Thermometer);
  
    if(lcdLockBuffer())
    {
        welcomeScreen.setCursor(0,0);
        lcdPrint(1);
        lcdPrint(" Degree.");
        
        welcomeScreen.setCursor(0,1);
        lcdPrint(2);
        lcdPrint(" Thermometer.");
        lcdWriteBuffer();
    }
    
    while(true);
}

void TestWriteBuffer()
{
    lcdWriteBuffer(Screen1);
    
    while(true);
}

void TestScreenDisplay()
{
    welcomeScreen.begin(0,0);
    welcomeScreen.display();
    
    delay(1000);
    
    welcomeScreen.setCursor(11,1);
    welcomeScreen.print(-32123);
    
    welcomeScreen.setCursor(11,2);
    welcomeScreen.print(32123);
    
    welcomeScreen.setCursor(11,3);
    welcomeScreen.print(1);
    
    welcomeScreen.display();
    
    while(true);
}
