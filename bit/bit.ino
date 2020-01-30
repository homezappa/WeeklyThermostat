

/* leggo o scrivo un bit, dato il giorno e l'ora */

char datiArray[7][3] = {
// 00,01,02,03,04,05,06,07,  08,09,10,11,12,13,14,15,  16,17,18,19,20,21,22,23 
    0B00000010, 0B00001100, 0B01111111, // Lunedi
    0B00000010, 0B00001100, 0B01111111, // Martedi
    0B00000010, 0B00001100, 0B01111111, // Mercoledi
    0B00000010, 0B00001100, 0B01111111, // Giovedi
    0B00000010, 0B00001100, 0B01111111, // Venerdi
    0B00000000, 0B00001111, 0B11111111, // Sabato
    0B00000000, 0B00001111, 0B11111111, // DOmenica
};

int readStatus(int d, int h) {
  return bitRead(datiArray[d][(h-(h%8))/8], h % 8);
}
void setStatus(int d, int h) {
  bitSet(datiArray[d][(h-(h%8))/8], h % 8);
}
void clearStatus(int d, int h) {
  bitClear(datiArray[d][(h-(h%8))/8], h % 8);
}
void flipStatus(int d, int h) {
  if (bitRead(datiArray[d][(h-(h%8))/8], h % 8) == 1) {
    bitClear(datiArray[d][(h-(h%8))/8], h % 8);
  } else {
    bitSet(datiArray[d][(h-(h%8))/8], h % 8);
  }
}


void setup() {
  // put your setup code here, to run once:
  setStatus(0,0);
  setStatus(5,12);

}

void loop() {
  // put your main code here, to run repeatedly:

}
