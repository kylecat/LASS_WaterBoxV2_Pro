/***** << WaterBox_V2.1 Pro:WatchDog with OLED and SD>> *****
  PCB borad：WaterBox V2.1
  功能：每次 loop後睡眠5秒，啟動時依序測試模組
  測試項目：
    1.OLED
    2.SD
    3.Tempture
**************************************************************/
#include <LWatchDog.h>

/***** << LinkIt 7697 EEPROM library >> *****/
#include <EEPROM.h>           // EEPROM library

/***** << OLED library: u8g2 >> *****/
#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

//U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

/***** << SD library >> *****/
#include <SPI.h>
#include <SD.h>

File myFile;
const int chipSelect = 4;


/**** << WaterBox_V2.1:DS3231 時間模組 >> *****
    功能：時間模組+EEPROM
    library：RTClib.h
    版本：1.2.0
***********************************************************/
#include <RTClib.h>           // DS3231 library
RTC_DS3231 rtc;


/**** << WaterBox_V2.1:DS18B20 溫度模組 >> *****
    library：DS18B20.h
    版本：1.0.0
***********************************************************/
#define DS18B20_Pin 7
#include <DS18B20.h>
uint8_t address[] = {40, 250, 31, 218, 4, 0, 0, 52};


/**** << WaterBox_V2.1: pin state test with ADS1115 >> *****
     ADC 數據及 switch 切換
     使用Library Adafruit_ADS1X15 (Ver. 1.0.1)
***********************************************************/
#include <Wire.h>
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
int print_interval = 1000;

int _c = 0;
float Temp_value, pH_value, EC_value, Tubidity_value, DO_value;

int chapter = 0;                               //  大項目：0~1(校正設定,時間設定)
int item = 0;                                  //  子項目：0~3(準備,參數1設定,參數2設定,參數3設定)
bool config_state = true;                      //  true時只能顯示目前設定，flase時可以改設定
int _YY, _year_1, _year_2, _MM, _DD, _HH, _mm; //  時間日期調整用

int _year, _month, _day, _hour, _minute;       //  系統時間用

String CSV_Header;   // 寫入CSV時的表頭
String CSV_fileName; // 檔案名稱(需少於8個字元)
String CSV_Data;     // CSV資料
String str_Time;     // 資料寫入時的時間  "YYYY-MM-DD mm:hh"



/***** << OLED function >> *****/
void OLED_msg(String _verMsg, float _delay = 0.5, bool _savePower = true)
{
  u8g2.setFlipMode(0);
  if (_savePower) u8g2.setPowerSave(0); // 打開螢幕

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_8x13B_tf);                   // 一個字母8 Pixel 寬，行高12
    u8g2.drawStr(24, 12, ">> LASS <<");                 // 16-10 = 6, 6*8=48 -> 24 開始

    u8g2.setFont(u8g2_font_timR08_tr);                 // 一個字母8 Pixel 寬，行高12
    u8g2.drawStr(80, 20, _fw.c_str());                 // 16-13 = 3, 3*8=24 -> 12 開始

    u8g2.setFont(u8g2_font_helvR14_te);                // 一個字母8 Pixel 寬，行高12
    u8g2.drawStr(0, 40, _verMsg.c_str());              // 16-13 = 3, 3*8=24 -> 12 開始
  } while ( u8g2.nextPage() );

  delay(_delay * 1000);
  if (_savePower) u8g2.setPowerSave(1); // 關閉螢幕
}

void OLED_content(String _msg1, String _msg2 = "", float _delay = 0.5, bool _savePower = true)
{
  u8g2.setFlipMode(0);
  if (_savePower) u8g2.setPowerSave(0); // 打開螢幕

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_8x13B_tf);                   // 字母寬10Pixel，高13Pixel，行高12
    u8g2.drawStr(24, 12, ">> LASS <<");                 // LASS logo

    u8g2.setFont(u8g2_font_timR08_tr);                  // 5x8 字母寬5Pixel，高8Pixel，行高10
    u8g2.drawStr(60, 20, _fw.c_str());                  // 韌體版本(靠右)

    u8g2.setFont(u8g2_font_helvR14_te);                 // 18x23 字母寬18Pixel，高23Pixel，行高20
    u8g2.drawStr(0, 40, _msg1.c_str());                 // 訊息A
    u8g2.drawStr(0, 60, _msg2.c_str());                 // 訊息B
  } while ( u8g2.nextPage() );

  delay(_delay * 1000);
  if (_savePower) u8g2.setPowerSave(1); // 關閉螢幕
}

