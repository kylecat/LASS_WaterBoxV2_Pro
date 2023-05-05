/***** << WaterBox_V2.:LSleep with OLED and SD>> *****
  PCB borad：WaterBox V2.1
  功能：每次 loop後睡眠5秒，啟動時依序測試模組
  測試項目：
    1.OLED
    2.SD
    3.Tempture
*/
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#include <LWatchDog.h>

/**** << WaterBox_V2.1:DS3231 時間模組 >> *****
    功能：時間模組+EEPROM
    library：RTClib.h
    版本：1.2.0
***********************************************************/
#include "RTClib.h"          // DS3231 library
RTC_DS3231 rtc;

/***** << SD library >> *****/
#include <SD.h>
File myFile;
const int chipSelect = 4;


/**** << WaterBox_V2.1:DS18B20 溫度模組 >> *****
    library：DS18B20.h
    版本：1.0.0
***********************************************************/
#define DS18B20_Pin 7
#include <OneWire.h>
OneWire  ds(DS18B20_Pin);  // on pin 10 (a 4.7K resistor is necessary)


/**** << WaterBox_V2.1: pin state test with ADS1115 >> *****
     ADC 數據及 switch 切換
     使用Library Adafruit_ADS1X15 (Ver. 1.0.1)
***********************************************************/
#include <Adafruit_ADS1015.h>
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
int16_t adc0, adc1, adc2, adc3;


/***** << firmware Version >> *****/
String _fw = "Ver 2.1.Pro";
String Description_Tittle = ">> LASS IS YOURS <<";
String Description_Firware = "The DEVICE FIRWARE: " + String(_fw);
String Description_Features = "THIS VERSION HAS THE　FOLLOWING FEATURES:\n\r"\
                              "\t 1.Get the EC and pH value via ADS1115 module\n\r"\
                              "\t 2.Get temptrue value via DS18B20 module\n\r"\
                              "\t 3.Save DATA with CSV format by Day\n\r"\
                              "\t 4.Upload Data to ThingSpeak cloud platform via NBIOT (Channel ID: 1095685)\n\r";
String Description_Precautions = "<< PRECAUTIONS >>\n\r"\
                                 "\t 1.All module power controled by P14\n\r"\
                                 "\t 2.LinkIt 7697 was not into deep sleep mode\n\r"\
                                 "\t 3.LoRa module did not used in this version\n\r";


#define pH_pin  0
#define EC_pin  1
#define Rotary_Pin_1    2  // VR1
#define Rotary_Pin_2    3  // VR2


/***** << 系統參數 >> *****/
#define modeSwitch 5
#define sensorSwitch 6
#define pinLED 7
#define modulePower 14

float pH_slop, pH_intercept, EC_slop, EC_intercept, pH_alarm, EC_alarm;
float Temp_value, pH_value, EC_value, Turbidity_value, DO_value;
int _year, _month, _day, _hour, _minute;       //  系統時間用

String CSV_Header;   // 寫入CSV時的表頭
String CSV_fileName; // 檔案名稱(需少於8個字元)
String CSV_Data;     // CSV資料
String str_Time;     // 資料寫入時的時間  "YYYY-MM-DD mm:hh"


/***** << SD function >> *****/
bool SD_checkDir(String _dirName)
{
  bool _stateCheck = SD.exists(_dirName);
  if (_stateCheck)
  {
    Serial.println("Directory <" + _dirName + "> exists");
  }
  else {
    Serial.println("Directory <" + _dirName + "> did not exist");
    _stateCheck = SD.mkdir(_dirName);
    Serial.print("Make a new directory:");
    if (_stateCheck) Serial.println("success");
    else             Serial.println("failed");
  }
  return _stateCheck;
}

bool SD_SaveCSV(String _dirName, String _fileName, String _data)
{
  bool _stateCheck = SD_checkDir(_dirName);
  String fileLocation = _dirName + "/" + _fileName + ".CSV";

  if (_stateCheck)  {
    _stateCheck = SD_WriteData(fileLocation, _data);
  }
  else            Serial.println("[SD ] Directory check failed");

  return _stateCheck;
}

bool SD_WriteData(String _fileName, String _data)
{
  bool _stateCheck;
  bool _hasFile = SD.exists(_fileName);

  myFile = SD.open(_fileName, FILE_WRITE);
  if (myFile)
  {
    Serial.print("[SD ] Saving Data: ");

    if (!_hasFile) myFile.println(CSV_Header);

    myFile.println(_data);
    myFile.close();

    Serial.println("Done");
    _stateCheck = true;
  }
  else
  {
    myFile.close();
    Serial.println("[SD ] error opening " + _fileName);
    _stateCheck = false;
  }
  return _stateCheck;
}

