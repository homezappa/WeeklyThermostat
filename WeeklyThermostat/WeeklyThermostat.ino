#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MyButton.h>
#include <RTClib.h>
#include <SimpleDHT.h>


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

#define PIN_BACK          5
#define PIN_UP            4
#define PIN_DOWN          3
#define PIN_NEXT          2

#define PIN_DHT11   10


#define MIN_TEMPH         18
#define MAX_TEMPH         24
#define MIN_TEMPL         10
#define MAX_TEMPL         18

#define MY_TAG      30547  

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
  char arDays[7][24] = {
  // 00,01,02,03,04,05,06,07,08,09,10,11,12,13,14,15,16,17,18,19,20,21,22,23 
      0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, // Lunedi
      0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, // Martedi
      0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, // Mercoledi
      0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, // Giovedi
      0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, // Venerdi
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // Sabato
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  // DOmenica
  };
};

int TAG_READ;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

MyButton BottoneMenu(PIN_NEXT, INPUT_PULLUP);
MyButton BottoneIndietro(PIN_BACK, INPUT_PULLUP);
MyButton BottoneSu(PIN_UP, INPUT_PULLUP);
MyButton BottoneGiu(PIN_DOWN, INPUT_PULLUP);

RTC_DS1307              RTC;

DateTime            now;

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


void setup() {
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

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
 // display.display();
  
  display.clearDisplay();

  
   
}

char arDays[7][24] = {
// 00,01,02,03,04,05,06,07,08,09,10,11,12,13,14,15,16,17,18,19,20,21,22,23 
    0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, // Lunedi
    0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, // Martedi
    0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, // Mercoledi
    0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, // Giovedi
    0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, // Venerdi
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // Sabato
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  // DOmenica
};
char arWeek[7][4] = {
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
    if (h>0) {
      display.setCursor(0,0); 
      display.setTextSize(1); 
      display.setTextColor(SSD1306_WHITE);
      display.cp437(true);
      sprintf(buffer,"%02d:%02d%c", h-1, 0, (isOn(d,h-1))? 254:32);
      display.print(buffer); 
    }
    // Current hour
    display.setCursor(0,8); 
    display.setTextSize(2); 
    display.setTextColor(SSD1306_WHITE);
    display.cp437(true);
    sprintf(buffer,"%02d:%02d%c", h, m, (isOn(d,h))? 254:32);
    display.print(buffer);
    // next hour
    if (h<23) {
      display.setCursor(0,24); 
      display.setTextSize(1); 
      display.setTextColor(SSD1306_WHITE);
      display.cp437(true);
      sprintf(buffer,"%02d:%02d%c", h+1, 0, (isOn(d,h+1))? 254:32);
      display.print(buffer); 
    }
    display.display();
    display.dim(1);
}
bool isOn(int d,int h) {
  return (arDays[d][h]==0)? false : true; 
}

bool toggleOnOff(int h,int d) {
  arDays[d][h] = (arDays[d][h]==0)? 1 : 0;
  return (arDays[d][h]==0)? false : true; 
}

void displayTemp(int t) {
    display.fillRect(76, 0 , SCREEN_WIDTH-1, SCREEN_HEIGHT/2, SSD1306_BLACK);
    display.drawLine(74, 0, 74, SCREEN_HEIGHT-1, SSD1306_WHITE);
    display.setCursor(78,0); 
    display.setTextSize(2); 
    display.setTextColor(SSD1306_WHITE);
    display.cp437(true);
    sprintf(buffer,"%02d%cC", t, 248 ); // chr(248)   
    display.print(buffer);
}

void displayDate(int d,int m,int y,int w) {
    display.fillRect(76, SCREEN_HEIGHT/2 , SCREEN_WIDTH-1, SCREEN_HEIGHT-1, SSD1306_BLACK);
    display.setCursor(78, SCREEN_HEIGHT/2); 
    display.setTextSize(1); 
    display.setTextColor(SSD1306_WHITE);
    display.cp437(true);
    sprintf(buffer,"%3s %02d", arWeek[w], d ); // chr(248)   
    display.print(buffer);
    display.setCursor(78, SCREEN_HEIGHT/2 + 8); 
    sprintf(buffer,"%3s %04d", arMonth[m-1], y ); // chr(248)   
    display.print(buffer);
}

