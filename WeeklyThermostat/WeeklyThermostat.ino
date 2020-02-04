#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MyButton.h>
#include <RTClib.h>
#include <SimpleDHT.h>
#include <EEPROM.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define STATUS_MENU       1
#define STATUS_RUNNING    2
#define STATUS_SET_TIME   3
#define STATUS_SET_HH     4
#define STATUS_SET_MM     5
#define STATUS_SET_DD     6
#define STATUS_SET_MO     7
#define STATUS_SET_YY     8
#define STATUS_SET_TEMP   9
#define STATUS_SET_TEMPH  10
#define STATUS_SET_TEMPL  11
#define STATUS_SET_CAL    12
#define STATUS_SET_DOW    13
#define STATUS_SET_HOURS  14
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

/* ***************************************************************** */
unsigned long timeStartMenu = 0L;
unsigned long timeStartRelais = 0L;



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


int TAG_READ;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

MyButton BottoneMenu;
MyButton BottoneIndietro;
MyButton BottoneSu;
MyButton BottoneGiu;

RTC_DS1307              RTC;

DateTime                now;

SimpleDHT11             dht11;


int                 previousSec = -1;
char buffer[32];  
int  programStatus = STATUS_RUNNING;

int menuStatus     = 0;
int menuTempStatus = 0;

int tempHigh       = 20;
int tempLow        = 15;

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


char arWeek[7][12] = {
  "Dom",
  "Lun",
  "Mar",
  "Mer",
  "Gio",
  "Ven",
  "Sab"
};
char arMonth[12][4] = {
  "Gen",
  "Feb",
  "Mar",
  "Apr",
  "Mag",
  "Giu",
  "Lug",
  "Ago",
  "Set",
  "Ott",
  "Nov",
  "Dic"
};
char arMainMenu[3][12] = {
  "OROLOGIO",
  "TEMPERAT.",
  "CALENDARIO"
};
char arTempMenu[2][12] = {
  "TEMP. MAX",
  "TEMP. MIN"
};
char arWeekMenu[2][12] = {
  "GIORNO",
  "ORARI"
};

void initScreen(){
  display.clearDisplay();
  display.setCursor(0,0); 
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);
}
void setCursorAndSize(int x, int y, int s) {
  display.setCursor(x,y); 
  display.setTextSize(s); 
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);
}
void displaySaved() {
  initScreen();
  setCursorAndSize(0,8,2);
  display.println("SAVED !");
  display.display();
  delay(1000);
  initScreen();
}

/* SETUP */
void setup() {
  int _tempTAG;
  pinMode(PIN_RELEAIS,OUTPUT);
  Serial.begin(9600);
  Serial.println("Start");
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

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
  
  display.clearDisplay();
/*
  for (int i=0; i<7; i++) {
    Serial.print(arWeek[i]); Serial.print(":");
    for(int h=0; h<24;h++) {
      Serial.print((readStatus(i,h)==1)?"1":"0");
    }
    Serial.println("");
  }
  Serial.println("-----");
*/
  timeStartRelais = millis();
}




/* displayTime: void
 *  displays 3 rows: first = hour -1 (if exixts) size 1
 *  second current hour-minute
 *  third hour + 1 (if exixts)
 *  all three followed by an indicator on/off
 *  @params: int h Current hour
 *           int m Current minute
 *           int d Current day of week
 */
void displayTime(int h,int m,int d) {
  char buffer[32];
    // Clear left half of video
    display.fillRect(0, 0, 72, SCREEN_HEIGHT, SSD1306_BLACK);
    // first row
    setCursorAndSize(0,0,1); 
    if (h>0) {
      sprintf(buffer,"%02d:%02d%c   %02d%c", h-1, 0, (isOn(d,h-1))? 254:32, tempHigh, 248);
    } else {
      sprintf(buffer,"%09s%02d%c", " ", tempHigh, 248);
    }
    display.print(buffer); 
    // Current hour
    setCursorAndSize(0,8,2); 
    sprintf(buffer,"%02d:%02d%c", h, m, (isOn(d,h))? 254:32);
    display.print(buffer);
    // next hour
    setCursorAndSize(0,24,1); 
    if (h<23) {
      sprintf(buffer,"%02d:%02d%c   %02d%c", h+1, 0, (isOn(d,h+1))? 254:32, tempLow, 248);
    } else {
      sprintf(buffer,"%09s%02d%c", " ", tempLow, 248);
    }
    display.print(buffer); 
    display.display();
    display.dim(1);
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
    display.fillRect(76, 0 , SCREEN_WIDTH-1, SCREEN_HEIGHT/2, SSD1306_BLACK);
    display.drawLine(74, 0, 74, SCREEN_HEIGHT-1, SSD1306_WHITE);
    setCursorAndSize(78,0,2); 
    sprintf(buffer,"%02d%cC", t, 248 ); // chr(248)   
    display.print(buffer);
}

