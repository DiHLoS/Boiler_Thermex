#include <avr/wdt.h>

#define SIZEBUFF 40
#define MAXBUFF  20

#define BUTTON_ON_OFF 4
#define BUTTON_POWER_SELECTOR 5
#define BUTTON_TEMP_SELECTOR 6

byte data;
int strob;
int ArrayStrob[SIZEBUFF];
int ArrayData[SIZEBUFF];
int arraytemp[MAXBUFF];
int arrayind1[MAXBUFF];
int arrayind2[MAXBUFF];
int arrayind3[MAXBUFF];
//int pretemp = 99;
int tmp = 0;
int tmp1 = 0;
int tmp2 = 0;
int ind1 = 0;
int ind2 = 0;
int ind3 = 0;
int powerState = 0;
int currTemperature = 0;
int newTemperature;
bool isTempSetNow = false;
int count = 0;



//-----------------------------------------------------------------------------------------
// Начальные установки
//-----------------------------------------------------------------------------------------
void setup() {
  wdt_enable(WDTO_4S);
  Serial.begin(19200);
  DDRC = B00000000;
  PORTC = B00000000;
  DDRB &= ~_BV(0);
  DDRD &= ~_BV(2);
  DDRD &= ~_BV(3);
  pinMode(BUTTON_ON_OFF, OUTPUT);
  pinMode(BUTTON_POWER_SELECTOR, OUTPUT);
  pinMode(BUTTON_TEMP_SELECTOR, OUTPUT);
  digitalWrite(BUTTON_ON_OFF, HIGH);
  digitalWrite(BUTTON_POWER_SELECTOR, HIGH);
  digitalWrite(BUTTON_TEMP_SELECTOR, HIGH);
}



//-----------------------------------------------------------------------------------------
// Повторяем по кругу
//-----------------------------------------------------------------------------------------
void loop() {
  wdt_reset();
  getDataFromPanel();
  getCommandFromESP();
  if (isTempSetNow && count < 20){
    count++;
    setTemperature(newTemperature);
  } else {
    isTempSetNow = false;
    count = 0;
  }
}



