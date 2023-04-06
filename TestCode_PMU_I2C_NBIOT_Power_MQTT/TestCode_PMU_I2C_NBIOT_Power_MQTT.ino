/*
  Name:    WaterBox_2.ino
  Created: 2020/3/10 下午 07:28:01
  Author:  Liu
*/
#include <Adafruit_INA219.h>
#include "WaterBox_PMU.h"
WaterBox_PMU PMU;


#include "SIM7000.h"
#define SIMCOM 3
SIM7000 NBIOT;

uint8_t _c = 0;
String _Data;

// LASS MQTT Config
#define Server_LASS "gpssensor.ddns.net"
#define Port_LASS   1883
#define MQTT_LASS_Topic "LASS/Test/WaterBox_TW/"

#define MSG_INIT "|device=Linkit7697|ver_app=1.1.0|tick=0|FAKE_GPS=1"
#define SIMCOM_MS 10
String AT_Buffer;

#define MSG_SIZE 300 // MQTT 訊息的上限
// String 轉換成 Char array 緩衝用
char STR_BUFFER[20];

// String 轉換成 Char array 緩衝用
uint32_t TOTAL_PL = 0; // 計算RL之前 用來統計整個訊息長度用
byte MQTT_RL[4];

String radom_str;  // 給client id 用

/***********<< 封包格式 >>**********
  CONNECT
  PUBLISH
*********************************/
struct PUBLISH {
  uint8_t Fix_Header = 0x30 | 0b0010; // 0x30(publish) | 0b0010 (QoS:1)| 0b0001 (Retain)
  byte RL[4];
  uint16_t RL_size;

  byte PL_topic[2];
  char TOPIC[40] = {'\0'};

  byte PL_msg[2];
  char MSG[MSG_SIZE] = {'\0'};
};

struct CONNECT {
  uint8_t Fix_Header = 0x10;
  byte RL[4];
  uint16_t RL_size;
  byte Flags = 0x02;                                     // Connect flags: 0b00000010 (Clean Session)
  byte Protocol[7] = {
    0x00, 0x04,                                          // Protocol Length: 4
    (byte)'M', (byte)'Q',  (byte)'T',  (byte)'T',        // Protocol Name:MQTT
    0x04                                                 // Protocol level (mqtt3.1.1 是0x04)
  };
  byte KeepAlive[2] = {0x00, 0x3C};                       // Keep Alive: 60 sec

  byte PL_id[2];
  char ID[20] = {'\0'};
};

CONNECT CONNECT_KPG;
PUBLISH PUBLISH_KPG;


/***********<< 資料整理 >>**********
  ZeroPadding
*********************************/
String ZeroHexPadding(byte _value) {
  String _buffer = "";

  if (_value < 16)  _buffer = "0" + String(_value, HEX);
  else              _buffer = String(_value, HEX);
  _buffer.toUpperCase();
  return _buffer;
}

void showByteBuffer(byte* _buffer, uint16_t _size, uint8_t _isChar = false) {
  for (uint16_t _i = 0; _i < _size; _i++) {
    if (_isChar) Serial.print((char)_buffer[_i]);
    else {
      Serial.print(F("0x"));
      Serial.print(ZeroHexPadding(_buffer[_i]));
      Serial.print(F(" "));
    }

    if (_i % 10 == 9) Serial.print(F("\r\n"));
    delay(10);
  }
  Serial.println(F("\r\n"));
}

String CharToHexStr(byte _char, uint8_t _show = false)
{
  byte _buffer = (byte) _char;
  if (_show) Serial.print(ZeroHexPadding(_buffer));
  return ZeroHexPadding(_buffer);
}

String ByteToHexStr(byte _byte, uint8_t _show = false)
{
  if (_show) Serial.print(ZeroHexPadding(_byte));
  return ZeroHexPadding(_byte);
}


/***********<< 封包格式 >>**********
  CalculateRL 計算RL, 填入MQTT_RL Buffer內，回傳用掉幾個bytes
*********************************/
uint8_t CalculateRL(uint32_t _size) {
  uint8_t _index = 0;
  memset(MQTT_RL, '\0', 4);  // 清空 Topic Buffer
  do
  {
    uint8_t _d = _size % 128;    // 取餘數 _size & 0b01111111
    _size /= 128;                // 取除以128後的整數：_size = _size >> 7;
    if (_size > 0)  _d |= 0x80;  // 除128的整數 > 0 時，對_d + 128 (bit[7]為1)
    MQTT_RL[_index] = _d;        // 計算好的 _d 放到 buffer內
    if (_index < 3)    _index++; // 避免寫入buffer爆掉，限制_index不超過3
  } while (_size > 0);
  return _index;
}

