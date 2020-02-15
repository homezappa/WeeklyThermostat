#include <SPI.h>
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include <MyButton.h>
#include <RTClib.h>
#include <dht11.h>
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

#define STATUS_SET_TEMPH  0x0A
#define STATUS_SET_TEMPL  0x0B
#define STATUS_SET_CAL    0x0C
#define STATUS_SET_DOW    0x0D
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

// Declariation of amount of time the display remains on (msecs)
#define TIMER_DISPLAY   60000L
// Declaration of every how many msecs we get the temperature (warning: the DHT11 permits only a read every 2 seconds!)
#define TIMER_TEMP      5000L
// Declaration of: every 1000 msec refresh display
#define TIMER_SECS      1000L

/* *************************************************************** */
/* ***************** GLOBAL VARS ********************************* */
/* *************************************************************** */
unsigned long timeStartDisplay = 0L;
unsigned long timeStartMenu = 0L;
unsigned long timeStartTemp   = 0L;
unsigned long timeSeconds   = 0L;
unsigned long currentMillis = 0L;

/*********************************************************************/
// Variabili lettura temperatura DHT11
byte temperature    = 0;

/********************************************************************/
// Struttura di configurazione
struct Config {
  int  TAG;                 // integer to know if I saved before on eeprom
  int  MinConfigTemp;       // Low Temp ...
  int  MaxConfigTemp;       // High Temp 
  char arDays[7][3] = {     // only 21 bytes to save 24x7 config!
  // 07,06,05,04,03,02,01,00,  15,14,13,12,11,10,09,08,  23,22,21,20,19,18,17,16 
      0B01000000, 0B00000000, 0B11111110, // Lunedi
      0B01000000, 0B00000000, 0B11111110, // Martedi
      0B01000000, 0B00000000, 0B11111110, // Mercoledi
      0B01000000, 0B00000000, 0B11111110, // Giovedi
      0B01000000, 0B00000000, 0B11111110, // Venerdi
      0B00000000, 0B11111000, 0B11111111, // Sabato
      0B00000000, 0B11111000, 0B11111111, // DOmenica
  };
};

Config config;

int TAG_READ;

int  previousSec = -1;
char buffer[128];  
int  programStatus = STATUS_RUNNING;

int  menuStatus     = 0;
int  menuTempStatus = 0;

int  tempHigh       = 20;
int  tempLow        = 15;
bool invertMode     = false;
char arWeek[7][4]      = { "Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab" };
char arWeekL[7][12]    = { "Domenica", "Lunedi", "Martedi", "Mercoledi", "Giovedi", "Venerdi", "Sabato" };
char arMonth[12][4]    = { "Gen", "Feb", "Mar", "Apr", "Mag", "Giu", "Lug", "Ago", "Set", "Ott", "Nov", "Dic" };
char arMainMenu[5][12] = { "TEMP.ALTA", "TEMP.BASSA", "GIORNO", "ORARI", "OROLOGIO" };
char arTime[6][12] = { "Set ORA", "Set Min.", "Set GIORNO", "Set MESE", "Set ANNO" };

int  currentDayOfWeek=0;

int _dowset, _hset, _mset, _daset, _sset, _moset, _yeset;
int _dow,_h,_m,_s,_da,_mo,_ye;

bool colonOn = true;


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

