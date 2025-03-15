/*
 * Adafruit Feather M4 CAN --  LCC Clock Display
 *  By Dana Zimmerli, Z System Designs
 *  Based on the Adafruit CAN Receiuver Example
 *    Needs Adafruit libraries as explained in the Adafruit 
 *       https://learn.adafruit.com/adafruit-feather-m4-can-express/arduino-ide-setup
 *    Implements Events from the NMRA LCC Broadcast Time Protocol
 */

#include <CANSAME5x.h>

CANSAME5x CAN;   // Create the CAN object

#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h> // Included for the Display
#include "Adafruit_LEDBackpack.h"

Adafruit_7segment matrix = Adafruit_7segment();
//#define zDebug  //  Sends debug to the Arduino Serial Monitor
#define zDebug2   //  Send the Display data to the Serial Monitor
//#define zDebugShow
//  Broadcast Time Event Initial bytes
byte BcastEvent[5] = {0x01, 0x01, 0, 0, 0x01};
byte event[8];
// Tiime variables -- to keep track of millis
unsigned long nowMilli, nextMilli, incrMilli ;
//  Time variables to display time
unsigned long buffer;
 int fastRate = 4; // Rate is 4 times desired rate
 int showHour = 0;  int showMin = 0;
 bool drawDots;
 unsigned int frt;  bool future = true;
//  Control --  Running
bool clkRun = false;  // Clcock is stopped
bool displayIt = false;
int clk24;  //  24 hour display

void setup() {
  #ifdef zDebugShow 
  Serial.begin(115200);
  while (!Serial) delay(10);
   Serial.println("LCC Clock Display");
 #endif

 if (!matrix.begin(0x70))  {  //  Check if display is okay
  Serial.println("7 segment/I2C Failed");
 }

  pinMode(PIN_CAN_STANDBY, OUTPUT);
  digitalWrite(PIN_CAN_STANDBY, false); // turn off STANDBY
  pinMode(PIN_CAN_BOOSTEN, OUTPUT);
  digitalWrite(PIN_CAN_BOOSTEN, true); // turn on booster

  // start the CAN bus at 125 kbps for LCC
  if (!CAN.begin(125000)) {
   #ifdef zDebug
    Serial.println("Starting CAN failed!");
    #endif
    while (1) delay(10);  //  Loop if failed
  }
  #ifdef zDebug
  Serial.println("Starting CAN!");
  #endif
  //  Decide if Fast Clock or Real Time Clock
  pinMode(4, INPUT);   //  use when reading to decider which
  // if read is false, Fast Clock, else Real Time
  pinMode(5, INPUT);  // Ckoose 12 or 24 clock
 if(digitalRead(5) == HIGH)  clk24 = 24;
  else clk24 = 12;
  //  get current time data
  nowMilli = millis();
  incrMilli = 240000/fastRate;  //  Number of ms until next 
  nextMilli = nowMilli+incrMilli;
  #ifdef zDebug
   Serial.print(" Incr ");  Serial.print(incrMilli);
   Serial.print("Now ");  Serial.print(nowMilli);   
   Serial.print(" Next ");   Serial.println(nextMilli);
  #endif
}

void loop() {
  // try to parse packet
  int packetSize = CAN.parsePacket();

  if (packetSize == 8) {
    // received a packet with a potential event
    //Serial.print("Received ");

    if (CAN.packetExtended()) {
      //Serial.print("extended ");
    }
#ifdef zDebug
    Serial.print("packet with id 0x");
    Serial.print(CAN.packetId(), HEX);
 
    Serial.print(" and length ");
    Serial.println(packetSize);
#endif
      //  Get event from CAN
      bool timeEvent = true;
      for(int i = 0; i < 5; i++)  {
        event[i] = CAN.read() ;
        if (event[i] != BcastEvent[i]) timeEvent = false;
      }  //  This loop checks the well known event part

      event[5] = CAN.read() ;  // Clock type - Real Time or Fast
      if((digitalRead(4) == LOW) && (event[5] != 0)) 
             timeEvent = false;
      
      //  Now read the type of event 
      event[6] = CAN.read();
      event[7] = CAN.read(); 
//  Print the ewvent if debug
#ifdef zDebug
      for(int j = 0; j< 8; j++) {
        Serial.print(event[j], HEX);
        Serial.print(" . ");
      }
        //  Check if Fast or real time
      if(event[5] == 1) Serial.print(" RealTime ");
        else Serial.print(" Fast Time ");
      Serial.println();
    #endif

    if(timeEvent)  {
      #ifdef zDebug
      Serial.println("Found Broadcast Time FaST");
      #endif
      int k = event[6] >> 4;
      switch (k) {    //  Switch for event type
        case 0:
        case 1:    // Time in bytes 7 and 8
          showHour = event[6];  showMin = event[7];
          displayIt = true;
          nowMilli = millis();  nextMilli = nowMilli + incrMilli;
         // writeIt = true;
          break;
        case 4: // Rate Event -- decode Rate
         frt = event[6]; 
         //Serial.println(frt, HEX);
         frt= frt & 0x0F ;
         //Serial.println(frt, HEX);
         frt = frt << 8 ;
         frt = frt | event[7];
         if((event[6] & 0x0F)) {  //  Check for negative rate
          frt = frt| 0xFFFFF000  ; 
          //Serial.println(frt , HEX);
          future = false;  
         }  else future = true;
         //Serial.println(frt, HEX);
         //Serial.println(frt);
          fastRate = frt ;  fastRate = abs(fastRate);
          //Serial.println(fastRate, HEX);
          //Serial.println(fastRate);
          incrMilli = 240000/fastRate;  // Modiufied rate increment
          break;
        case 15:  //  Start and Pause evenmts
          if(event[7] == 1)  clkRun = false;
            else clkRun = true;
          break;
        default:
          break;
      }
      #ifdef zDebug
        Serial.print(showHour); Serial.print(":");
        Serial.print(showMin);  Serial.print(" ");
        Serial.println(fastRate, DEC);
      #endif
      //  Next update clock if running
    }
  }

      if (clkRun )  {
        nowMilli = millis();
        if (nowMilli > nextMilli)  {
          nextMilli = nowMilli + incrMilli;
          if(future)  { //  Fortward counting
            if(++showMin > 59)  {
            showMin = 0;
            if (++showHour > 23) 
              showHour = 0;
          }
          }  else  {    // backward counting
            if(--showMin < 0)  {
              showMin = 59;
              if(--showHour < 0)
                showHour = 23;
            }
          }
          displayIt = true;
        }
      }
        if (displayIt)  {
        //  Display the time
        displayIt = false;
        int dispHour = showHour;
        if ((clk24 == 12) && (showHour > 12)) dispHour = showHour - 12;
        #ifdef zDebug2
          Serial.print("*** 7 Segment Display = ");
          Serial.print(dispHour, DEC);  Serial.print(":");
          Serial.println(showMin);
          #endif
  
  drawDots = true; 
     // Display example for backpack
     Serial.println("7 segment drive");
    matrix.writeDigitNum(0, dispHour / 10);
    matrix.writeDigitNum(1, dispHour % 10);
    matrix.drawColon(drawDots);
    matrix.writeDigitNum(3, showMin / 10, true);
    matrix.writeDigitNum(4, showMin % 10);
   matrix.writeDisplay();
        }

 
}