#include <Arduino.h>
#ifndef _SIMCOM7000E_H_
#define _SIMCOM7000E_H_

#define		AT_Tx				8
#define 	AT_Rx				9
#define		AT_rate	    9600

class SIM7000
{
  public:
    enum UART
    {
      NONE,
      EOL,
      H_NBIOT,
      H_CMD
    };

    enum APN
    {
      cht_NB,
      cht_4G,
      fet_NB,
      twm_4G,
      twm_NB
    };

    enum Protocol {
      CLOSED,
      TCP,
      UDP,
    };


    SIM7000();
    ~SIM7000();

    void init(uint16_t _pin);
    void begin(uint16_t _rate);
    void setDebuger(Stream& refSer);   // �]�wdebug�Ϊ���X
    void setAPN(APN _apnID);

    void ON();
    void OFF();

    String getCSQ();
    uint8_t  getPosition();

    uint8_t openNetwork(const char* host, int port);
    uint8_t closeNetwork();

    void sendSMS(String _msg);

    uint8_t uploadThingSpeak(String _API, String _DATA);
    uint8_t uploadLASS(String _DATA);
    uint8_t uploadHTTP(String _DATA);

    uint8_t AT_Test();						// ��²���� AT ���աG���ˮ֬O�_OK
    uint8_t AT_CMD(String _cmd, uint8_t _info);	// AT���O���ˮ֬O�_OK

    void AT_print(String _msg);
    void AT_end();

    void getGPS();
    String Latitude;
    String Longitude;
    String GPSTimeTag;

    static String ResString;

    static uint8_t _Rx;
    static uint8_t _Tx;

  private:
    String _APN;

    static uint8_t _Debug;
    uint16_t _PowerPin;
    static Stream& _refSerial;

    static String _StrBuffer;
    uint16_t _cTemp;
    uint8_t _boolBuffer;
    uint8_t _i_c = 0;
    uint16_t _indexFrom = 0;
    uint16_t _indexTo = 0;

    unsigned int _waitingSec = 0;
    
    String _slicer(String _msg, String _mark, uint8_t _index);
    static void _Debuger(String _msg, UART _header, UART _uart = NONE);
    void _AT(String _cmd);

    String _ATReceive();
    uint8_t _ATState(String _str);
};

#endif // !SIMCOM7000E_H