dht11             DHT11;



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
  for (int i=0; i<7; i++) {
    Serial.print(arWeek[i]); Serial.print(":");
    for(int h=0; h<24;h++) {
      Serial.print((readStatus(i,h)==1)?"1":"0");
    }
    Serial.println("");
  }
  Serial.println("-----");
  currentMillis     = millis();
  now = RTC.now();
  timeStartTemp     = currentMillis;
  timeSeconds       = currentMillis;
  timeStartDisplay  = currentMillis;
  _dow = now.dayOfTheWeek();
  _h = now.hour();
  _m = now.minute();
  _s = now.second();
  _da = now.day();
  _mo = now.month();
  _ye = now.year();

  displayTime(_h,_m,_dow);
  displayDate(_da, _mo, _ye,_dow);
  if (ReadTemp()) {
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

void loop() {
  currentMillis = millis();
  BottoneMenu.read();
  BottoneIndietro.read();
  BottoneSu.read();
  BottoneGiu.read();
  now = RTC.now();
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
      if ( (BottoneMenu.isPushed()==true) ||
           (BottoneSu.isPushed()==true) ||
           (BottoneGiu.isPushed()==true) ||
           (BottoneIndietro.isPushed()==true) ) {
        DisplayOn();
      }
      if (BottoneMenu.wasLongPushed()==true) {
        DisplayOn();
        timeStartMenu=currentMillis;
        programStatus = STATUS_MENU;
        menuStatus = 0;
        display.invertDisplay(false);
        doMenu("Configurazione",arMainMenu, 5, menuStatus);
      } else {
        if(currentMillis > (timeStartDisplay+TIMER_DISPLAY)) {
          DisplayOff();
        }
        if (BottoneSu.wasLongPushed()==true) {
          DisplayOn();
          setOn(_dow,_h);
        } 
        if (BottoneGiu.wasLongPushed()==true) {
          DisplayOn();
          setOff(_dow,_h);
        }
        if(currentMillis > (timeSeconds+TIMER_SECS)) {
          timeSeconds = currentMillis;
          if (currentMillis > (timeStartTemp + TIMER_TEMP)) {
            timeStartTemp = currentMillis;
            if (ReadTemp()) {
              displayTemp(temperature);
              if ((temperature < tempHigh) && isOn(_dow,_h)) {
                SwichReleais(HIGH);
              } else {
                if (temperature < tempLow) {
                  SwichReleais(HIGH);
                } else {
                  if (temperature > tempHigh) {
                    SwichReleais(LOW);
                  } else {
                    if ((temperature > tempLow) && ! isOn(_dow,_h)) {
                      SwichReleais(LOW);
                    }
                  }
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
          DisplayOn();
          menuStatus--;
          timeStartMenu=currentMillis;
          doMenu("Configurazione",arMainMenu, 5, menuStatus);
        }
        if ( (BottoneGiu.wasPushed()==true) && menuStatus < 4) {
          DisplayOn();
          menuStatus++;
          timeStartMenu=currentMillis;
          doMenu("Configurazione",arMainMenu, 5, menuStatus);
        }
        if (BottoneIndietro.wasPushed()==true) {
          initScreen();
          DisplayOn();
          displayTime(_h,_m,_dow);
          displayDate(_da, _mo, _ye,_dow);
          programStatus=STATUS_RUNNING;
          break;
        }
        if (BottoneMenu.wasPushed()==true) {
          DisplayOn();
          initScreen();
          timeStartMenu=currentMillis;
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
              programStatus = STATUS_SET_CAL;
              menuTempStatus = 0;
              doMenuHour((String)arWeekL[currentDayOfWeek], menuTempStatus);
              break;
            case 4:
              programStatus = STATUS_SET_TIME;
              menuStatus = 0;
              _hset = now.hour();
              _mset = now.minute();
              _sset = now.second();
              _daset = now.day();
              _moset = now.month();
              _yeset = now.year();
              doMenu("Set Orologio", arTime, 6,  menuStatus);
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
          DisplayOn();
          tempHigh--;
          timeStartMenu=currentMillis;
          doMenuInt("Set Temp H",MAX_TEMPH, MIN_TEMPH,  tempHigh);
        }
        if ( (BottoneSu.wasPushed()==true) && tempHigh < MAX_TEMPH) {
          DisplayOn();
          tempHigh++;
          timeStartMenu=currentMillis;
          doMenuInt("Set Temp H",MAX_TEMPH, MIN_TEMPH,  tempHigh);
        }
        if (BottoneIndietro.wasPushed()==true) {
          DisplayOn();
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
          DisplayOn();
          tempLow--;
          timeStartMenu=currentMillis;
          doMenuInt("Set Temp L", MAX_TEMPL, MIN_TEMPL, tempLow);
        }
        if ( (BottoneSu.wasPushed()==true) && tempLow < MAX_TEMPL) {
          DisplayOn();
          tempLow++;
          timeStartMenu=currentMillis;
          doMenuInt("Set Temp L", MAX_TEMPL, MIN_TEMPL, tempLow);
        }
        if (BottoneIndietro.wasPushed()==true) {
          DisplayOn();
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
          DisplayOn();
          menuTempStatus--;
          timeStartMenu=currentMillis;
          doMenu("Giorno Sett.", arWeekL, 7,  menuTempStatus);
        }
        if ( (BottoneGiu.wasPushed()==true) && menuTempStatus < 6) {
          DisplayOn();
          menuTempStatus++;
          timeStartMenu=currentMillis;
          doMenu("Giorno Sett.", arWeekL, 7,  menuTempStatus);
        }
        if (BottoneMenu.wasPushed()==true) {
          DisplayOn();
          timeStartMenu=currentMillis;
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
          DisplayOn();
          menuTempStatus--;
          timeStartMenu=currentMillis;
          doMenuHour((String)arWeekL[currentDayOfWeek], menuTempStatus);
        }
        if ( (BottoneGiu.wasPushed()==true) && menuTempStatus < 23) {
          DisplayOn();
          menuTempStatus++;
          timeStartMenu=currentMillis;
          doMenuHour((String)arWeekL[currentDayOfWeek], menuTempStatus);
        }
        if (BottoneMenu.wasPushed()==true) {
          DisplayOn();
          timeStartMenu=currentMillis;
          flipStatus(currentDayOfWeek, menuTempStatus) ;
          doMenuHour((String)arWeekL[currentDayOfWeek], menuTempStatus);
          break;
        }
        if (BottoneIndietro.wasPushed()==true) {
          DisplayOn();
          EEPROM.put(0, config);
          displaySaved();
          programStatus=STATUS_SET_DOW;
          menuTempStatus = currentDayOfWeek;
          doMenu("Giorno Sett.", arWeekL, 7,  menuTempStatus);
          break;
        }
      }
      break;
    case STATUS_SET_TIME:
    case STATUS_SET_HH:
    case STATUS_SET_MM:
    case STATUS_SET_DD:
    case STATUS_SET_MO:
    case STATUS_SET_YY:
      setTime();
      break;
  }
}

/* *************************************************************** */
/* ************** FUNCTIONS ************************************** */
/* *************************************************************** */

void setMyClock() {
  RTC.adjust(DateTime(_yeset, _moset, _daset, _hset, _mset, 0));
}

void setTime() {
  switch(programStatus) {
    case STATUS_SET_TIME:
      if(checkMenuTimeout()==true){
        initScreen();
        programStatus=STATUS_RUNNING;
      } else {
        if ( (BottoneSu.wasPushed()==true) && menuStatus > 0) {
          DisplayOn();
          menuStatus--;
          timeStartMenu=currentMillis;
          doMenu("Orologio",arTime, 5, menuStatus);
        }
        if ( (BottoneGiu.wasPushed()==true) && menuStatus < 4) {
          DisplayOn();
          menuStatus++;
          timeStartMenu=currentMillis;
          doMenu("Orologio",arTime, 5, menuStatus);
        }
        if (BottoneIndietro.wasPushed()==true) {
          initScreen();
          menuStatus=0;
          setMyClock();
          displaySaved();
          programStatus=STATUS_MENU;
          DisplayOn();
          displayTime(_h,_m,_dow);
          displayDate(_da, _mo, _ye,_dow);
          break;
        }
        if (BottoneMenu.wasPushed()==true) {
          DisplayOn();
          initScreen();
          timeStartMenu=currentMillis;
          switch(menuStatus) {
            case 0:
              programStatus = STATUS_SET_HH;
              doMenuInt("Set Ora",23, 0, _hset);
              break;
            case 1:
              programStatus = STATUS_SET_MM;
              doMenuInt("Set Minuti", 59, 0, _mset);
              break;
            case 2:
              programStatus = STATUS_SET_DD;
              doMenuInt("Set Giorno", 31, 1,  _daset);
              break;
            case 3:
              programStatus = STATUS_SET_MO;
              doMenuInt("Set Mese", 12, 1,  _moset);
              break;
            case 4:
              programStatus = STATUS_SET_YY;
              doMenuInt("Set Anno", 2025, 2020,  _yeset);
              break;
          }
          break;
        }
      }
      break;
    case STATUS_SET_HH:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
        menuStatus = 0;
        initScreen();
        doMenu("Configurazione",arMainMenu, 4, menuStatus);
      } else {
        if ( (BottoneGiu.wasPushed()==true) && _hset > 0) {
          DisplayOn();
          _hset--;
          timeStartMenu=currentMillis;
          doMenuInt("Set Ora",24, 0, _hset);
        }
        if ( (BottoneSu.wasPushed()==true) && _hset < 23) {
          DisplayOn();
          _hset++;
          timeStartMenu=currentMillis;
          doMenuInt("Set Ora",24, 0, _hset);
        }
        if ((BottoneIndietro.wasPushed()==true) || (BottoneMenu.wasPushed()==true)) {
          DisplayOn();
          programStatus=STATUS_SET_TIME;
          doMenu("Orologio",arTime, 5, menuStatus);
          break;
        }
      }
      break;    
    case STATUS_SET_MM:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
        menuStatus = 0;
        initScreen();
        doMenu("Configurazione",arMainMenu, 4, menuStatus);
      } else {
        if ( (BottoneGiu.wasPushed()==true) && _mset > 0) {
          DisplayOn();
          _mset--;
          timeStartMenu=currentMillis;
          doMenuInt("Set Minuti", 59, 0, _mset);
        }
        if ( (BottoneSu.wasPushed()==true) && _mset < 59) {
          DisplayOn();
          _mset++;
          timeStartMenu=currentMillis;
          doMenuInt("Set Minuti", 59, 0, _mset);
        }
        if ((BottoneIndietro.wasPushed()==true) || (BottoneMenu.wasPushed()==true)) {
          DisplayOn();
          programStatus=STATUS_SET_TIME;
          doMenu("Orologio",arTime, 5, menuStatus);
          break;
        }
      }
      break;    
    case STATUS_SET_DD:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
        menuStatus = 0;
        initScreen();
        doMenu("Configurazione",arMainMenu, 4, menuStatus);
      } else {
        if ( (BottoneGiu.wasPushed()==true) && _daset > 1) {
          DisplayOn();
          _daset--;
          timeStartMenu=currentMillis;
          doMenuInt("Set Giorno", 31, 1,  _daset);
        }
        if ( (BottoneSu.wasPushed()==true) && _daset < 31) {
          DisplayOn();
          _daset++;
          timeStartMenu=currentMillis;
          doMenuInt("Set Giorno", 31, 1,  _daset);
        }
        if ((BottoneIndietro.wasPushed()==true) || (BottoneMenu.wasPushed()==true)) {
          DisplayOn();
          programStatus=STATUS_SET_TIME;
          doMenu("Orologio",arTime, 5, menuStatus);
          break;
        }
      }
      break;    
    case STATUS_SET_MO:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
        menuStatus = 0;
        initScreen();
        doMenu("Configurazione",arMainMenu, 4, menuStatus);
      } else {
        if ( (BottoneGiu.wasPushed()==true) && _moset > 1) {
          DisplayOn();
          _moset--;
          timeStartMenu=currentMillis;
          doMenuInt("Set Mese", 12, 1,  _moset);
        }
        if ( (BottoneSu.wasPushed()==true) && _moset < 12) {
          DisplayOn();
          _moset++;
          timeStartMenu=currentMillis;
          doMenuInt("Set Mese", 12, 1,  _moset);
        }
        if ((BottoneIndietro.wasPushed()==true) || (BottoneMenu.wasPushed()==true)) {
          DisplayOn();
          programStatus=STATUS_SET_TIME;
          doMenu("Orologio",arTime, 5, menuStatus);
          break;
        }
      }
      break;    
    case STATUS_SET_YY:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
        menuStatus = 0;
        initScreen();
        doMenu("Configurazione",arMainMenu, 4, menuStatus);
      } else {
        if ( (BottoneGiu.wasPushed()==true) && _yeset > 2020) {
          DisplayOn();
          _yeset--;
          timeStartMenu=currentMillis;
          doMenuInt("Set Anno", 2025, 2020,  _yeset);
        }
        if ( (BottoneSu.wasPushed()==true) && _yeset < 2025) {
          DisplayOn();
          _yeset++;
          timeStartMenu=currentMillis;
          doMenuInt("Set Anno", 2025, 2020,  _yeset);
        }
        if ((BottoneIndietro.wasPushed()==true) || (BottoneMenu.wasPushed()==true)) {
          DisplayOn();
          programStatus=STATUS_SET_TIME;
          doMenu("Orologio",arTime, 5, menuStatus);
          break;
        }
      }
      break;    
  }  
}



