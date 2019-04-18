
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

const char* ssid = "...";
const char* password = "...";
const char* mqtt_server = "192.168.1.100"; //Сервер MQTT
const char* MQTT_LOGIN  = "...";
const char* MQTT_PASS   = "...";

#define ID_CONNECT "Boiler_Thermex"
#define LED     2

WiFiClient espClient;
PubSubClient client(espClient);

// текущие данные
int temp = 0;
int prsv = 0;
int dbl = 0;
int sngl = 0;
int pwr = 0;
bool power = false;
// предыдущие данные
int temp_prev = 0;
int prsv_prev = 0;
int dbl_prev = 0;
int sngl_prev = 0;
bool power_prev = false;
// сохраненные данные
int temp_saved = 0;
int prsv_saved = 0;
int dbl_saved = 0;
int sngl_saved = 0;
bool power_saved = false;

bool needRefresh;
bool isChanged = false;
String inputString = "";



//-----------------------------------------------------------------------------------------
// Начальные установки
//-----------------------------------------------------------------------------------------
void setup() {
  pinMode(LED, OUTPUT);
  Serial.begin(19200);
  // Установка параметров Wi-Fi
  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    digitalWrite(LED, !digitalRead(LED)); // частое мигание при подключении к Wi-Fi
  }
  digitalWrite(LED, LOW);
  // Установка адреса сервера MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  // Установка параметров OTA
  ArduinoOTA.setHostname("Boiler_Thermex");
  ArduinoOTA.onStart([]() {  });
  ArduinoOTA.onEnd([]() {  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {  });
  ArduinoOTA.onError([](ota_error_t error) {  });
  ArduinoOTA.begin();
}




//-----------------------------------------------------------------------------------------
// Повторяем по кругу
//-----------------------------------------------------------------------------------------
void loop() {
  ArduinoOTA.handle();
  // Если соединение MQTT неактивно, то пытаемся установить его и опубликовать/подписаться
  if (!client.connected()) {
    digitalWrite(LED, !digitalRead(LED)); // в итоге выходит мигание с интервалом 5 сек при коннекте к серверу MQTT
    // Соединение с сервером MQTT публикация и подписка на данные
    if (client.connect(ID_CONNECT, MQTT_LOGIN, MQTT_PASS)) {
      client.publish("myhome/Boiler/connection",      " ");
      client.publish("myhome/Boiler/temperature",     " ");
      //client.publish("myhome/Boiler/set_temperature", "");
      //client.publish("myhome/Boiler/power",           "false");
      client.publish("myhome/Boiler/single_power",    " ");
      client.publish("myhome/Boiler/double_power",    " ");
      client.publish("myhome/Boiler/preservation",    " ");
      client.publish("myhome/Boiler/power_selector",  " ");
      client.publish("myhome/Boiler/temp_selector",   " ");
      client.subscribe("myhome/Boiler/#");
      digitalWrite(LED, LOW);
      needRefresh = true;
    } else {
      delay(5000);
    }
    // Cоединение MQTT активно...
  } else {
    // Проверка входящих соединений по подписке
    client.loop();
    // Слушаем серийный порт (данные от Arduino)
    if (Serial.available() > 0) {
      inputString = Serial.readStringUntil(':');
      if (inputString.length() == 9 || inputString.length() == 10) {
        parseString(); // разбор строки
      }
    }
  }
  yield();
}





//-----------------------------------------------------------------------------------------
// Получаем данные с сервера MQTT
// и преобразовываем в команды, которые посылаем по серийному порту для Arduino
//-----------------------------------------------------------------------------------------
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String strTopic = String(topic);
  String strPayload = String((char*)payload);

  // посылаем для видимости доступности бойлера в сети
  if (strTopic == "myhome/Boiler/connection") {
    if (strPayload == "false") {
      needRefresh = true;
    }
  }

  if (strTopic == "myhome/Boiler/set_temperature") {
    int t = strPayload.toInt();
    if (t == 35 || t == 40 || t == 45 || t == 50 || t == 55 || t == 60 || t == 65 || t == 70 || t == 75) {
      Serial.print("cmd");
      Serial.println(t);
    }
  }

  if (strTopic == "myhome/Boiler/power") {
    if (strPayload == "true" && power == false) {
      Serial.println("cmd1");
    } else if (strPayload == "false" && power == true) {
      Serial.println("cmd1");
    }
  }

  if (strTopic == "myhome/Boiler/power_selector") {
    if (strPayload == "true") {
      client.publish("myhome/Boiler/power_selector", "false");
      if (power == true) {
        Serial.println("cmd2");
      }
    }
  }

  if (strTopic == "myhome/Boiler/temp_selector") {
    if (strPayload == "true") {
      client.publish("myhome/Boiler/temp_selector", "false");
      Serial.println("cmd3");
    }
  }

}



//-----------------------------------------------------------------------------------------
// Парсинг строки, полученной от Arduino по серийному порту
// и отправка данных на сервер MQTT
//-----------------------------------------------------------------------------------------
void parseString() {
  //
  // inputString в формате:
  //
  // 35;0;0;1;1:
  // или
  // 9;0;0;1;1:
  //
  // температура может иметь 1 или 2 знака

  int p = inputString.indexOf(';');
  temp = inputString.substring(0, p).toInt();
  prsv = inputString.substring(p + 1, p + 2).toInt();
  dbl  = inputString.substring(p + 3, p + 4).toInt();
  sngl = inputString.substring(p + 5, p + 6).toInt();
  pwr  = inputString.substring(p + 7, p + 8).toInt();
  inputString = "";
  power = (pwr > 0) ? true : false;

  // флаг changed стоит, новые данные не изменились? значит будем обновлять: ставим флаг refresh
  // или сбросим флаг changed, т.к. это "случайные" данные (аля "дребезг контактов")
  if (isChanged) {
    if ((prsv == prsv_prev) && (dbl == dbl_prev) && (sngl == sngl_prev) && (power == power_prev)) {
      needRefresh = true;
      isChanged = false;
    } else {
      // восстановим сохраненное предыдущее (*)
      temp_prev = temp_saved;
      prsv_prev = prsv_saved;
      dbl_prev = dbl_saved;
      sngl_prev = sngl_saved;
      power_prev = power_saved;	
      isChanged = false;
    }
  }

  // произошло изменение данных, выставим флаг changed
  if (prsv != prsv_prev || dbl != dbl_prev || sngl != sngl_prev || power != power_prev || temp >= (temp_prev + 2) || temp <= (temp_prev - 2)) {
    // сохраним предыдущее на всяк случай (*)
    temp_saved = temp_prev;
    prsv_saved = prsv_prev;
    dbl_saved = dbl_prev;
    sngl_saved = sngl_prev;
    power_saved = power_prev;
    // предыдущее = текущему
    temp_prev = temp;
    prsv_prev = prsv;
    dbl_prev = dbl;
    sngl_prev = sngl;
    power_prev = power;
    isChanged = true;
  }

  // выставлен флаг refresh, оновляем данные на сервере MQTT
  if (needRefresh) {
    client.publish("myhome/Boiler/connection", "true");
    client.publish("myhome/Boiler/power", (power) ? "true" : "false");
    client.publish("myhome/Boiler/temperature", String(temp).c_str());
    client.publish("myhome/Boiler/single_power", (sngl > 0) ? "true" : "false");
    client.publish("myhome/Boiler/double_power", (dbl > 0) ? "true" : "false");
    client.publish("myhome/Boiler/preservation", (prsv > 0) ? "true" : "false");
    needRefresh = false;
  }
}
