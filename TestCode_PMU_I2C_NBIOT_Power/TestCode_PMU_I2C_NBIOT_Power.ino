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


void setup()
{
  Serial.begin(9600);

  NBIOT.begin(9600);
  NBIOT.setDebuger(Serial);
  NBIOT.init(SIMCOM);

  PMU.init();
  PMU.setDebuger(Serial);
  PMU.setSleepSec(5);
  PMU.setWakeUpVolate(3.2);
  PMU.PowerSaveMode(PMU.ON);
  PMU.setINA219(0x40);

  Serial.println(F("[System] Setup Done"));

  PMU.Sleep();
}

uint8_t _c = 0;
String _Data;

void loop()
{
  //PMU.ControlPower(PMU.ON);
  //PMU.getBetteryState();

  if (PMU.state == PMU.MASTER)
  {
    PMU.ControlPower(PMU.OFF);
    Serial.println(F("[System] Upload to ThinkSpeak"));

    NBIOT.ON();
    NBIOT.AT_Test();
    NBIOT.getGPS();

     /*********************
      準備上傳 ThingSpeak(舊)
    *********************/

//    _Data = F("AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/update?api_key=JM921M08U3DHZ3JC");
//    _Data += F("&field1=");
//    _Data += String(PMU.Volate);
//    _Data += F("&field2=");
//    _Data += String(PMU.Current);
//    _Data += F("&field3=");
//    _Data += String(PMU.Field_3);
//    _Data += F("&field4=");
//    _Data += String(PMU.Field_4);
//    _Data += F("&field5=");
//    _Data += String(PMU.Field_5);
//    _Data += F("&field6=");
//    _Data += String(PMU.Field_6);
//    _Data += F("&field7=");
//    _Data += String(PMU.Field_7);
//    _Data += F("&field8=");
//    _Data += String(PMU.Field_8);
//    _Data += F("\"");

    /*********************
      準備上傳 ThingSpeak(水保-水沙河)
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
    

//    NBIOT.AT_CMD(F("AT+CSTT=\"internet.iot\""), true);
    NBIOT.AT_CMD(F("AT+CSTT=\"iot4ga2\""), true);
    
    NBIOT.AT_CMD(F("AT+CIICR"), true);
    NBIOT.AT_CMD(F("AT+SAPBR=3,1,\"Contype\", \"GPRS\""), true);
//    NBIOT.AT_CMD(F("AT+SAPBR=3,1,\"APN\",\"internet.iot\""), true);
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

    PMU.setSleepSec(1060);  // 20分鐘一次循環，包含分析
    PMU.Sleep();
  }


  // delay(1000);

  //    NBIOT.ON();
  // delay(500);
  //NBIOT.OFF();
  // delay(500);
  /*
    String _Data = F("AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/update?api_key=RDCKV7AD5Q7Q03N3");
    _Data += F("&field1=");
    _Data += String(PMU.Volate);
    _Data += F("&field2=");
    _Data += String(PMU.Current);
    _Data += F("&field3=");
    _Data += String("");
    _Data += F("&field4=");
    _Data += String("");
    _Data += F("&field5=");
    _Data += String("");
    _Data += F("&field6=");
    _Data += String("");
    _Data += F("&field7=");
    _Data += String("");
    _Data += F("&field8=");
    _Data += String("");
    _Data += F("\"");

    NBIOT.ON();
    NBIOT.AT_Test();
    NBIOT.AT_CMD(F("AT+CSTT=\"nbiot\""), true);
    NBIOT.AT_CMD(F("AT+CIICR"), true);
    NBIOT.AT_CMD(F("AT+SAPBR=3,1,\"Contype\", \"GPRS\""), true);
    NBIOT.AT_CMD(F("AT+SAPBR=3,1,\"APN\",\"nbiot\""), true);
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

    PMU.Sleep();
  */
}
