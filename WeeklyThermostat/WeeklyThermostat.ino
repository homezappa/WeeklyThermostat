#include <SPI.h>
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include <MyButton.h>
#include <RTClib.h>
#include <SimpleDHT.h>
#include <EEPROM.h>

/* *************************************************************** */
/* ********************* DEFINES ********************************* */
/* *************************************************************** */


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define STATUS_MENU       0x01
#define STATUS_RUNNING    0x02
#define STATUS_SET_TIME   0x03
#define STATUS_SET_HH     0x04
#define STATUS_SET_MM     0x05
#define STATUS_SET_DD     0x06
#define STATUS_SET_MO     0x07
#define STATUS_SET_YY     0x08
#define STATUS_SET_TEMP   0x09
#define STATUS_SET_TEMPH  0x0A
#define STATUS_SET_TEMPL  0x0B
#define STATUS_SET_CAL    0x0C
#define STATUS_SET_DOW    0x0D
#define STATUS_SET_HOURS  0x0E
// menu timeout 20 secs
#define MENU_TIMEOUT  20000

#define MENU_NONE         -1

#define PIN_BACK          5
#define PIN_UP            4
#define PIN_DOWN          3
#define PIN_NEXT          2

#define PIN_DHT11   10

#define PIN_RELEAIS 9

#define MIN_TEMPH         18
#define MAX_TEMPH         24
#define MIN_TEMPL         10
#define MAX_TEMPL         18

#define MY_TAG      30550  

// Define proper RST_PIN if required.
#define RST_PIN 4
#define I2C_ADDRESS 0x3C

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)


/* *************************************************************** */
/* ***************** GLOBAL VARS ********************************* */
/* *************************************************************** */

unsigned long timeStartMenu = 0L;
unsigned long timeStartRelais = 0L;
unsigned long timeStartTemp   = 0L;

/*********************************************************************/
// Variabili lettura temperatura DHT11
byte temperature    = 0;
byte humidity       = 0;
byte humidity2      = 0;
byte temperature2   = 0;
byte data[40]       = {0};

/********************************************************************/
// Struttura di configurazione
struct Config {
  int  TAG;           // integer to know if I saved before on eeprom
  int  MinConfigTemp;       // Low Temp ...
  int  MaxConfigTemp;       // High Temp 
  char arDays[7][3] = {
  // 00,01,02,03,04,05,06,07,  08,09,10,11,12,13,14,15,  16,17,18,19,20,21,22,23 
      0B00000010, 0B00001100, 0B01111111, // Lunedi
      0B00000010, 0B00001100, 0B01111111, // Martedi
      0B00000010, 0B00001100, 0B01111111, // Mercoledi
      0B00000010, 0B00001100, 0B01111111, // Giovedi
      0B00000010, 0B00001100, 0B01111111, // Venerdi
      0B00000000, 0B00001111, 0B11111111, // Sabato
      0B00000000, 0B00001111, 0B11111111, // DOmenica
  };
};

Config config;

int TAG_READ;

int  previousSec = -1;
char buffer[32];  
int  programStatus = STATUS_RUNNING;

int  menuStatus     = 0;
int  menuTempStatus = 0;

int  tempHigh       = 20;
int  tempLow        = 15;

char arWeek[7][4]      = { "Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab" };
char arWeekL[7][12]    = { "Domenica", "Lunedi", "Martedi", "Mercoledi", "Giovedi", "Venerdi", "Sabato" };
char arMonth[12][4]    = { "Gen", "Feb", "Mar", "Apr", "Mag", "Giu", "Lug", "Ago", "Set", "Ott", "Nov", "Dic" };
char arMainMenu[4][12] = { "TEMP.ALTA", "TEMP.BASSA", "GIORNO", "ORARI" };

int  currentDayOfWeek=0;

/* *************************************************************** */
/* ******************** OBJECTS ********************************** */
/* *************************************************************** */

SSD1306AsciiWire display;

MyButton BottoneMenu;
MyButton BottoneIndietro;
MyButton BottoneSu;
MyButton BottoneGiu;

