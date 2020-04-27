/**************************************************************************************************************************************
**
** Project: ARINC 568 DME (DME-40) Simulator 
** File:    ARINC568TX.INO
** Purpose: Main SW File
** 
** (C) 2019 by Daniele Sgroi - daniele.sgroi@gmail.com
**
** VERSION:
**  - December 08, 2019 - ALPHA 0.1 - D. Sgroi
**
** TARGET HW:
** - ATMEGA 328P @ 16 MHz / 5V (Arduino Pro Mini)
** - FTB-40 interface board
**
** SOFTWARE:
** - Arduino 1.8.9+ IDE
** 
**************************************************************************************************************************************/

//#define _DEBUG_                          // enable verbose debug messages on console   

/**************************************************************************************************************************************
** DEFINES
**************************************************************************************************************************************/

#define VERSION                                 "1.0" // SW Version
#define DEBUG_BAUD                             115200 // Serial Port Baud rate
#define DELAY_CYCLES(n) __builtin_avr_delay_cycles(n) // 1 cycle @ 16 MHz = 62.5 nSec

/**************************************************************************************************************************************
** HW DEFINES - ARDUINO PRO MINI PINOUT
**************************************************************************************************************************************/

#define RXD                0
#define TXD                1
//                         2
//                         3
//                         4
//                         5
//                         6
//                         7
//                         8 // PB1
//                         9 // PB0
#define CLK_561_PIN       10 // PB2
#define DTA_561_PIN       11 // PB3
#define SYN_561_PIN       12 // PB4
//#define LED_BUILTIN     13 // PB5, no need to redefine LED_BUILTIN
//                        A0
//                        A1
//                        A2
//                        A3
#define I2C_SDA           A4
#define I2C_SCL           A5
//                        A6
//                        A7

/**************************************************************************************************************************************
** ARINC 561/568 DEFINES AND TYPES
**************************************************************************************************************************************/

#define CLK_561          PB2 // Pin 10
#define DTA_561          PB3 // Pin 11
#define SYN_561          PB4 // Pin 12

union t56xDmeWord {
  struct {
     unsigned  char ucLabel        :  8; // LSB - 201 octal = 129 decimal
     unsigned  char ucPad          :  4; // always 0
     unsigned  char ucHundredths   :  4; //   0.01 NM
     unsigned  char ucTenths       :  4; //   0.1  NM
     unsigned  char ucUnits        :  4; //   1    NM
     unsigned  char ucTens         :  4; //  10    NM
     unsigned  char ucHundreds     :  2; // 100    NM
     unsigned  char ucStatus       :  2; // 0 = Valid, 1 = Invalid, 2 = Test, 3 = Undefined 
  } tFields;
  unsigned long int ul56xData      : 32; // must be exact size of tFields struct above
};

/**************************************************************************************************************************************
** GLOBALS
**************************************************************************************************************************************/

t56xDmeWord DmeDistance;

/**************************************************************************************************************************************
** A561Out
**
** Shift Out 32 bit data in ARINC 561/658 format
** LSB first
** sync @ b8
** clock @ 11,5 kHz nominal bitrate (87 usec bit time)
**
** CLK_561           10 // PB2 // PORTB |= _BV(PB2); // CLK_561 High // PORTB &= ~_BV(PB2); // CLK_561 Low
** DTA_561           11 // PB3 // PORTB |= _BV(PB3); // DTA_561 High // PORTB &= ~_BV(PB3); // DTA_561 Low 
** SYN_561           12 // PB4 // PORTB |= _BV(PB4); // SYN_561 High // PORTB &= ~_BV(PB4); // SYN_561 Low
**
**************************************************************************************************************************************/

