#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// настройки сети Wi-Fi
const char* ssid = "...";
const char* password = "...";
// адрес сервера MQTT и данные аутентификации
const char* mqtt_server = "192.168.1.100"; 
const char* MQTT_LOGIN  = "...";
const char* MQTT_PASS   = "...";

#define ID_CONNECT	"Boiler_Thermex"
#define LED     	2

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
// установленная температура (храним для случая, чтобы при реконнекте к серверу MQTT заново 
// не выставлять температуру на бойлере, т.к. она уже установлена
int set_temperature = 0; 



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
  ArduinoOTA.setPassword("...");
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
      //client.publish("home/Boiler/connection",    	" ");
      //client.publish("home/Boiler/get/temperature",	" ");
      //client.publish("home/Boiler/set/temperature",	" ");
      //client.publish("home/Boiler/set/power",         " ");
      //client.publish("home/Boiler/get/power",         " ");
      //client.publish("home/Boiler/get/single_power",	" ");
      //client.publish("home/Boiler/get/double_power",	" ");
      //client.publish("home/Boiler/get/preservation",	" ");
      client.publish("home/Boiler/set/power_selector",  "false");
      client.publish("home/Boiler/set/temp_selector",   "false");
      
      client.subscribe("home/Boiler/set/#");
      client.subscribe("home/Boiler/connection");
      
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

  // запрос доступности бойлера в сети
  if (strTopic == "home/Boiler/connection") {
    if (strPayload != "true") {
      needRefresh = true;
    }
  }

  if (strTopic == "home/Boiler/set/temperature") {
    int t = strPayload.toInt();
    if (t != set_temperature)
    {
      set_temperature = t; // сохраним установленную температуру
      if (t == 35 || t == 40 || t == 45 || t == 50 || t == 55 || t == 60 || t == 65 || t == 70 || t == 75) {
        Serial.print("cmd");
        Serial.println(t);
      }
    }
  }

  if (strTopic == "home/Boiler/set/power") {
    if (strPayload == "true" && power == false) {
      Serial.println("cmd1");
    } else if (strPayload == "false" && power == true) {
      Serial.println("cmd1");
    }
  }

  if (strTopic == "home/Boiler/set/power_selector") {
    if (strPayload == "true") {
      client.publish("home/Boiler/set/power_selector", "false");
      if (power == true) {
        Serial.println("cmd2");
      }
    }
  }

  if (strTopic == "home/Boiler/set/temp_selector") {
    if (strPayload == "true") {
      client.publish("home/Boiler/set/temp_selector", "false");
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

  // выставлен флаг refresh, обновляем данные на сервере MQTT
  if (needRefresh) {
    client.publish("home/Boiler/connection", "true");
    client.publish("home/Boiler/get/power", (power) ? "true" : "false");
    client.publish("home/Boiler/get/temperature", String(temp).c_str());
    client.publish("home/Boiler/get/single_power", (sngl > 0) ? "true" : "false");
    client.publish("home/Boiler/get/double_power", (dbl > 0) ? "true" : "false");
    client.publish("home/Boiler/get/preservation", (prsv > 0) ? "true" : "false");
    needRefresh = false;
  }

}