/***********<< 更新 BUFFER >>**********
  Flash_STR_BUFFER 更新 STR_BUFFER, 輸入String
*********************************/
bool Flash_STR_BUFFER(String _str) {
  if (_str.length() > 20) {
    Serial.print("[STR BUFFER ERROR]");
    Serial.print(_str.length());
    Serial.println("->" + _str);
    return false;
  }
  else {
    memset(STR_BUFFER, '\0', 20); // 清空 Buffer
    _str.toCharArray(STR_BUFFER, _str.length() + 1);
    return true;
  }
}

void setup()
{
  Serial.begin(9600);

  NBIOT.begin(9600);
  NBIOT.setDebuger(Serial);
  NBIOT.init(SIMCOM);

  PMU.init();
  PMU.setDebuger(Serial);
  // 5(SLEEP) + 20 (SIM7000 IDEL) + 60(GPS) + 35 (System)
  // 間隔20分鐘: 1085(SLEEP) + 20 (SIM7000 IDEL) + 60(GPS) + 35 (System)
  // 間隔最後在設定
  PMU.setSleepSec(5);

  PMU.setWakeUpVolate(3.4);
  PMU.PowerSaveMode(PMU.ON);
  PMU.setINA219(0x40);

  Serial.println(F("[System] Setup Done"));
  radom_str = (String)"_" + (char)random(65, 90) + (char)random(65, 90) + (char)random(97, 122) + (char)random(97, 122);

  PMU.Sleep();
}