int readStatus(int d, int h) { 
  // read the bit that correspond to dayOfWeek d and Hour h: 1 = high temp, 0 = low temp
  return bitRead(config.arDays[d][(h-(h%8))/8], h % 8);
}
void setStatus(int d, int h) {
  // set to 1 the bit that correspond to dayOfWeek d and Hour h: 1 = high temp, 0 = low temp
  bitSet(config.arDays[d][(h-(h%8))/8], h % 8);
}
void clearStatus(int d, int h) {
  // set to 0 the bit that correspond to dayOfWeek d and Hour h: 1 = high temp, 0 = low temp
  bitClear(config.arDays[d][(h-(h%8))/8], h % 8);
}
void flipStatus(int d, int h) {
  // flip the bit that correspond to dayOfWeek d and Hour h: 1 = high temp, 0 = low temp
  if (bitRead(config.arDays[d][(h-(h%8))/8], h % 8) == 1) {
    bitClear(config.arDays[d][(h-(h%8))/8], h % 8);
  } else {
    bitSet(config.arDays[d][(h-(h%8))/8], h % 8);
  }
}
boolean ReadTemp() {
  if(DHT11.read(PIN_DHT11) == DHTLIB_OK) {
    temperature = DHT11.temperature;
    return true;
  }
  return false;
}

