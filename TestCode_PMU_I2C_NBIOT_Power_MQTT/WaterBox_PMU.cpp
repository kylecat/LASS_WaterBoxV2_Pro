#include "WaterBox_PMU.h"
#include <LowPower.h>

#include <Adafruit_INA219.h>

#ifndef TwoWire_h
#include <Wire.h>
#endif // _WIRE_H
#ifndef _SIMCOM7000E_H_
#include "SIM7000.h"
#endif // _SIMCOM7000E_H_

#define  SLEEPTIME SLEEP_1S

uint8_t                  WaterBox_PMU::_Debug;
Stream& WaterBox_PMU::refSerial = Serial;
uint16_t                WaterBox_PMU::_i_for;
uint8_t                 WaterBox_PMU::_PowerSave = false;
uint16_t                WaterBox_PMU::_SleepSec;
WaterBox_PMU::IC2Request  WaterBox_PMU::_REQUEST;
String                  WaterBox_PMU::_ComStr;
String                  WaterBox_PMU::_recBuffer;

float                   WaterBox_PMU::_WakeUpVoltage = 3.6;
float                   WaterBox_PMU::Volate;
float                   WaterBox_PMU::Current;
enum WaterBox_PMU::STATE WaterBox_PMU::state = MASTER;
uint16_t                WaterBox_PMU::_CrtlPinMAIN = 2;
uint16_t                WaterBox_PMU::_CrtlPinNBIOT = 3;
uint16_t                WaterBox_PMU::_CrtlPinINA219 = 4;
uint8_t                 WaterBox_PMU::_Addr = 0x70;
uint8_t                 WaterBox_PMU::_INA219Addr = 0x40;

//SIM7000               WaterBox_PMU::_NB;
String                  WaterBox_PMU::ATCMD;
String                  WaterBox_PMU::Field_1;
String                  WaterBox_PMU::Field_2;
String                  WaterBox_PMU::Field_3;
String                  WaterBox_PMU::Field_4;
String                  WaterBox_PMU::Field_5;
String                  WaterBox_PMU::Field_6;
String                  WaterBox_PMU::Field_7;
String                  WaterBox_PMU::Field_8;

WaterBox_PMU::WaterBox_PMU() {
}

WaterBox_PMU::WaterBox_PMU(uint8_t _addr) {
  _Addr = _addr;
}

WaterBox_PMU::~WaterBox_PMU() {
}

void WaterBox_PMU::init(uint16_t _pinMain, uint16_t _pinNBIOT, uint16_t _pinINA219) {
  _CrtlPinMAIN = _pinMain;
  _CrtlPinINA219 = _pinINA219;
  _CrtlPinNBIOT = _pinNBIOT;
  pinMode(_CrtlPinMAIN, OUTPUT);
  pinMode(_CrtlPinINA219, OUTPUT);
  //_NB.begin(9600);
  //_NB.init(_CrtlPinNBIOT);
}

void WaterBox_PMU::setINA219(uint8_t _addr) {
  _INA219Addr = _addr;

  _Deguber(F("Setting INA219 I2C address: 0x"), H_PMU, NONE);
  _Deguber(String(_INA219Addr, HEX), NONE, EOL);
}

void WaterBox_PMU::setDebuger(Stream& refSer) {
  _Debug = true;
  refSerial = refSer;
  //_NB.setDebuger(refSer);
}

void WaterBox_PMU::setWakeUpVolate(float _v) {
  _WakeUpVoltage = _v;
  _Deguber(F("Setting WAKE UP Volate: "), H_PMU);
  _Deguber(String(_WakeUpVoltage), NONE);
  _Deguber(F(" V."), NONE, EOL);
}

void WaterBox_PMU::setSleepSec(uint32_t _sec) {
  _SleepSec = _sec;

  _Deguber(F("Setting Sleep Time: "), H_PMU);
  _Deguber(String(_SleepSec), NONE);
  _Deguber(F(" sec."), NONE, EOL);
}

// 開始睡眠：切換到Master，關閉供電後開始進入睡眠循環，循環結束時更新電池狀態，低於喚醒電源後再開始供電返回1 (可以用while(Sleep())一值睡
uint8_t WaterBox_PMU::Sleep(STATE _state = SLAVER) {

  LED(1, 700);
  _Deguber(F("Prepare SLEEP Mode"), H_PMU, EOL);
  _Deguber(F("Off the LED & Control POWER"), H_PMU, EOL);

  digitalWrite(LED_BUILTIN, LOW);
  ControlPower(OFF);

  _Deguber(F("Enter SLEEP Mode"), H_PMU, EOL);
  delay(100);

  for (_i_for = 0; _i_for < _SleepSec; _i_for++)
  {
    if (_PowerSave) LowPower.powerDown(SLEEPTIME, ADC_OFF, BOD_OFF);
    else                  delay(1000);
  }

  state = MASTER;
  _Deguber(F("Waker up, init I2C with Master and get bettery state..."), H_PMU, EOL);

  ControlPower(ON);
  getBetteryState();
  ControlPower(OFF);

  // 確認電池電壓是否足夠，低於限值則回傳True
  if (Volate < _WakeUpVoltage)
  {
    _Deguber(F("ERROR: Volate < Wake up voltage"), H_PMU);
    _Deguber(String(_WakeUpVoltage), NONE, EOL);
    return 1;
  }
  else
  {
    // 喚醒WaterBox，並把I2C控制權給WaterBox(I2C設定為slaver)
    if (_state == SLAVER)
    {
      _Deguber(F(" I2C --> Slaver"), H_PMU, EOL);
      _SwitchToSlaver(_Addr);
      ControlPower(ON);
    }
    return 0;
  }
}