void loop()
{
  //PMU.ControlPower(PMU.ON);
  //PMU.getBetteryState();

  if (PMU.state == PMU.MASTER)
  {
    PMU.ControlPower(PMU.OFF);
    Serial.println(F("[System] Upload to ThinkSpeak"));

    NBIOT.ON();
//    NBIOT.AT_Test();
    NBIOT.getGPS();

    /*********************
      準備上傳 ThingSpeak
    *********************/
    _Data = F("AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/update?api_key=JM921M08U3DHZ3JC");
    _Data += F("&field1=");
    _Data += String(PMU.Volate);
    _Data += F("&field2=");
    _Data += String(PMU.Current);
    _Data += F("&field3=");
    _Data += String(PMU.Field_1); // 溫度 s_t6
    _Data += F("&field4=");
    _Data += String(PMU.Field_2); // EC s_ec1
    _Data += F("&field5=");
    _Data += String(PMU.Field_3); // 溫度 s_Tb
    _Data += F("&field6=");
    _Data += String(NBIOT.GPSTimeTag);
    _Data += F("&field7=");
    _Data += String(NBIOT.Longitude);
    _Data += F("&field8=");
    _Data += String(NBIOT.Latitude);
    _Data += F("\"");

    /*********************
      上傳 ThingSpeak
    *********************/
    //    NBIOT.ON();
    //    NBIOT.AT_Test();

    NBIOT.AT_CMD(F("AT+CSTT=\"iot4ga2\""), true);
    NBIOT.AT_CMD(F("AT+CIICR"), true);
    NBIOT.AT_CMD(F("AT+SAPBR=3,1,\"Contype\", \"GPRS\""), true);
    NBIOT.AT_CMD(F("AT+SAPBR=3,1,\"APN\",\"iot4ga2\""), true);
    NBIOT.AT_CMD(F("AT+SAPBR=1,1"), true);

    NBIOT.AT_CMD(F("AT+HTTPINIT"), true);

    NBIOT.AT_CMD(F("AT+HTTPPARA=\"CID\",1"), true);

    NBIOT.AT_CMD(_Data, false);
    delay(500);

    NBIOT.AT_CMD(F("AT+HTTPACTION=0"), true);
    delay(20000);
    NBIOT.AT_CMD(F("AT+HTTPREAD"), true);
    NBIOT.AT_CMD(F("AT+HTTPTERM"), true);

    NBIOT.AT_CMD(F("AT+SAPBR=0,1"), true);
    NBIOT.AT_CMD(F("AT+CIPSHUT"), true);
    NBIOT.OFF();

    /*********************
      準備MQTT封包
    *********************/
    // 建立Topic
    memset(PUBLISH_KPG.TOPIC, '\0', 40);  // 清空 Topic Buffer
    strcat(PUBLISH_KPG.TOPIC, MQTT_LASS_Topic);
    if (Flash_STR_BUFFER("FT_WB_01"))   strcat(PUBLISH_KPG.TOPIC, STR_BUFFER); // 把(String)ID 放到STR_BUFFER中

    // 建立 從Field 建立ID 的String
    memset(CONNECT_KPG.ID, '\0', 20);       // 清空 ID Buffer
    strcat(CONNECT_KPG.ID, STR_BUFFER);     // 沿用 STR_BUFFER裡面的 MAC or DeviceID
    if (Flash_STR_BUFFER(radom_str)) strcat(CONNECT_KPG.ID, STR_BUFFER);


    // 開始打包MQTT封包
    memset(PUBLISH_KPG.MSG, '\0', MSG_SIZE);
    strcat(PUBLISH_KPG.MSG, MSG_INIT);

    // 日期
    if (Flash_STR_BUFFER(PMU.Field_4)) {  // F4,Date
      strcat(PUBLISH_KPG.MSG, "|date=");
      strcat(PUBLISH_KPG.MSG, STR_BUFFER);
    }
    // 時間
    if (Flash_STR_BUFFER(PMU.Field_5)) { // F5, Time
      strcat(PUBLISH_KPG.MSG, "|time=");
      strcat(PUBLISH_KPG.MSG, STR_BUFFER);
    }

    // Device ID
    if (Flash_STR_BUFFER("FT_WB_01")) {
      strcat(PUBLISH_KPG.MSG, "|device_id=");
      strcat(PUBLISH_KPG.MSG, STR_BUFFER);
    }

    // 經度
    if (Flash_STR_BUFFER(NBIOT.Longitude)) {
      strcat(PUBLISH_KPG.MSG, "|gps_lon=");
      strcat(PUBLISH_KPG.MSG, STR_BUFFER);
    }

    // 緯度
    if (Flash_STR_BUFFER(NBIOT.Latitude)) {
      strcat(PUBLISH_KPG.MSG, "|gps_lat=");
      strcat(PUBLISH_KPG.MSG, STR_BUFFER);
    }
    if (Flash_STR_BUFFER(PMU.Field_1))   {
      strcat(PUBLISH_KPG.MSG, "|s_t6=");
      strcat(PUBLISH_KPG.MSG, STR_BUFFER);
    }

    if (Flash_STR_BUFFER(PMU.Field_2))   {
      strcat(PUBLISH_KPG.MSG, "|s_ec1=");
      strcat(PUBLISH_KPG.MSG, STR_BUFFER);
    }

    if (Flash_STR_BUFFER(PMU.Field_3))   {
      strcat(PUBLISH_KPG.MSG, "|s_Tb=");
      strcat(PUBLISH_KPG.MSG, STR_BUFFER);
    }

    // 放電壓
    _Data = String(PMU.Volate);
    if (Flash_STR_BUFFER(_Data)) {
      strcat(PUBLISH_KPG.MSG, "|bat_v=");
      strcat(PUBLISH_KPG.MSG, STR_BUFFER);
    }

    // 放電流
    _Data = String(PMU.Current);
    if (Flash_STR_BUFFER(_Data)) {
      strcat(PUBLISH_KPG.MSG, "|bat_a=");
      strcat(PUBLISH_KPG.MSG, STR_BUFFER);
    }
    strcat(PUBLISH_KPG.MSG, "|");


    /********** < 打包 CONNECT_KPG > *********/
    CONNECT_KPG.PL_id[0] = strlen(CONNECT_KPG.ID) / 256;
    CONNECT_KPG.PL_id[1] = strlen(CONNECT_KPG.ID) % 256;

    TOTAL_PL = 10 + 2 + strlen(CONNECT_KPG.ID);                  // 計算RL長度 = 2+PL(id)
    CONNECT_KPG.RL_size =  CalculateRL(TOTAL_PL);                // 計算RL 用掉幾個bytes，並更新MQTT_RL 這個Buffer
    memcpy(CONNECT_KPG.RL, MQTT_RL, 4);                          // 把MQTT_RL 這個Buffer 複製到PUBLIC_KPG裡面


    /********** < 打包 PUBLISH_KPG > *********/
    // 計算 TOPIC 的 PL
    PUBLISH_KPG.PL_topic[0] = strlen(PUBLISH_KPG.TOPIC) / 256;
    PUBLISH_KPG.PL_topic[1] = strlen(PUBLISH_KPG.TOPIC) % 256;

    // 計算 MSG 的 PL
    PUBLISH_KPG.PL_msg[0] = strlen(PUBLISH_KPG.MSG) / 256;
    PUBLISH_KPG.PL_msg[1] = strlen(PUBLISH_KPG.MSG) % 256;

    // 計算整個RL
    TOTAL_PL = 2 + strlen(PUBLISH_KPG.TOPIC) + 2 + strlen(PUBLISH_KPG.MSG);  // 計算RL長度 = 2+PL(Topic)+2+PL(Payload)
    PUBLISH_KPG.RL_size =  CalculateRL(TOTAL_PL);                            // 計算RL 用掉幾個bytes，並更新MQTT_RL 這個Buffer
    memcpy(PUBLISH_KPG.RL, MQTT_RL, 4);                                      // 把MQTT_RL 這個Buffver 複製到PUBLIC_KPG裡面


    /*********************
          MQTT 傳輸
    *********************/
    NBIOT.ON();
    NBIOT.AT_CMD(F("AT+CIPSENDHEX=1"), true);               // 設定 TCP/IP的傳輸格式為HEX
    delay(500);
    NBIOT.AT_CMD(F("AT+CSTT=\"iot4ga2\""), true);
    delay(1500);
    NBIOT.AT_CMD(F("AT+CIICR"), true);                     // 啟動網路功能
    delay(1500);
    NBIOT.AT_CMD(F("AT+CIFSR"), true);                     // 取得IP: 測試用
    delay(1500);
    NBIOT.openNetwork(Server_LASS, Port_LASS);             // 開啟 TCP 連線，預留10秒緩衝
    delay(10000);

    NBIOT.AT_CMD("AT+CIPSEND", true);     // 開始傳輸內容, Concnet 及 publish 一起送
    Serial.println(F("[ >> ] Sending Data"));

    /********** < 逐一輸出 CONNECT_KPG 內容 > *********/
    /*
      順序很重要：
      1.FixHeader
      2.RL
      3.Protocol
      4.Connect flags
      5.KeepAlive
      6.Client ID
      7.User
      8.Password
    */
    Serial.println(F("\r\n[MQTT Concnet Package]"));

    // 1.FixHeader
    NBIOT.AT_print(ByteToHexStr(CONNECT_KPG.Fix_Header));
    NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);

    // 2.RL
    for (uint8_t _i = 0; _i < CONNECT_KPG.RL_size; _i++) {
      NBIOT.AT_print(ByteToHexStr(CONNECT_KPG.RL[_i]));
      NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);
    }
    // 3.Protocol
    for (uint8_t _i = 0; _i < 7; _i++) {
      NBIOT.AT_print(ByteToHexStr(CONNECT_KPG.Protocol[_i]));
      NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);
    }
    // 4.Connect flags
    NBIOT.AT_print(ByteToHexStr(CONNECT_KPG.Flags));
    NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);
    // 5.KeepAlive
    for (uint8_t _i = 0; _i < 2; _i++) {
      NBIOT.AT_print(ByteToHexStr(CONNECT_KPG.KeepAlive[_i]));
      NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);
    }
    // 6.Client ID
    for (uint8_t _i = 0; _i < 2; _i++) {
      NBIOT.AT_print(ByteToHexStr(CONNECT_KPG.PL_id[_i]));
      NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);
    }
    for (uint8_t _i = 0; _i < strlen(CONNECT_KPG.ID); _i++) {
      NBIOT.AT_print(CharToHexStr(CONNECT_KPG.ID[_i]));
      NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);
    }


    /********** < 逐一輸出 PUBLISH_KPG 內容> *********/
    Serial.println(F("\r\n[MQTT Data Package]"));

    NBIOT.AT_print(ByteToHexStr(PUBLISH_KPG.Fix_Header));
    NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);

    for (uint8_t _i = 0; _i < PUBLISH_KPG.RL_size; _i++) {
      NBIOT.AT_print(ByteToHexStr(PUBLISH_KPG.RL[_i]));
      NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);
    }

    for (uint8_t _i = 0; _i < 2; _i++) {
      NBIOT.AT_print(ByteToHexStr(PUBLISH_KPG.PL_topic[_i]));
      NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);
    }

    for (uint8_t _i = 0; _i < strlen(PUBLISH_KPG.TOPIC); _i++) {
      NBIOT.AT_print(CharToHexStr(PUBLISH_KPG.TOPIC[_i]));
      NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);
    }

    for (uint8_t _i = 0; _i < 2; _i++) {
      NBIOT.AT_print(ByteToHexStr(PUBLISH_KPG.PL_msg[_i]));
      NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);
    }

    for (uint8_t _i = 0; _i < strlen(PUBLISH_KPG.MSG); _i++) {
      NBIOT.AT_print(CharToHexStr(PUBLISH_KPG.MSG[_i]));
      NBIOT.AT_print(F(" ")); delay(SIMCOM_MS);
    }
    Serial.println();
    delay(100);

    NBIOT.AT_end(); // 發送封包
    delay(1000);

    NBIOT.closeNetwork(); // 關閉網路
    NBIOT.OFF();

    PMU.setSleepSec(10);
    PMU.Sleep();
  }
}