RTC_DS1307              RTC;

DateTime                now;

SimpleDHT11             dht11;



/* SETUP */
void setup() {
  int _tempTAG;

  Wire.begin();
  Wire.setClock(400000L);
  pinMode(PIN_RELEAIS,OUTPUT);
  Serial.begin(9600);
  Serial.println("Start");

#if RST_PIN >= 0
  display.begin(&Adafruit128x32, I2C_ADDRESS, RST_PIN);
#else // RST_PIN >= 0
  display.begin(&Adafruit128x32, I2C_ADDRESS);
#endif // RST_PIN >= 0

  RTC.begin();
  if (! RTC.isrunning()) {
    Serial.println("RTC is NOT running!");
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }

  // Initialize buttons
  BottoneMenu.begin(PIN_NEXT, INPUT_PULLUP);
  BottoneIndietro.begin(PIN_BACK, INPUT_PULLUP);
  BottoneSu.begin(PIN_UP, INPUT_PULLUP);
  BottoneGiu.begin(PIN_DOWN, INPUT_PULLUP);

/* *************** Read Config from EEprom ************** */
  EEPROM.get(0,_tempTAG);
  if (_tempTAG != MY_TAG ) {
    config.TAG = MY_TAG;
    config.MinConfigTemp = 16;
    config.MaxConfigTemp = 22;
    EEPROM.put(0, config);
    Serial.println("Saved defaults to memory!");
  } else {
    EEPROM.get(0, config);
    Serial.println("Got values from memory!");
  }
/* ****************** End Read Config *************** */  
/* ************ Initialize temps ******************** */
  tempHigh =   config.MaxConfigTemp;
  tempLow  =   config.MinConfigTemp;
  
  initScreen();
  timeStartRelais = millis();

  for (int i=0; i<7; i++) {
    Serial.print(arWeek[i]); Serial.print(":");
    for(int h=0; h<24;h++) {
      Serial.print((readStatus(i,h)==1)?"1":"0");
    }
    Serial.println("");
  }
  Serial.println("-----");

}