void WaterBox_PMU::PowerSaveMode(POWER _state) {
  if (_state == ON)            _Deguber(F("Power Save Mode: ON"), H_PMU, EOL);
  else if (_state == OFF)        _Deguber(F("Power Save Mode: OFF"), H_PMU, EOL);
  _PowerSave = _state;
}

void WaterBox_PMU::ControlPower(POWER _state) {

  if (_state == ON) {
    digitalWrite(_CrtlPinMAIN, HIGH);
    _Deguber(F("Control: ON"), H_PMU, EOL);
  }
  else if (_state == OFF) {
    digitalWrite(_CrtlPinMAIN, LOW);
    _Deguber(F("Control: OFF"), H_PMU, EOL);
  }

}

void WaterBox_PMU::getBetteryState() {

  digitalWrite(_CrtlPinINA219, HIGH);

  Wire.begin();

  uint32_t currentFrequency;
  Adafruit_INA219 bettery(_INA219Addr);

  bettery.begin();
  // To use a slightly lower 32V, 1A range (higher precision on amps):
  //bettery.setCalibration_32V_1A();
  // Or to use a lower 16V, 400mA range (higher precision on volts and amps):
  bettery.setCalibration_16V_400mA();
  Volate = bettery.getBusVoltage_V();
  Current = bettery.getCurrent_mA();

  Wire.end();
  digitalWrite(_CrtlPinINA219, LOW);

  _Deguber(F("Bettery Volate: "), H_PMU);
  _Deguber(String(Volate), NONE);
  _Deguber(F(", Current: "), NONE);
  _Deguber(String(Current), NONE, EOL);
}

void WaterBox_PMU::LED(uint16_t _times, uint16_t _ratio) {
  pinMode(LED_BUILTIN, OUTPUT);
  for (_i_for = 0; _i_for < _times; _i_for++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(_ratio);
    digitalWrite(LED_BUILTIN, LOW);
    delay(300);
  }
}

void WaterBox_PMU::ATClear()
{
  ATCMD = "";
}

void WaterBox_PMU::_Deguber(String _msg, UART _header, UART _uart = NONE) {
  if (_Debug) {
    switch (_header)
    {
    case H_PMU:
      refSerial.print(F("[PMU]\t"));
      break;
    case H_I2C:
      refSerial.print(F("[I2C]\t"));
      break;
    case H_CMD:
      refSerial.print(F("[CMD]\t"));
      break;
    default:
      break;
    }

    switch (_uart)
    {
    case NONE:
      refSerial.print(_msg);
      break;
    case EOL:
      refSerial.println(_msg);
      break;
    default:
      break;
    }
  }
}

void WaterBox_PMU::_requestEvent()
{
  // 按照_cmd解析後的指令狀態(_REQUEST) 回傳30 char長度的資料，多餘的長度用" "填滿
  switch (_REQUEST)
  {
  case Req_NONE:
    _recBuffer = F("No String");
    break;
  case Req_TIME:
    _recBuffer = F("OK,timer -> ");
    _recBuffer += String(_SleepSec);
    _recBuffer += F(" sec");
    break;
  case Req_SLEEP:
    _recBuffer = F("Sleep");
    break;
  case Req_LOWPOWER:
    _recBuffer = F("OK, LowPower -> ");
    _recBuffer += String(_PowerSave);
    break;
  case Req_BAT_VOLATE:
    _recBuffer = F("OK,");
    _recBuffer += String(Volate);
    break;
  case Req_BAT_CURRENT:
    _recBuffer = F("OK,");
    _recBuffer += String(Current);
    break;
  case Req_AT:
    _recBuffer = F("OK");
    break;
  default:
    _recBuffer = F("Command ERROR");
    break;
  }

  uint16_t _BufferSize = _recBuffer.length();

  for (_i_for = 0; _i_for + _BufferSize < _I2C_BUFFER_SIZE; _i_for++)
  {
    _recBuffer += F(" ");
  }

  _Deguber(_recBuffer, H_I2C, EOL);
  _Deguber("", NONE, EOL);

  Wire.write(_recBuffer.c_str());
}