//-----------------------------------------------------------------------------------------
// Получаем данные с панели(микроконтроллера) бойлера Thermex IF50V
//-----------------------------------------------------------------------------------------
void getDataFromPanel() {
  for (int m = 0; m < MAXBUFF; m++) {
    for (int i = 0; i < SIZEBUFF; i++) {
      ArrayStrob[i] = (PIND & (1 << PD2));
      ArrayData[i] = PINC;
      ArrayData[i] = (ArrayData[i] << 2) | (PINB & (1 << PB0));
      delay(1);
      getCommandFromESP();
      //Serial.println(ArrayData[i]);
    }
    for (int x = 0; x < SIZEBUFF - 1; x++) {
      int tempbuf = ArrayData[x];
      strob = ArrayStrob[x];
      getCommandFromESP();
      if (tempbuf == 144 && strob) {tmp1 = 0;}
      else if (tempbuf == 64  && strob) {tmp1 = 0;}
      else if (tempbuf == 121 && strob) {tmp1 = 1;}
      else if (tempbuf == 136 && strob) {tmp1 = 2;}
      else if (tempbuf == 40  && strob) {tmp1 = 3;}
      else if (tempbuf == 49  && strob) {tmp1 = 4;}
      else if (tempbuf == 36  && strob) {tmp1 = 5;}
      else if (tempbuf == 4   && strob) {tmp1 = 6;}
      else if (tempbuf == 120 && strob) {tmp1 = 7;}
      else if (tempbuf == 0   && strob) {tmp1 = 8;}
      else if (tempbuf == 32  && strob) {tmp1 = 9;}
      else if (tempbuf == 132) {tmp1 = 9;}
      else if (tempbuf == 64  && !strob) {tmp2 = 0;}
      else if (tempbuf == 121 && !strob) {tmp2 = 1;}
      else if (tempbuf == 136 && !strob) {tmp2 = 2;}
      else if (tempbuf == 40  && !strob) {tmp2 = 3;}
      else if (tempbuf == 49  && !strob) {tmp2 = 4;}
      else if (tempbuf == 36  && !strob) {tmp2 = 5;}
      else if (tempbuf == 4   && !strob) {tmp2 = 6;}
      else if (tempbuf == 120 && !strob) {tmp2 = 7;}
      else if (tempbuf == 0   && !strob) {tmp2 = 8;}
      else if (tempbuf == 32  && !strob) {tmp2 = 9;}
      else if (tempbuf == 245) {
        ind1 = 1;
        ind2 = 0;
        ind3 = 0;
      }
      else if (tempbuf == 252) {
        ind1 = 0;
        ind2 = 1;
        ind3 = 0;
      }
      else if (tempbuf == 249) {
        ind1 = 0;
        ind2 = 0;
        ind3 = 1;
      }
      if (tempbuf == 253) {
        ind1 = 0;
        ind2 = 0;
        ind3 = 0;
      }
      String Strtemp =  String(tmp1) + String(tmp2);
      tmp = Strtemp.toInt();
    }
    if (tmp > 15) {arraytemp[m] = tmp;}
    arrayind1[m] = ind1;
    arrayind2[m] = ind2;
    arrayind3[m] = ind3;
  }
  
  int maxtemp = arraytemp[0];
  int maxind1 = arrayind1[0];
  int maxind2 = arrayind2[0];
  int maxind3 = arrayind3[0];
  for (int mx = 1; mx < 10; mx++) {
    getCommandFromESP();
    if (arrayind1[mx] >= maxind1) {
      maxind1 = arrayind1[mx];
    }
    if (arrayind2[mx] >= maxind2) {
      maxind2 = arrayind2[mx];
    }
    if (arrayind3[mx] >= maxind3) {
      maxind3 = arrayind3[mx];
    }
  }
  
  wdt_reset();
  
  //Находим максимальные значения для отсеивания мусора
  int confidence = 0;
  int* candidate = NULL;
  for (int i = 0; i < MAXBUFF; i++) {
    getCommandFromESP();
    if (confidence == 0) {
      candidate = arraytemp + i;
      confidence++;
    }
    else if (*candidate == arraytemp[i]) {
      confidence++;
    }
    else {
      confidence--;
    }
  }
  if (confidence > 0 && candidate[1] > 15) {
    maxtemp = candidate[1];
  }
  currTemperature = maxtemp;
  
  if (maxind1 || maxind2 || maxind3){
    powerState = 1;
  } else {
    powerState = 0;
  }
  
  // Отправляем в серийный порт команды для ESP
  Serial.print(currTemperature); 	// текущая температура на индикаторе // глюки, если t < 10 oC
  Serial.print(";");
  Serial.print(maxind1); 			// состояние светодиода Temperature Preservation (L2)
  Serial.print(";");
  Serial.print(maxind2);			// состояние светодиода Double Power (L3)
  Serial.print(";");
  Serial.print(maxind3);			// состояние светодиода Single Power (L1)
  Serial.print(";");
  Serial.print(powerState);			// состояние бойлера включен/выключен
  Serial.print(":");
}



//-----------------------------------------------------------------------------------------
// Слушаем серийный порт и получаем команды от ESP
//-----------------------------------------------------------------------------------------
void getCommandFromESP() {
  if (Serial.available()) {
    if (Serial.find((char*)"cmd")) {
      int cmd = Serial.parseInt();
      if (cmd == 1) {
        pressButton(BUTTON_ON_OFF);
      } else if (cmd == 2) {
        pressButton(BUTTON_POWER_SELECTOR);
      } else if (cmd == 3) {
        pressButton(BUTTON_TEMP_SELECTOR);
      } else if (cmd == 35 || cmd == 40 || cmd == 45 || cmd == 50 || cmd == 55 || cmd == 60 || cmd == 65 || cmd == 70 || cmd == 75){
        newTemperature = cmd;
        setTemperature(newTemperature);
      }
    }
  }
}



//-----------------------------------------------------------------------------------------
// Установка указанной температуры (35, 40, 45, 50, 55, 60, 65, 70, 75)
// путем нажатия кнопки Temp Selector
//-----------------------------------------------------------------------------------------
void setTemperature(int t){
  if (currTemperature != t) {
        pressButton(BUTTON_TEMP_SELECTOR);
        delay(300);
        isTempSetNow = true;
  } else {
    isTempSetNow = false;
  }
}



//-----------------------------------------------------------------------------------------
// Нажатие указанной кнопки
//
// BUTTON_ON_OFF 4
// BUTTON_POWER_SELECTOR 5
// BUTTON_TEMP_SELECTOR 6
//-----------------------------------------------------------------------------------------
void pressButton(int b) {
  digitalWrite(b, LOW);
  delay(50);
  digitalWrite(b, HIGH);
}
