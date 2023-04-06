#include "SIM7000.h"
#ifndef  SoftwareSerial_h
#include "SoftwareSerial.h"
#endif // SoftwareSerial_h



Stream& SIM7000::_refSerial = Serial;

uint8_t		SIM7000::_Rx = AT_Rx;
uint8_t		SIM7000::_Tx = AT_Tx;

uint8_t		SIM7000::_Debug = false;
String		SIM7000::ResString = "";
String		SIM7000::_StrBuffer = "";

SoftwareSerial _ATSerial(SIM7000::_Rx, SIM7000::_Tx);


SIM7000::SIM7000() {
}

SIM7000::~SIM7000() {
}


void SIM7000::init(uint16_t _pin)
{
	_PowerPin = _pin;
	pinMode(_PowerPin, OUTPUT);
}

void SIM7000::begin(uint16_t _rate)
{
	_ATSerial.begin(_rate);
}

void SIM7000::setDebuger(Stream& refSer)
{
	_Debug = true;
	_refSerial = refSer;
}

void SIM7000::setAPN(APN _apnID){
	switch (_apnID){
	case cht_4G:
		_APN = "internet.iot";
		break;
	case fet_NB:
		_APN = "nbiot";
		break;
	case twm_NB:
		_APN = "twm.nbiot";
		break;
	default:
		_APN = "internet";
		break;
	}
}


void SIM7000::ON()
{
	digitalWrite(_PowerPin, HIGH);
	_Debuger(F("Power On, Waiting 20 sec.."), H_NBIOT, EOL);
	delay(20000);
}

void SIM7000::OFF()
{
	digitalWrite(_PowerPin, LOW);
	_Debuger(F("Power OFF, Waiting 1 sec.."), H_NBIOT, EOL);
	delay(1000);
}

uint8_t SIM7000::AT_Test()
{
	_boolBuffer = false;
	_AT(F("AT"));
	delay(1500);
	_ATReceive();
	return _boolBuffer;
}

uint8_t SIM7000::AT_CMD(String _cmd, uint8_t _info)
{
	_AT(_cmd);
	delay(1000);

	ResString = _ATReceive();
	if (_info)
	{
		_Debuger(F("Response -> "), H_NBIOT, NONE);
		_Debuger(ResString, NONE, EOL);
	}

	_boolBuffer = _ATState(ResString);

	return _boolBuffer;
}

void SIM7000::_Debuger(String _msg, UART _header, UART _uart){
	if (_Debug) {
		switch (_header)
		{
		case H_NBIOT:
			_refSerial.print(F("[NBIOT]\t"));
			break;
		case H_CMD:
			_refSerial.print(F("[CMD  ]\t"));
			break;
		default:
			break;
		}

		switch (_uart)
		{
		case NONE:
			_refSerial.print(_msg);
			break;
		case EOL:
			_refSerial.println(_msg);
			break;
		default:
			break;
		}
	}

}

void SIM7000::_AT(String _cmd){
	/*_Debuger(F("Send CMD -> "), H_CMD, NONE);
	_Debuger(_cmd, NONE, EOL);*/
	_ATSerial.println(_cmd);
}

String SIM7000::_ATReceive(){
	ResString = "";

	while (_ATSerial.available())
	{
		char _c = _ATSerial.read();
		ResString += (String)_c;
	}

	return ResString;
}

uint8_t SIM7000::_ATState(String _str)
{
	_boolBuffer = false;
	if(_str.indexOf(F("OK"))>0)		_boolBuffer=true; 	
	return _boolBuffer;
}

void SIM7000::getGPS() {
	// 停5秒確認ＡＴ正常
	for (_i_c = 0; _i_c < 5; _i_c++) {
		if (AT_Test()) break;
		delay(1000);
	}

	AT_CMD(F("AT+CGNSPWR=1"), false); // 取得GPS數據
	_Debuger(F("GPS ON, Waiting 60s for GPS satellite"), H_NBIOT, EOL);
	delay(30000);
	_AT(F("AT+CGNSINF"));
	delay(100);
	ResString = _ATReceive();
	if (ResString.indexOf(F("+CGNSINF: 1,1,")) > 0)
	{
		_Debuger(ResString, H_NBIOT, EOL);

		_Debuger(_slicer(ResString, ",", 0), H_NBIOT, EOL);

		GPSTimeTag = _slicer(ResString, ",", 2);
		GPSTimeTag = GPSTimeTag.substring(0, GPSTimeTag.length() - 5); // �h�����ڪ�.000

		Latitude = _slicer(ResString, ",", 3);
		Longitude = _slicer(ResString, ",", 4);

		_Debuger(F("GPS Time -> "), H_NBIOT, NONE);
		_Debuger(GPSTimeTag, NONE, EOL);

		_Debuger(F("Latitude -> "), H_NBIOT, NONE);
		_Debuger(Latitude, NONE, EOL);

		_Debuger(F("Longitude -> "), H_NBIOT, NONE);
		_Debuger(Longitude, NONE, EOL);
	}
	else {
		_Debuger(F("GPS ERROR -> "), H_NBIOT, NONE);
		_Debuger(ResString, NONE, EOL);
	}
	AT_CMD(F("AT+CGNSPWR=0"), false); // ����GPS�Ҳ�
	_Debuger(F("GPS OFF"), H_NBIOT, EOL);
}

String SIM7000::_slicer(String _msg, String _mark, uint8_t _index)
{
	_indexFrom = 0;
	_indexTo = _msg.indexOf(_mark, _indexFrom);

	for (_i_c = 0; _i_c < _index; _i_c++)
	{
		_indexFrom = _msg.indexOf(_mark, _indexTo);
		_indexTo = _msg.indexOf(_mark, _indexFrom + 1);
	}
	return _msg.substring(_indexFrom + 1, _indexTo);
}

uint8_t SIM7000::openNetwork(const char* host, int port)
{
  _ATSerial.print(F("AT+CIPSTART=\"TCP\",\"")); // 開起TCP
  _ATSerial.print(host);
  _ATSerial.print(F("\",\""));
  _ATSerial.print(port, DEC);
  _ATSerial.println("\"");

  delay(2000);

  ResString = _ATReceive();
  _Debuger(F("Response -> "), H_NBIOT, NONE);
  _Debuger(ResString, NONE, EOL);
  _boolBuffer = _ATState(ResString);

  return _boolBuffer;
}

uint8_t SIM7000::closeNetwork()
{
  AT_CMD(F("AT+CIPCLOSE"), true);    // 關閉TCP/IP連線
  _boolBuffer = AT_CMD(F("AT+CIPSHUT"), true);     // 關閉網路連線

  return _boolBuffer;
}

void SIM7000::AT_print(String _msg)
{
  Serial.print(_msg);
  _ATSerial.print(_msg);
  _ATSerial.flush();
}

void SIM7000::AT_end()
{
  _ATSerial.write(26);
  _ATSerial.flush();
  delay(1000);

  ResString = _ATReceive();
  _Debuger(F("\rResponse -> "), H_NBIOT, NONE);
  _Debuger(ResString, NONE, EOL);
}