void displayDate(int d,int m,int y,int w) {
    display.fillRect(76, SCREEN_HEIGHT/2 , SCREEN_WIDTH-1, SCREEN_HEIGHT-1, SSD1306_BLACK);
    setCursorAndSize(78,SCREEN_HEIGHT/2,1); 
    sprintf(buffer,"%3s %02d", arWeek[w], d ); // chr(248)   
    display.print(buffer);
    setCursorAndSize(78,SCREEN_HEIGHT/2+8,1); 
    sprintf(buffer,"%3s %04d", arMonth[m-1], y ); // chr(248)   
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

void doMenu(char *intest, char m[][12], int l, int i) {
  initScreen();
  if (i==0) {
    display.println(intest);
  } else {
    display.println(m[i-1]);  
  }
  setCursorAndSize(0,8,2); 
  display.print(m[i]);
  if(i<(l-1)) {
  setCursorAndSize(0,24,1); 
    display.println(m[i+1]);  
  }
  display.display();
}
void doMenuInt(char *intest, int iMax, int iMin, int iCurrent) {
  initScreen();
  if (iCurrent==iMax) {
    display.println(intest);
  } else {
      sprintf(buffer,"%02d", iCurrent+1);
      display.print(buffer); 
  }
  setCursorAndSize(0,8,2); 
  sprintf(buffer,"%02d", iCurrent);
  display.print(buffer); 
  if(iCurrent > iMin) {
    setCursorAndSize(0,24,1); 
    display.println(iCurrent-1);  
  }
  display.display();
}
void loop() {
  int currentDayOfWeek=0;
  BottoneMenu.read();
  BottoneIndietro.read();
  BottoneSu.read();
  BottoneGiu.read();
  now = RTC.now();
  
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
      } else {
        if (BottoneSu.wasLongPushed()==true) {
          setOn(now.dayOfTheWeek(),now.hour());
        } else {
          if (BottoneGiu.wasLongPushed()==true) {
            setOff(now.dayOfTheWeek(),now.hour());
          } else {
            
          }
        }
        if(now.second() != previousSec) {
          previousSec = now.second();
          if((now.second() % 5) == 0) {
            if (ReadTemp()) {
              displayTemp(temperature);
              if ((temperature < tempHigh) && isOn(now.dayOfTheWeek(),now.hour())) {
                Serial.println("T < TEMPHIGH && isOn");
                SwichReleais(HIGH);
              } else {
                if ((temperature < tempLow) && ! isOn(now.dayOfTheWeek(),now.hour())) {
                  Serial.println("T < TEMPLOW && ! isOn");
                  SwichReleais(HIGH);
                } else {
                  Serial.println("Else");
                  SwichReleais(LOW);
                }
              }
            }
          }
          displayTime(now.hour(),now.minute(),now.dayOfTheWeek());
          displayDate(now.day(), now.month(), now.year(),now.dayOfTheWeek());
        }
      }
      break;
    case STATUS_MENU:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_RUNNING;
      } else {
        if ( (BottoneSu.wasPushed()==true) && menuStatus > 0) {
          menuStatus--;
          timeStartMenu=millis();
        }
        if ( (BottoneGiu.wasPushed()==true) && menuStatus < 2) {
          menuStatus++;
          timeStartMenu=millis();
        }
        if (BottoneIndietro.wasPushed()==true) {
          display.clearDisplay();
          programStatus=STATUS_RUNNING;
          break;
        }
        if (BottoneMenu.wasPushed()==true) {
          display.clearDisplay();
          timeStartMenu=millis();
          switch(menuStatus) {
            case 0:
              break;
            case 1:
              programStatus = STATUS_SET_TEMP;
              menuTempStatus = 0;
              break;
            case 2:
              programStatus = STATUS_SET_CAL;
              menuTempStatus = 0;
              break;
          }
          break;
        }
        doMenu("Configurazione",arMainMenu, 3, menuStatus);
      }
      break;
    case STATUS_SET_TEMP:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
      } else {
        if ( (BottoneSu.wasPushed()==true) && menuTempStatus > 0) {
          menuTempStatus--;
          timeStartMenu=millis();
        }
        if ( (BottoneGiu.wasPushed()==true) && menuTempStatus < 1) {
          menuTempStatus++;
          timeStartMenu=millis();
        }
        if (BottoneIndietro.wasPushed()==true) {
          display.clearDisplay();
          programStatus=STATUS_MENU;
          break;
        }
        if(BottoneMenu.wasPushed()==true) {
          display.clearDisplay();
          timeStartMenu=millis();
          programStatus = (menuTempStatus == 0) ? STATUS_SET_TEMPH : STATUS_SET_TEMPL; 
          break;
        }
        doMenu("Temperature", arTempMenu, 2, menuTempStatus);
      }
      break;
    case STATUS_SET_TEMPH:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
      } else {
        if ( (BottoneGiu.wasPushed()==true) && tempHigh > MIN_TEMPH) {
          tempHigh--;
          timeStartMenu=millis();
        }
        if ( (BottoneSu.wasPushed()==true) && tempHigh < MAX_TEMPH) {
          tempHigh++;
          timeStartMenu=millis();
        }
        if (BottoneIndietro.wasPushed()==true) {
          config.MaxConfigTemp = tempHigh;
          EEPROM.put(0, config);
          displaySaved();
          programStatus=STATUS_SET_TEMP;
          break;
        }
        doMenuInt("Set Temp H",MAX_TEMPH, MIN_TEMPH,  tempHigh);
      }
      break;
    case STATUS_SET_TEMPL:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
      } else {
        if ( (BottoneGiu.wasPushed()==true) && tempLow > MIN_TEMPL) {
          tempLow--;
          timeStartMenu=millis();
        }
        if ( (BottoneSu.wasPushed()==true) && tempLow < MAX_TEMPL) {
          tempLow++;
          timeStartMenu=millis();
        }
        if (BottoneIndietro.wasPushed()==true) {
          config.MinConfigTemp = tempLow;
          EEPROM.put(0, config);
          displaySaved();
          programStatus=STATUS_SET_TEMP;
          break;
        }
        doMenuInt("Set Temp L", MAX_TEMPL, MIN_TEMPL, tempLow);
      }
      break;
      /* Set calendar */
    case STATUS_SET_CAL:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
      } else {
        if ( (BottoneSu.wasPushed()==true) && menuTempStatus > 0) {
          menuTempStatus--;
          timeStartMenu=millis();
        }
        if ( (BottoneGiu.wasPushed()==true) && menuTempStatus < 1) {
          menuTempStatus++;
          timeStartMenu=millis();
        }
        if (BottoneIndietro.wasPushed()==true) {
          display.clearDisplay();
          programStatus=STATUS_MENU;
          break;
        }
        if(BottoneMenu.wasPushed()==true) {
          display.clearDisplay();
          timeStartMenu=millis();
          programStatus = (menuTempStatus == 0) ? STATUS_SET_DOW : STATUS_SET_HOURS; 
          break;
        }
        doMenu(arWeek[currentDayOfWeek], arWeekMenu, 2, menuTempStatus);
      }
      break;
    case STATUS_SET_DOW:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
      } else {
        if ( (BottoneGiu.wasPushed()==true) && menuTempStatus > 0) {
          menuTempStatus--;
          timeStartMenu=millis();
        }
        if ( (BottoneSu.wasPushed()==true) && menuTempStatus < 7) {
          menuTempStatus++;
          timeStartMenu=millis();
        }
        if (BottoneIndietro.wasPushed()==true) {
          timeStartMenu=millis();
          currentDayOfWeek =  menuTempStatus;
          menuTempStatus = 0;
          programStatus=STATUS_SET_CAL;
          break;
        }
        doMenu("Giorno Sett.", arWeek, 7,  menuTempStatus);
      }
      break;
  }
}