void WaterBox_PMU::_receiveEvent(int howMany)
{
  // 收到WaterBox I2C的指令後，丟給_cmd()處理
  _ComStr = "";

  while (Wire.available())
  {
    char _c_ = Wire.read();
    _ComStr += String(_c_);
  }
  LED(1, 300);
  _cmd(_ComStr);
}

void WaterBox_PMU::_cmd(String _str)
{
  /*
  TIME,sec         // 設定鬧鐘(秒)
  SLEEP             // 進入睡眠
  LOWPOWER,1  // 省電模式
  VOLATE           // 回傳 volate
  CURRENT        // 回傳 current
  ATCMD,***** //  輸入AT Commend 資料
  */

  _str.toUpperCase();
  _ComStr = _str.substring(_str.indexOf(F(",")) + 1);  // 重新利用_ComStr

  if (_str.length() > 0) _REQUEST = Req_ERROR;
  else                         _REQUEST = Req_NONE;

  if (_str.indexOf(F("TIME")) > -1)
  {
    _Deguber(F("TIME -> "), H_CMD);
    _Deguber(_ComStr, NONE, EOL);
    _SleepSec = _ComStr.toInt();
    setSleepSec(_SleepSec);
    _REQUEST = Req_TIME;
  }
  if (_str.indexOf(F("SLEEP")) > -1)
  {  // 先睡著，醒了才回傳資料(???)
    _Deguber(F("get SLEEP command"), H_CMD, EOL);
    _Deguber(F("Control Swtich To PMU"), H_CMD, EOL);
    state = MASTER;
    //Sleep();
    _REQUEST = Req_SLEEP;
  }
  if (_str.indexOf(F("LOWPOWER,")) > -1)
  {
    _Deguber(F("LOWPOWER -> "), H_CMD);
    _Deguber(_ComStr, NONE, EOL);
    _PowerSave = _ComStr.toInt();
    PowerSaveMode(POWER(_PowerSave));
    _REQUEST = Req_LOWPOWER;
  }
  if (_str.indexOf(F("VOLATE,")) > -1)
  {
    _Deguber(F("GET BATTERY VOLATE"), H_CMD, EOL);
    _REQUEST = Req_BAT_VOLATE;
  }
  if (_str.indexOf(F("CURRENT")) > -1)
  {
    _Deguber(F("GET BATTERY CURRENT"), H_CMD, EOL);
    _REQUEST = Req_BAT_CURRENT;
  }
  if (_str.indexOf(F("ATCMD,")) > -1)
  {
    ATClear();
    _Deguber(F("ATCMD >> "), H_CMD);
    _Deguber(_ComStr, NONE, EOL);
    ATCMD = _ComStr;
    _REQUEST = Req_AT;
  }
  if (_str.indexOf(F("F1,")) > -1)
  {
    _Deguber(F("Set Field 1>> "), H_CMD);
    _Deguber(_ComStr, NONE, EOL);
    Field_1 = _ComStr;
  }
  if (_str.indexOf(F("F2,")) > -1)
  {
    _Deguber(F("Set Field 2>> "), H_CMD);
    _Deguber(_ComStr, NONE, EOL);
    Field_2 = _ComStr;
  }
  if (_str.indexOf(F("F3,")) > -1)
  {
    _Deguber(F("Set Field 3>> "), H_CMD);
    _Deguber(_ComStr, NONE, EOL);
    Field_3 = _ComStr;
  }

  if (_str.indexOf(F("F4,")) > -1)
  {
    _Deguber(F("Set Field 4>> "), H_CMD);
    _Deguber(_ComStr, NONE, EOL);
    Field_4 = _ComStr;
  }

  if (_str.indexOf(F("F5,")) > -1)
  {
    _Deguber(F("Set Field 5>> "), H_CMD);
    _Deguber(_ComStr, NONE, EOL);
    Field_5 = _ComStr;
  }

  if (_str.indexOf(F("F6,")) > -1)
  {
    _Deguber(F("Set Field 6>> "), H_CMD);
    _Deguber(_ComStr, NONE, EOL);
    Field_6 = _ComStr;
  }

  if (_str.indexOf(F("F7,")) > -1)
  {
    _Deguber(F("Set Field 7>> "), H_CMD);
    _Deguber(_ComStr, NONE, EOL);
    Field_7 = _ComStr;
  }

  if (_str.indexOf(F("F8,")) > -1)
  {
    _Deguber(F("Set Field 8>> "), H_CMD);
    _Deguber(_ComStr, NONE, EOL);
    Field_8 = _ComStr;
  }
}

void WaterBox_PMU::_SwitchToSlaver(uint8_t _addr) {
  Wire.begin(_addr);
  Wire.onRequest(&WaterBox_PMU::_requestEvent);  // I2C Master的指令：回傳
  Wire.onReceive(&WaterBox_PMU::_receiveEvent);   // I2C Master的指令：不回傳
  state = SLAVER;
}