void SavingData(String _fileName, String _data)
{
  if (_fileName.length() > 8) {
    Serial.println("[SD ] 檔案名稱過長: " + _fileName);
    _fileName = _fileName.substring(0, 8);
  }
  SD.begin(chipSelect);
  SD_SaveCSV("DATA", _fileName, _data);  // 寫入資料
  delay(50);
}


/***** << ADS1115 function >> *****/
void initADC(void)
{
  ads.begin();
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  // ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
}

float _getVoltage(int _ch)
{
  //  initADC();
  delay(2000);
  int adc = ads.readADC_SingleEnded(_ch);
  float _mv = adc * 0.125 * 2;
  return _mv;
}

/*
  把特定的ADC腳位電壓，換算成0-val的數值後返回float格式
  rotary_button(指定的ADC pin, 要轉換的數值)
*/
float _analog_convert(uint8_t pin, int _val)
{
  //  int  _read = analogRead(pin);
  int  _read = ads.readADC_SingleEnded(pin);
  float _readVolt = _read * 0.125;

  //  float _value = _val * (_read * 0.125) / 3350.0;     // 0 ~ _val
  float _value = map(_readVolt, 0, 3350, 0, _val);            // _val ~ 0
  _value = _val - _value;

  if (_readVolt > 4000) _value = -1;

  return _value;
}


/***** << fromate function >> *****
   把int轉成2位數String，自動補0
***********************************/
String convert_2digits(int i)
{
  String number;
  if (i < 10) number = "0" + (String)i;
  else number = (String)i;
  return number;
}


/***** << DFRobot function >> *****
   DFRobot 模組用
   getPH  // 讀取pH
   getEC  // 讀取EC
***********************************/
float getPH(float slope = 1.0, float intercept = 0.0)
{
  float _ads_Volate = _getVoltage(pH_pin) * slope + intercept;
  float pH_Value = 3.5 * _ads_Volate * 0.001;
  return pH_Value;
}

double getECmV(float _mV, float _temp = 25.0)
{
  // 溫度補償係數
  float _value;
  float _temp_coefficient = 1.0 + 0.0185 * (_temp - 25.0);      //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.0185*(fTP-25.0));
  float _coefficient_volatge = _mV / _temp_coefficient;        // 電壓係數

  // 三個不同區間的導電度換算
  if (_coefficient_volatge < 150)
  {
    Serial.print("[EC ]No solution!(");
    Serial.print(_coefficient_volatge, 4);
    Serial.println(")");

    _value = 6.69 * _coefficient_volatge - 25.92;

    //    if (_value < 0) _value = 0;
  }
  else if (_coefficient_volatge > 3300)
  {
    _value = 5.3 * _coefficient_volatge + 2278; // 20ms/cm<EC (暫定)
    Serial.print("[EC ]Out of the range!(");
    Serial.print(_value);
    Serial.println(")");
  }
  else
  {
    if (_coefficient_volatge <= 448)        _value = 6.84 * _coefficient_volatge - 64.32; // 1ms/cm<EC<=3ms/cm
    else if (_coefficient_volatge <= 1457)  _value = 6.98 * _coefficient_volatge - 127;   // 3ms/cm<EC<=10ms/cm
    else                                    _value = 5.3 * _coefficient_volatge + 2278;   // 10ms/cm<EC<20ms/cm
  }
  return _value;
}

double getEC(float _temp, float slope = 1.0, float intercept = 0.0)
{
  double _ads_Volate;
  for (int _i = 0 ; _i < 10; _i++)
  {
    _ads_Volate += _getVoltage(EC_pin);
    delay(300);
  }
  _ads_Volate = _ads_Volate * 0.1;
  _ads_Volate = _ads_Volate * slope + intercept;

  return 1000 * getECmV(_ads_Volate, _temp);
}