void loop() {
  BottoneMenu.read();
  BottoneIndietro.read();
  BottoneSu.read();
  BottoneGiu.read();
  now = RTC.now();
  int _dow,_h,_m,_s,_da,_mo,_ye;
  _dow = now.dayOfTheWeek();
  _h = now.hour();
  _m = now.minute();
  _s = now.second();
  _da = now.day();
  _mo = now.month();
  _ye = now.year();
/*  
  Serial.print(digitalRead(PIN_BACK));
  Serial.print(digitalRead(PIN_UP));
  Serial.print(digitalRead(PIN_DOWN));
  Serial.println(digitalRead(PIN_NEXT));
*/
  switch(programStatus){
    case STATUS_RUNNING:
      if (BottoneMenu.wasLongPushed()==true) {
        timeStartMenu=millis();
        programStatus = STATUS_MENU;
        menuStatus = 0;
        doMenu("Configurazione",arMainMenu, 4, menuStatus);
      } else {
        if (BottoneSu.wasLongPushed()==true) {
          setOn(_dow,_h);
        } else {
          if (BottoneGiu.wasLongPushed()==true) {
            setOff(_dow,_h);
          } else {
            
          }
        }
        if(_s != previousSec) {
          previousSec = _s;
          if ((timeStartTemp + 5000) < millis()) {
            timeStartTemp = millis();
            if (ReadTemp()) {
              Serial.print("get temp:"); Serial.println(temperature);
              displayTemp(temperature);
              if ((temperature < tempHigh) && isOn(_dow,_h)) {
                SwichReleais(HIGH);
              } else {
                if ((temperature < tempLow) && ! isOn(_dow,_h)) {
                  SwichReleais(HIGH);
                } else {
                  SwichReleais(LOW);
                }
              }
            }
          }
          displayTime(_h,_m,_dow);
          displayDate(_da, _mo, _ye,_dow);
        }
      }
      break;
    case STATUS_MENU:
      if(checkMenuTimeout()==true){
        initScreen();
        programStatus=STATUS_RUNNING;
      } else {
        if ( (BottoneSu.wasPushed()==true) && menuStatus > 0) {
          menuStatus--;
          timeStartMenu=millis();
          doMenu("Configurazione",arMainMenu, 4, menuStatus);
        }
        if ( (BottoneGiu.wasPushed()==true) && menuStatus < 3) {
          menuStatus++;
          timeStartMenu=millis();
          doMenu("Configurazione",arMainMenu, 4, menuStatus);
        }
        if (BottoneIndietro.wasPushed()==true) {
          initScreen();
          programStatus=STATUS_RUNNING;
          break;
        }
        if (BottoneMenu.wasPushed()==true) {
          initScreen();
          timeStartMenu=millis();
          switch(menuStatus) {
            case 0:
              programStatus = STATUS_SET_TEMPH;
              menuTempStatus = 0;
              doMenuInt("Set Temp H",MAX_TEMPH, MIN_TEMPH,  tempHigh);
              break;
            case 1:
              programStatus = STATUS_SET_TEMPL;
              menuTempStatus = 0;
              doMenuInt("Set Temp L", MAX_TEMPL, MIN_TEMPL, tempLow);
              break;
            case 2:
              programStatus = STATUS_SET_DOW;
              menuTempStatus = 0;
              doMenu("Giorno Sett.", arWeekL, 7,  menuTempStatus);
              break;
            case 3:
              programStatus = STATUS_SET_HOURS;
              menuTempStatus = 0;
              break;
          }
          break;
        }
      }
      break;
    case STATUS_SET_TEMPH:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
        menuStatus = 0;
        initScreen();
        doMenu("Configurazione",arMainMenu, 4, menuStatus);
      } else {
        if ( (BottoneGiu.wasPushed()==true) && tempHigh > MIN_TEMPH) {
          tempHigh--;
          timeStartMenu=millis();
          doMenuInt("Set Temp H",MAX_TEMPH, MIN_TEMPH,  tempHigh);
        }
        if ( (BottoneSu.wasPushed()==true) && tempHigh < MAX_TEMPH) {
          tempHigh++;
          timeStartMenu=millis();
          doMenuInt("Set Temp H",MAX_TEMPH, MIN_TEMPH,  tempHigh);
        }
        if (BottoneIndietro.wasPushed()==true) {
          config.MaxConfigTemp = tempHigh;
          EEPROM.put(0, config);
          displaySaved();
          programStatus=STATUS_MENU;
          menuStatus = 0;
          doMenu("Configurazione",arMainMenu, 4, menuStatus);
          break;
        }
      }
      break;
    case STATUS_SET_TEMPL:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
        menuStatus = 0;
        initScreen();
        doMenu("Configurazione",arMainMenu, 4, menuStatus);
      } else {
        if ( (BottoneGiu.wasPushed()==true) && tempLow > MIN_TEMPL) {
          tempLow--;
          timeStartMenu=millis();
          doMenuInt("Set Temp L", MAX_TEMPL, MIN_TEMPL, tempLow);
        }
        if ( (BottoneSu.wasPushed()==true) && tempLow < MAX_TEMPL) {
          tempLow++;
          timeStartMenu=millis();
          doMenuInt("Set Temp L", MAX_TEMPL, MIN_TEMPL, tempLow);
        }
        if (BottoneIndietro.wasPushed()==true) {
          config.MinConfigTemp = tempLow;
          EEPROM.put(0, config);
          displaySaved();
          programStatus=STATUS_MENU;
          menuStatus = 0;
          doMenu("Configurazione",arMainMenu, 4, menuStatus);
          break;
        }
      }
      break;
      /* Set calendar */
    case STATUS_SET_DOW:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
        menuStatus = 0;
        initScreen();
        doMenu("Configurazione",arMainMenu, 4, menuStatus);
      } else {
        if ( (BottoneSu.wasPushed()==true) && menuTempStatus > 0) {
          menuTempStatus--;
          timeStartMenu=millis();
          doMenu("Giorno Sett.", arWeekL, 7,  menuTempStatus);
        }
        if ( (BottoneGiu.wasPushed()==true) && menuTempStatus < 6) {
          menuTempStatus++;
          timeStartMenu=millis();
          doMenu("Giorno Sett.", arWeekL, 7,  menuTempStatus);
        }
        if (BottoneMenu.wasPushed()==true) {
          timeStartMenu=millis();
          currentDayOfWeek =  menuTempStatus;
          menuTempStatus = 0;
          programStatus=STATUS_SET_CAL;
          doMenuHour((String)arWeekL[currentDayOfWeek], menuTempStatus);
          break;
        }
        if (BottoneIndietro.wasPushed()==true) {
          EEPROM.put(0, config);
          displaySaved();
          programStatus=STATUS_MENU;
          menuStatus = 0;
          doMenu("Configurazione",arMainMenu, 4, menuStatus);
          break;
        }
      }
      break;
    case STATUS_SET_CAL:
      if(checkMenuTimeout()==true){
          menuStatus = 0;
          doMenu("Configurazione",arMainMenu, 4, menuStatus);
          programStatus=STATUS_MENU;
      } else {
        if ( (BottoneSu.wasPushed()==true) && menuTempStatus > 0) {
          menuTempStatus--;
          timeStartMenu=millis();
          doMenuHour((String)arWeekL[currentDayOfWeek], menuTempStatus);
        }
        if ( (BottoneGiu.wasPushed()==true) && menuTempStatus < 23) {
          menuTempStatus++;
          timeStartMenu=millis();
          doMenuHour((String)arWeekL[currentDayOfWeek], menuTempStatus);
        }
        if (BottoneMenu.wasPushed()==true) {
          timeStartMenu=millis();
          flipStatus(currentDayOfWeek, menuTempStatus) ;
          doMenuHour((String)arWeekL[currentDayOfWeek], menuTempStatus);
          break;
        }
        if (BottoneIndietro.wasPushed()==true) {
          EEPROM.put(0, config);
          displaySaved();
          programStatus=STATUS_SET_DOW;
          menuTempStatus = currentDayOfWeek;
          doMenu("Giorno Sett.", arWeekL, 7,  menuTempStatus);
          break;
        }
      }
      break;
  }
}