void SwichReleais(int st) {
  digitalWrite(PIN_RELEAIS,st); // enable/disable releais 
  if(st==HIGH) {
    display.invertDisplay(true);
    invertMode = true;
  } else {
    display.invertDisplay(false);
    invertMode = false;
  }
}

void DisplayOff() {
  display.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
}
void DisplayOn() {
  display.ssd1306WriteCmd(SSD1306_DISPLAYON);
  timeStartDisplay = currentMillis;
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
  display.print(" SAVED !");
  delay(1000);
  initScreen();
}
void displayTime(int h,int m,int d) {
    setCursorAndSize(0,0,1); 
    if (h>0) {
      sprintf(buffer,"%02d:%02d%c   %02d%c", h-1, 0, (isOn(d,h-1))? 0x7F:0x20, tempHigh, 0x7E);
    } else {
      sprintf(buffer,"%09s%02d%c", " ", tempHigh,0x7E);
    }
    display.print(buffer); 
    // Current hour
    setCursorAndSize(0,1,2); 
    sprintf(buffer,"%02d%s%02d%c", h,(colonOn==true) ? ":" : " ", m, (isOn(d,h))? 0x7F:0x20);
    display.print(buffer);
    if(colonOn==true)
      colonOn=false;
    else
      colonOn=true;
    // next hour
    setCursorAndSize(0,3,1); 
    if (h<23) {
      sprintf(buffer,"%02d:%02d%c   %02d%c", h+1, 0, (isOn(d,h+1))? 0x7F:0x20, tempLow, 0x7E);
    } else {
      sprintf(buffer,"%09s%02d%c", " ", tempLow, 0x7E);
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
    display.invertDisplay(invertMode);
    setCursorAndSize(80,0,2); 
    sprintf(buffer,"%02d%cC", t , 0x7E); 
    display.print(buffer);
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
    display.print(intest);
  } else {
    display.print(m[i-1]);  
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
    sprintf(buffer,"%02d:00%c", iCurrent-1, (isOn(currentDayOfWeek,iCurrent-1))? 0x7F:0x20);
    display.print(buffer); 
  }
  setCursorAndSize(0,1,2); 
  sprintf(buffer,"%02d:00%c", iCurrent, (isOn(currentDayOfWeek,iCurrent))? 0x7F:0x20);
  display.print(buffer); 
  if(iCurrent < 23) {
    setCursorAndSize(0,3,1); 
    sprintf(buffer,"%02d:00%c", iCurrent+1, (isOn(currentDayOfWeek,iCurrent+1))? 0x7F:0x20);
    display.print(buffer); 
  }
}


void doMenuInt(String intest, int iMax, int iMin, int iCurrent) {
  initScreen();
  setCursorAndSize(0,0,1); 
  if (iCurrent==iMax) {
    display.print(intest);
  } else {
      sprintf(buffer,"%02d", iCurrent+1);
      display.print(buffer); 
  }
  setCursorAndSize(0,1,2); 
  sprintf(buffer,"%02d", iCurrent);
  display.print(buffer); 
  if(iCurrent > iMin) {
    setCursorAndSize(0,3,1); 
    display.print(iCurrent-1);  
  }
}