unsigned long timeStartMenu;

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
  display.clearDisplay();
  display.setCursor(0,0); 
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);
  if (i==0) {
    display.println(intest);
  } else {
    display.println(m[i-1]);  
  }
  display.setCursor(0,8); 
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);
  display.print(m[i]);
  if(i<(l-1)) {
    display.setCursor(0,24); 
    display.setTextSize(1); 
    display.setTextColor(SSD1306_WHITE);
    display.cp437(true);
    display.println(m[i+1]);  
  }
  display.display();
}
void doMenuInt(char *intest, int iMin, int iMax, int iCurrent) {
  display.clearDisplay();
  display.setCursor(0,0); 
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);
  if (iCurrent==iMin) {
    display.println(intest);
  } else {
      sprintf(buffer,"%02d", iCurrent-1);
      display.print(buffer); 
  }
  display.setCursor(0,8); 
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);
  sprintf(buffer,"%02d", iCurrent);
  display.print(buffer); 
  if(iCurrent < iMax) {
    display.setCursor(0,24); 
    display.setTextSize(1); 
    display.setTextColor(SSD1306_WHITE);
    display.cp437(true);
    display.println(iCurrent + 1);  
  }
  display.display();
}
void loop() {
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
      if (BottoneMenu.isLongPushed()==true) {
        timeStartMenu=millis();
        programStatus = STATUS_MENU;
      } else {
        if(now.second() != previousSec) {
          if((now.second() % 5) == 0) {
            if (ReadTemp()) {
              displayTemp(temperature);
            }
          }
          displayTime(now.hour(),now.minute(),now.dayOfTheWeek());
          displayDate(now.day(), now.month(), now.year(),now.dayOfTheWeek());
          previousSec = now.second();
        }
      }
      break;
    case STATUS_MENU:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_RUNNING;
      } else {
        if ( (BottoneSu.isPushed()==true) && menuStatus > 0) {
          menuStatus--;
          timeStartMenu=millis();
        }
        if ( (BottoneGiu.isPushed()==true) && menuStatus < 2) {
          menuStatus++;
          timeStartMenu=millis();
        }
        if (BottoneIndietro.isPushed()==true) {
          display.clearDisplay();
          programStatus=STATUS_RUNNING;
          delay(200);
          break;
        }
        if (BottoneMenu.isPushed()==true) {
          display.clearDisplay();
          timeStartMenu=millis();
          switch(menuStatus) {
            case 0:
              break;
            case 1:
              programStatus = STATUS_SET_TEMP;
              break;
            case 2:
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
        if ( (BottoneSu.isPushed()==true) && menuTempStatus > 0) {
          menuTempStatus--;
          timeStartMenu=millis();
        }
        if ( (BottoneGiu.isPushed()==true) && menuTempStatus < 1) {
          menuTempStatus++;
          timeStartMenu=millis();
        }
        if (BottoneIndietro.isPushed()==true) {
          display.clearDisplay();
          programStatus=STATUS_MENU;
          delay(200);
          break;
        }
        if(BottoneMenu.isPushed()==true) {
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
        if ( (BottoneSu.isPushed()==true) && tempHigh > MIN_TEMPH) {
          tempHigh--;
          timeStartMenu=millis();
        }
        if ( (BottoneGiu.isPushed()==true) && tempHigh < MAX_TEMPH) {
          tempHigh++;
          timeStartMenu=millis();
        }
        if (BottoneIndietro.isPushed()==true) {
          display.clearDisplay();
          programStatus=STATUS_SET_TEMP;
          delay(200);
          break;
        }
        doMenuInt("Set Temp H", MIN_TEMPH, MAX_TEMPH, tempHigh);
      }
      break;
    case STATUS_SET_TEMPL:
      if(checkMenuTimeout()==true){
        programStatus=STATUS_MENU;
      } else {
        if ( (BottoneSu.isPushed()==true) && tempLow > MIN_TEMPL) {
          tempLow--;
          timeStartMenu=millis();
        }
        if ( (BottoneGiu.isPushed()==true) && tempLow < MAX_TEMPL) {
          tempLow++;
          timeStartMenu=millis();
        }
        if (BottoneIndietro.isPushed()==true) {
          display.clearDisplay();
          programStatus=STATUS_SET_TEMP;
          delay(200);
          break;
        }
        doMenuInt("Set Temp L", MIN_TEMPL, MAX_TEMPL, tempLow);
      }
      break;
  }
  delay(100);
}