/* *************************************************************** */
/* ************** FUNCTIONS ************************************** */
/* *************************************************************** */
int readStatus(int d, int h) {
  return bitRead(config.arDays[d][(h-(h%8))/8], h % 8);
}
void setStatus(int d, int h) {
  bitSet(config.arDays[d][(h-(h%8))/8], h % 8);
}
void clearStatus(int d, int h) {
  bitClear(config.arDays[d][(h-(h%8))/8], h % 8);
}
void flipStatus(int d, int h) {
  if (bitRead(config.arDays[d][(h-(h%8))/8], h % 8) == 1) {
    bitClear(config.arDays[d][(h-(h%8))/8], h % 8);
  } else {
    bitSet(config.arDays[d][(h-(h%8))/8], h % 8);
  }
}
boolean ReadTemp() {
  if (!dht11.read(PIN_DHT11, &temperature, &humidity, data)) {
      humidity2 = b2byte(data + 8);
      temperature2 = b2byte(data + 24);
      return true;
  }
  return false;
}

byte b2byte(byte data[8]) {
  byte v = 0;
  for (int i = 0; i < 8; i++) {
      v += data[i] << (7 - i);
  }
  return v;
} 

void SwichReleais(int st) {
  if ((millis() - timeStartRelais > (long)60000)) {
    digitalWrite(PIN_RELEAIS,st); // enable/disable releais (once a minute!)
    timeStartRelais = millis();
  }
}

void DisplayOff() {
  display.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
}
void DisplayOn() {
  display.ssd1306WriteCmd(SSD1306_DISPLAYON);
}    