float getTemp()
{ byte i;
  byte present = 0;
  byte type_s;
  byte data[9];
  byte addr[8];
  float celsius, fahrenheit;
  
  if ( !ds.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
  }
  
  // Serial.print("ROM =");
  // for( i = 0; i < 8; i++) {
  //   Serial.write(' ');
  //   Serial.print(addr[i], HEX);
  // }

  // if (OneWire::crc8(addr, 7) != addr[7]) {
  //     Serial.println("CRC is not valid!");
  // }
  // Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  delay(1000);              // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad
  // Serial.print("  Data = ");
  // Serial.print(present, HEX);
  // Serial.print(" ");

  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    // Serial.print(data[i], HEX);
    // Serial.print(" ");
  }
  // Serial.print(" CRC=");
  // Serial.print(OneWire::crc8(data, 8), HEX);
  // Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  // fahrenheit = celsius * 1.8 + 32.0;
  // Serial.print("  Temperature = ");
  // Serial.print(celsius);
  // Serial.print(" Celsius, ");
  // Serial.print(fahrenheit);
  // Serial.println(" Fahrenheit");
  return celsius;
}



void sendMPU(String _cmd)
{
  uint16_t MPUAddr = 0x70;
  Wire.beginTransmission(0x70);
  Wire.write(_cmd.c_str());
  Wire.endTransmission();
  delay(100);
}


/***** << Main function: Setup >> *****/
void setup(void)
{
  Serial.begin(9600);
  Serial.println("System Start");

  pinMode(modulePower, OUTPUT);     // 開啟模組電源
  digitalWrite(modulePower, HIGH);  

  pinMode(sensorSwitch, OUTPUT);    // 設定EC/pH切換控制IO
  digitalWrite(sensorSwitch, HIGH);
  delay(300);
  digitalWrite(sensorSwitch, LOW);

  rtc.begin();
  Serial.println("DS3231 初始化完成");
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2023, 1, 21, 3, 0, 0));
  }

  pinMode(10, OUTPUT);      // 手動控制LoRa 的CS
  digitalWrite(10, HIGH);   // 上拉LoRa SPI 的CS腳位，避免抓到LoRa

  Serial.print("\nInitializing SD card...");
  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card inserted?");
    Serial.println("* is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
  } else {
    Serial.println("SD Card Ready");
  }


  initADC();

  LWatchDog.begin(30);

  Serial.println();
  Serial.println("**************************************");
  Serial.println();
  Serial.println(Description_Tittle);
  Serial.println(Description_Firware);
  Serial.println("版本功能：");
  Serial.println("\t" + Description_Features);
  Serial.println("注意事項：");
  Serial.println("\t" + Description_Precautions);
  Serial.println();
  Serial.println("**************************************");
  Serial.println();

  LWatchDog.feed();
}


/***** << Main function: Setup >> *****/
void loop(void) {

  Temp_value =getTemp(); // 讀取兩次溫度，只有一次的話可能會出現錯誤
  if(Temp_value<1)
  {
    Temp_value =getTemp();
  }
  

  digitalWrite(sensorSwitch, LOW);
  //  EC_value = getEC(Temp_value);
  EC_value = _getVoltage(EC_pin);

  digitalWrite(sensorSwitch, HIGH);
  //  pH_value = getPH();
  Turbidity_value = _getVoltage(pH_pin);

  String msgTemp = " T:\t" + String(Temp_value, 2) + " C";
  String msgEC = "EC(Raw):" + String(EC_value, 2);
  String msgTurbidity = "Turbidity(Raw):" + String(Turbidity_value, 2);

  /*****<< 取得時間資料 >>*****/
  DateTime now = rtc.now();          // 取得目前時間
  _year = now.year();
  _month = now.month();
  _day = now.day();
  _hour = now.hour();
  _minute = now.minute();  
  str_Time = (String)_year + "-" + convert_2digits(_month) + "-" + convert_2digits(_day) + " " + convert_2digits(_hour) + ":" + convert_2digits(_minute);

  CSV_fileName = convert_2digits(_month) + convert_2digits(_day);
  CSV_Header   = "Date,Temp,EC,Turbidity";
  CSV_Data = str_Time + "," + String(Temp_value) + "," + String(EC_value) + "," + String(Turbidity_value);
  SavingData(CSV_fileName, CSV_Data);     //  寫入CSV
  LWatchDog.feed();

  // 顯示目前設定
  Serial.println(str_Time);
  Serial.println(msgTemp);
  Serial.println(msgEC);
  Serial.println(msgTurbidity);

// 改成給MCU的設定
  msgTemp = "F1," + String(Temp_value, 2);
  msgEC = "F2," + String(EC_value, 2);
  msgTurbidity = "F3," + String(Turbidity_value, 2);
  String msgCMD = "SLEEP";

  sendMPU(msgTemp);
  sendMPU(msgEC);
  sendMPU(msgTurbidity);
  sendMPU(msgCMD);
  LWatchDog.feed();
}