void A56xOut(uint32_t val) {
         
    noInterrupts();                                                     // Begin of time critical section
    //PORTB &= ~_BV(SYN_561) & ~_BV(DTA_561) & ~_BV(CLK_561);           // All Low
    for (uint8_t i = 0; i < 32; i++)  {
        PORTB |= _BV(CLK_561) | _BV(SYN_561);                           // SYN_561 High, CLK_561 High
        (val & (0x1)) ? PORTB |= _BV(DTA_561) : PORTB &= ~_BV(DTA_561); // DTA_561 bit, LSB shiftout         
        DELAY_CYCLES(685);                                              // Experimental fine tuning, 1 cycle = 0.0625 uSec (685)
        (i == 7) ? PORTB &= ~_BV(SYN_561) & ~_BV(CLK_561) :             // SYN_561 Low,  CLK_561 Low
                   PORTB &=                 ~_BV(CLK_561);              // CLK_561 Low
        DELAY_CYCLES(680);                                              // Experimental fine tuning, 1 cycle = 0.0625 uSec (680)
        val = val >> 1;                                                 // LSB shiftout 
    }
    DELAY_CYCLES(5);                                                    // Experimental fine tuning, 1 cycle = 0.0625 uSec (5)
    PORTB &= ~_BV(SYN_561) & ~_BV(DTA_561) & ~_BV(CLK_561);             // All Low
    interrupts();                                                       // End of time critical section

}

/**************************************************************************************************************************************
** setup
**
** run once at startup
**
**************************************************************************************************************************************/

void setup() {
  // setup debug serial port (FTDI)
  Serial.begin(DEBUG_BAUD, SERIAL_8N1);
  while (!Serial);       // Wait for the DEBUG serial port to come online

  // Print Version
  Serial.print(F("ARINC568TX - V"));
  Serial.print(VERSION);
  Serial.println(F("(C)2019 by daniele.sgroi@gmail.com"));

  // Init discretes
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(CLK_561_PIN, OUTPUT);
  pinMode(DTA_561_PIN, OUTPUT);
  pinMode(SYN_561_PIN, OUTPUT);
 
  // SET Output Defaults
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(CLK_561_PIN, LOW);
  digitalWrite(DTA_561_PIN, LOW);
  digitalWrite(SYN_561_PIN, LOW);
  
  // Init DME distance word with 123.45 NM valid

  DmeDistance.tFields.ucLabel      = 0201; // ARINC 568 DME Distance Label (label 001 for ARINC 561 LRN)
  DmeDistance.tFields.ucPad        = 0;
  DmeDistance.tFields.ucHundredths = 5;    //   0.01 NM
  DmeDistance.tFields.ucTenths     = 4;    //   0.1  NM
  DmeDistance.tFields.ucUnits      = 3;    //   1    NM
  DmeDistance.tFields.ucTens       = 2;    //  10    NM
  DmeDistance.tFields.ucHundreds   = 1;    // 100    NM
  DmeDistance.tFields.ucStatus     = 0;    // 0 = Valid, 1 = Invalid, 2 = Test, 3 = Undefined 

}

/**************************************************************************************************************************************
** LOOP
**
** Run continuosly
**
**************************************************************************************************************************************/

void loop() {

  digitalWrite(LED_BUILTIN, HIGH);     

  // TODO: read rotary switch, set DME data accordingly

  DmeDistance.tFields.ucLabel      = 0201; // ARINC 568 DME Distance Label (label 001 for ARINC 561 LRN)
  DmeDistance.tFields.ucPad        = 0;
  DmeDistance.tFields.ucHundredths = 5;    //   0.01 NM
  DmeDistance.tFields.ucTenths     = 4;    //   0.1  NM
  DmeDistance.tFields.ucUnits      = 3;    //   1    NM
  DmeDistance.tFields.ucTens       = 2;    //  10    NM
  DmeDistance.tFields.ucHundreds   = 1;    // 100    NM
  DmeDistance.tFields.ucStatus     = 0;    // 0 = Valid, 1 = Invalid, 2 = Test, 3 = Undefined 

  Serial.print(F("ARINC 56x Word: 0x"));
  Serial.println(DmeDistance.ul56xData, HEX); 
  
  A56xOut(DmeDistance.ul56xData);

  digitalWrite(LED_BUILTIN, LOW);

  delay(199);

}

/**************************************************************************************************************************************
** EOF
**************************************************************************************************************************************/