void OLED_content_title(String _title, String _msg1, String _msg2 = "", float _delay = 0.5, bool _savePower = true)
{
  u8g2.setFlipMode(0);
  if (_savePower) u8g2.setPowerSave(0); // 打開螢幕

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_8x13B_tf);                   // 字母寬10Pixel，高13Pixel，行高12
    u8g2.drawStr(24, 12, ">> LASS <<");                 // LASS logo

    u8g2.setFont(u8g2_font_timR08_tr);                  // 5x8 字母寬5Pixel，高8Pixel，行高10
    u8g2.drawStr(0, 20, _title.c_str());                // 左邊的小title
    u8g2.drawStr(60, 20, _fw.c_str());                  // 韌體版本(靠右)

    u8g2.setFont(u8g2_font_helvR14_te);                 // 18x23 字母寬18Pixel，高23Pixel，行高20
    u8g2.drawStr(0, 40, _msg1.c_str());                 // 訊息A
    u8g2.drawStr(0, 60, _msg2.c_str());                 // 訊息B
  } while ( u8g2.nextPage() );

  delay(_delay * 1000);
  if (_savePower) u8g2.setPowerSave(1); // 關閉螢幕
}

void OLED_smallContent(String _msg1, String _msg2 = "", String _msg3 = "", float _delay = 0.5, bool _savePower = true)
{
  u8g2.setFlipMode(0);                 // 是否翻轉螢幕
  u8g2.setPowerSave(0);                // 打開螢幕

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_8x13B_tf);                   // 字母寬10Pixel，高13Pixel，行高12
    u8g2.drawStr(24, 12, ">> LASS <<");                 // LASS logo

    u8g2.setFont(u8g2_font_timR08_tr);                  // 5x8 字母寬5Pixel，高8Pixel，行高10
    u8g2.drawStr(60, 20, _fw.c_str());                  // 韌體版本(靠右)

    u8g2.setFont(u8g2_font_helvR12_te);                 // 15x20 字母寬15Pixel，高20Pixel，行高15
    u8g2.drawStr(0, 34, _msg1.c_str());                 // 訊息A
    u8g2.drawStr(0, 49, _msg2.c_str());                 // 訊息B

    u8g2.setFont(u8g2_font_6x13O_tf );                  // 5x8 字母寬5Pixel，高6Pixel，行高10
    u8g2.drawStr(0, 62, _msg3.c_str());                 // 訊息C 最底層小字
  } while ( u8g2.nextPage() );

  delay(_delay * 1000);
  if (_savePower)u8g2.setPowerSave(1); // 關閉螢幕
}



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


/***** << RTC function >> *****/
bool initRTC(bool _setTime = false)
{
  bool _state = true;
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    _state = false;
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    //    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // rtc.adjust(DateTime(2020, 7, 11, 7, 31, 0));
  }
  if (_setTime) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  return _state;
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


/***** << GPIO function >> *****
   計算某pin上拉一次花多少秒(0.1)
   並設定上限秒數超過自動跳出回傳
********************************/
float pull_time(uint8_t pin, int _limit)
{
  bool _virutal_button = false;             // 紀錄按鈕狀態用
  int _while_i = 0;                         // 中斷while用
  unsigned long _push_time = 0;             // 按下時的時間(暫存)
  float _duration_time = 0;                 // 總經歷秒數
  bool _btn_state = digitalRead(pin);       // 確認按鈕有沒有被按下

  for (int _i = 0 ; _i < 2; _i++) {         //  跑兩遍，第一遍用來確認按鈕狀態，第二遍用來結算時間
    if (_btn_state) {                       // 按鈕被按下的話，確認虛擬按鈕的狀態
      _virutal_button = true;               // 按下虛擬按鈕
      _push_time = millis();                // 紀錄按下開始時間(millisecond)

      while (_btn_state) {                                                 // 當按鈕被按下時進入while迴圈，超過_limit秒數(*10ms)後自動中斷
        if (_while_i < _limit * 100)  _btn_state = digitalRead(pin);        // 用while停住程式，並持續更新按鈕狀態
        else {
          _btn_state = false;                                              // 跳出前先把按鈕狀態關掉，避免再次進入while
        }
        delay(10);
        _while_i++;
      } // end of while (按鈕確認)

    }
    else {                           // 按鈕彈起時，結算按住時間
      if (_virutal_button) {         // 如果是按鈕彈起後還沒更新虛擬按鈕，結算時間
        _duration_time = (millis() - _push_time) * 0.001;
      }
    }
  }
  return _duration_time;
}