void initScreen(){
  display.setFont(Adafruit5x7);
  display.clear();
}
void setCursorAndSize(int x, int y, int s) {
  display.setCursor(x,y); 
  if(s==1)
    display.set1X();
  else
    display.set2X();
}
void displaySaved() {
  initScreen();
  setCursorAndSize(0,1,2);
  display.println("SAVED !");
  delay(1000);
  initScreen();
}
void displayTime(int h,int m,int d) {
    // Clear left half of video
    // first row
    setCursorAndSize(0,0,1); 
    if (h>0) {
      sprintf(buffer,"%02d:%02d%s   %02d'", h-1, 0, (isOn(d,h-1))? "*":" ", tempHigh);
    } else {
      sprintf(buffer,"%09s%02d'", " ", tempHigh);
    }
    display.print(buffer); 
    // Current hour
    setCursorAndSize(0,1,2); 
    sprintf(buffer,"%02d:%02d%s", h, m, (isOn(d,h))? "*":" ");
    display.print(buffer);
    // next hour
    setCursorAndSize(0,3,1); 
    if (h<23) {
      sprintf(buffer,"%02d:%02d%s   %02d'", h+1, 0, (isOn(d,h+1))? "*":" ", tempLow);
    } else {
      sprintf(buffer,"%09s%02d'", " ", tempLow);
    }
    display.print(buffer); 
}
bool isOn(int d,int h) {
  return (readStatus(d,h)==0)? false : true; 
}

bool toggleOnOff(int h,int d) {
  flipStatus(d,h);
  return isOn(d,h); 
}
void setOn(int d, int h){
  setStatus(d,h);
}
void setOff(int d, int h){
  clearStatus(d,h);
}

void displayTemp(int t) {
    setCursorAndSize(80,0,2); 
    sprintf(buffer,"%02d C", t ); 
    display.print(buffer);
    setCursorAndSize(104,0,1); 
    display.print("o");
}

void displayDate(int d,int m,int y,int w) {
    setCursorAndSize(78,2,1); 
    sprintf(buffer,"%3s %02d", arWeek[w], d );   
    display.print(buffer);
    setCursorAndSize(78,3,1); 
    sprintf(buffer,"%3s %04d", arMonth[m-1], y );    
    display.print(buffer);
}


void setTimeStartMenu(){
  timeStartMenu = millis();
}

bool checkMenuTimeout(){
  if ((millis() - timeStartMenu) > MENU_TIMEOUT)
    return true;
  else
    return false;  
}

void doMenu(String intest, char m[][12], int l, int i) {
  initScreen();
  setCursorAndSize(0,0,1); 
  if (i==0) {
    display.println(intest);
  } else {
    display.println(m[i-1]);  
  }
  setCursorAndSize(0,1,2); 
  display.print(m[i]);
  if(i<(l-1)) {
  setCursorAndSize(0,3,1); 
    display.print(m[i+1]);  
  }
}

void doMenuHour(String intest, int iCurrent) {
  initScreen();
  setCursorAndSize(0,0,1); 
  if (iCurrent==0) {
    display.print(intest);
  } else {
    sprintf(buffer,"%02d:00%s", iCurrent-1, (isOn(currentDayOfWeek,iCurrent-1))? "*":" ");
    display.print(buffer); 
  }
  setCursorAndSize(0,1,2); 
  sprintf(buffer,"%02d:00%s", iCurrent, (isOn(currentDayOfWeek,iCurrent))? "*":" ");
  display.print(buffer); 
  if(iCurrent < 23) {
    setCursorAndSize(0,3,1); 
    sprintf(buffer,"%02d:00%s", iCurrent+1, (isOn(currentDayOfWeek,iCurrent+1))? "*":" ");
    display.print(buffer); 
  }
}


void doMenuInt(String intest, int iMax, int iMin, int iCurrent) {
  initScreen();
  setCursorAndSize(0,0,1); 
  if (iCurrent==iMax) {
    display.println(intest);
  } else {
      sprintf(buffer,"%02d", iCurrent+1);
      display.print(buffer); 
  }
  setCursorAndSize(0,1,2); 
  sprintf(buffer,"%02d", iCurrent);
  display.print(buffer); 
  if(iCurrent > iMin) {
    setCursorAndSize(0,3,1); 
    display.println(iCurrent-1);  
  }
}