/***** << GPIO function >> *****/
void SystemTest_GPIO(int _loop)
{
  Serial.print("LOOP:\t" + String(_loop));
  Serial.print("\tOn");
  delay(1000);

  digitalWrite(sensorSwitch, LOW);
  Serial.print("\tEC");
  delay(1000);
  digitalWrite(sensorSwitch, HIGH);
  Serial.print("\tpH");
  delay(1000);
  Serial.println("\tOff");
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


/***** << LinkIt 7697 EEPROM function >> *****
   LinkIt 7697 內部記憶體的讀寫
   bool EEPROM_write(char* _str, int _page, int _length)
***********************************/
bool EEPROM_write(char* _str, int _page, int _length) // 寫入資料，1頁 32 bytes
{
  int _address = _page * 32;
  if (_length > 31) {                // 超出頁面
    Serial.println("Out Of Pages");
    return false;
  }
  else {
    Serial.print("Writing data：");
    for ( int _i = 0; _i < _length; _i++ ) {
      EEPROM.update(_address + _i, _str[_i]);
      Serial.print(_str[_i]);
    }
    Serial.println();
    return true;
  } // end of if
} // end of EEPROM_write()

String EEPROM_read(int _page, int _length) // 讀取資料，1頁 30 bytes
{
  int _address = _page * 32;
  char _str;
  String read_buffer = "";

  if (_length > 31) {                         // 超出頁面
    Serial.println("Out Of Pages");
  }
  else {
    for ( int _i = 0; _i < _length; _i++ ) {
      _str = EEPROM.read(_address + _i);
      read_buffer += (String)_str;
    }
  }
  return read_buffer;
} // end of EEPROM_read()


/***** << System config function >> *****/
void _config(bool _state = true)
{
  String _buffer;
  char _char_buffer[30];

  if (_state)  Serial.println("讀取設定....");
  else        Serial.println("開始更新設定");

  for (int _i = 0; _i < 6; _i++) {

    if (_state) {
      _buffer = EEPROM_read(_i, 10);    // Linklt 7697 內部的EE{ROM)
      Serial.print(_buffer);
    }

    switch (_i) {
      case 0:
        if (_state) pH_slop = _buffer.toFloat();
        else _buffer = (String)pH_slop;
        break;
      case 1:
        if (_state) pH_intercept = _buffer.toFloat();
        else       _buffer = (String)pH_intercept;
        break;
      case 2:
        if (_state) EC_slop = _buffer.toFloat();
        else       _buffer = (String)EC_slop;
        break;
      case 3:
        if (_state) EC_intercept = _buffer.toFloat();
        else       _buffer = (String)EC_intercept;
        break;
      case 4:
        if (_state) pH_alarm = _buffer.toFloat();
        else       _buffer = (String)pH_alarm;
        break;
      case 5:
        if (_state) EC_alarm = _buffer.toFloat();
        else       _buffer = (String)EC_alarm;
        break;
    }// end of switch

    if (!_state) {
      _buffer.toCharArray(_char_buffer, _buffer.length() + 1);

      Serial.print("寫入資料");
      Serial.println(_char_buffer);
      EEPROM_write(_char_buffer, _i, _buffer.length() + 1);
      delay(50);
    }
    delay(100);
  }
  Serial.print("設定完成");
}

bool alarm_check(float pH, int EC)
{
  bool _pH_alarm = false, _EC_alarm = false;
  if (pH < 7 - pH_alarm || pH > 7 + pH_alarm)  _pH_alarm = true;
  if (EC > EC_alarm)  _EC_alarm = true;
  return _pH_alarm || _EC_alarm;
}


void SystemTest_SD(int _loop)
{
  SD.begin(chipSelect);
  Serial.println("建立資料夾");
  String _dirName = "Dir-" + String(_loop);
  SD_checkDir(_dirName);
  Serial.print("尋找資料夾(" + _dirName + "): ");
  Serial.println(SD.exists(_dirName));
  delay(5000);
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
  Serial.println("System Start after 5 second..");

  LWatchDog.begin(20);
  //  OLED_content("WatchDog", "Init", 1.0);

  delay(5000);

  pinMode(modulePower, OUTPUT);     //
  digitalWrite(modulePower, HIGH);  // 開啟模組電源

  pinMode(sensorSwitch, OUTPUT);    // 設定EC/pH切換控制IO
  digitalWrite(sensorSwitch, HIGH);
  delay(300);
  digitalWrite(sensorSwitch, LOW);

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

  initRTC(false);
  Serial.println("DS3231 初始化完成");

  u8g2.begin();
  //  OLED_content("OLED", "Init", 1.0);

  initADC();
  //  OLED_content("ADS1115", "Init", 1.0);

  digitalWrite(modulePower, HIGH); // 開幾電源
  OLED_content("Hello", "LASS", 1.0);
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

  OLED_smallContent("Data", "Sensoring", "Analysis Mode", 0.5);
  LWatchDog.feed();

  DS18B20 ds(DS18B20_Pin);
  Temp_value = ds.getTempC();
  LWatchDog.feed();

  digitalWrite(sensorSwitch, LOW);
  //  EC_value = getEC(Temp_value);
  EC_value = _getVoltage(EC_pin);
  LWatchDog.feed();

  digitalWrite(sensorSwitch, HIGH);
  //  pH_value = getPH();
  pH_value = _getVoltage(pH_pin);
  LWatchDog.feed();

  LWatchDog.feed();

  String msgTemp = " T:\t" + String(Temp_value, 2) + " C";
  String msgEC = "EC(Raw):" + String(EC_value, 2);
  String msgPH = "pH(Raw):" + String(pH_value, 2);

  OLED_smallContent(msgPH, msgEC, msgTemp, 2.0);

  Serial.println("[MCU] " + msgTemp);
  Serial.println("[MCU] " + msgPH);
  Serial.println("[MCU] " + msgEC);

  OLED_smallContent("RTC", "GetDateNow", "Analysis Mode", 0.5);
  /*****<< 取得時間資料 >>*****/
  DateTime now = rtc.now();          // 取得目前時間
  _year = now.year();
  _month = now.month();
  _day = now.day();
  _hour = now.hour();
  _minute = now.minute();
  LWatchDog.feed();

  str_Time = (String)_year + "-" + convert_2digits(_month) + "-" + convert_2digits(_day) + " " + convert_2digits(_hour) + ":" + convert_2digits(_minute);
  CSV_fileName = convert_2digits(_month) + convert_2digits(_day);
  CSV_Header   = "Date,Temp,EC,DO";
  CSV_Data = str_Time + "," + String(Temp_value) + "," + String(EC_value) + "," + String(DO_value);

  OLED_smallContent("SD Saving", str_Time, "Analysis Mode", 0.5);
  Serial.println("[MCU] " + str_Time);

  SavingData(CSV_fileName, CSV_Data);     //  寫入CSV
  LWatchDog.feed();

  OLED_smallContent("Data", "UpdateNow", "Analysis Mode", 0.5);
  Serial.println("[MCU] UpdateNow");
  msgTemp = "F1," + String(Temp_value, 2);
  msgEC = "F2," + String(EC_value, 2);
  msgPH = "F3," + String(pH_value, 2);
  String msgCMD = "SLEEP";

  sendMPU(msgTemp);
  LWatchDog.feed();

  sendMPU(msgEC);
  LWatchDog.feed();

  sendMPU(msgPH);
  LWatchDog.feed();

  sendMPU(msgCMD);
  LWatchDog.feed();
}
