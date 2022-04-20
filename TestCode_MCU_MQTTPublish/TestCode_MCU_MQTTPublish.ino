/* LinkIt 7697 專用
   1. 連接Wifi (2.4G)
   2. 取得MAC編號
   3. 連上LASS 公開伺服器
   4. 上傳測試用包裹: 自動打包
*/

#include <LWiFi.h>
#define WifiSSID "KyleiPhoneXR" //  your network SSID (name)
#define WifiPW "11115555"       // your network password (use for WPA, or use as key for WEP)
WiFiClient client;
int status = WL_IDLE_STATUS;

// the MAC address of your Wifi shield
byte mac[6];

// LinkIt Logo Http Config
#define Server_LinkItLogo "download.labs.mediatek.com"
#define Port_LinkItLogo   80

// LASS MQTT Config
#define Server_LASS "gpssensor.ddns.net"
#define Port_LASS   1883
#define MQTT_LASS_Topic "LASS/Test/WaterBox_TW/"

// MQTT Test Config
#define Server_TEST "test.mosquitto.org"
#define Port_TEST   1883

#define BufferSize 256
String DeviceID = "WB_"; //格式 WB_aabbccddeeff
uint32_t MQTT_PL = 0;
byte MQTT_RL[4];
byte MQTT_BUFFER[BufferSize];

// MQTT封包
byte MQTT_CONCENT_package[] = {0x10,  // Fixed Header
                               0x20,  // RL: 32
                               0x00, 0x04,                                            // Protocol Length: 4
                               (byte)'M', (byte)'Q',  (byte)'T',  (byte)'T',          // Protocol Name:MQTT
                               0x04,                                                  // Protocol level (mqtt3.1.1 是0x04)
                               0x02,                                                  // Connect flags: 0b00000010 (Clean Session)
                               0x00, 0x3C,                                            // Keep Alive: 60 sec
                               0x00, 0x14,                                            // ClientID Lenght: 20 (WBp_aabbccddeeff_RRR, R是亂數加上)
                               (byte)'W', (byte)'B',  (byte)'p',  (byte)'_',
                               (byte)'A', (byte)'1',  (byte)'B',  (byte)'2', (byte)'C', (byte)'3',
                               (byte)'D', (byte)'4',  (byte)'E',  (byte)'5', (byte)'F', (byte)'6',
                               (byte)'_', (byte)random(65, 90), (byte)random(65, 90), (byte)random(65, 90)
                              }; // MQTT 連線用封包

byte MQTT_Test_package[] = {0x30,  // Fixed Header
                            0x30,  // RL: 41 (Remaining Length) 34+2+10+2
                            0x00, 0x22,                                             // Topic Length: 34
                            (byte)'L', (byte)'A', (byte)'S', (byte)'S', (byte)'/',  // Topic: LASS/TEST/WaterBox_TW/TEST_MESSAGE
                            (byte)'T', (byte)'e', (byte)'s', (byte)'T', (byte)'/',
                            (byte)'W', (byte)'a', (byte)'t', (byte)'e', (byte)'r',
                            (byte)'B', (byte)'o', (byte)'x', (byte)'_', (byte)'T', (byte)'W', (byte)'/',
                            (byte)'T', (byte)'E', (byte)'S', (byte)'T', (byte)'_',
                            (byte)'M', (byte)'E', (byte)'S', (byte)'S', (byte)'A', (byte)'G', (byte)'E',
                            0x00, 0x0A,                                               // Payload Length: 10
                            (byte)'W', (byte)'A',  (byte)'T',  (byte)'E', (byte)'R',  // Payload: WATERBOX!!
                            (byte)'B', (byte)'O',  (byte)'X',  (byte)'!', (byte)'!'
                           }; // 訊息用封包：測試用


// 打包用struct
struct PAYLOAD {
  uint8_t Length[2];
  char Content[BufferSize];
};

PAYLOAD mqtt_topic;
PAYLOAD mqtt_msg;
PAYLOAD client_id;

/***********<< 封包格式 >>**********
  CONNECT
  PUBLISH
  DISCONNECT
*********************************/
struct PUBLISH {
  uint8_t Fix_Header = 0x30 | 0b0010; // 0x30(publish) | 0b0010 (QoS:1)| 0b0001 (Retain)
  byte RL[4];
  uint8_t RL_size;
  PAYLOAD* Topic;
  PAYLOAD* Msg;
} PUBLISH_KPG;

struct CONNECT {
  uint8_t Fix_Header = 0x10;
  byte RL[4];
  uint8_t RL_size;
  byte Flags = 0x02;                                     // Connect flags: 0b00000010 (Clean Session)
  byte Protocol[7] = {
    0x00, 0x04,                                          // Protocol Length: 4
    (byte)'M', (byte)'Q',  (byte)'T',  (byte)'T',        // Protocol Name:MQTT
    0x04                                                 // Protocol level (mqtt3.1.1 是0x04)
  };
  byte KeepAlive[2] = {0x00, 0x3C};                       // Keep Alive: 60 sec
  PAYLOAD* ID;
  PAYLOAD* User;
  PAYLOAD* Password;
} CONNECT_KPG;


/***********<< 封包格式 >>**********
  CalculatePL 把String填入PAYLOAD 結構內並計算PL(2 bytes)
  CalculateRL 計算RL, 填入MQTT_RL Buffer內，回傳用掉幾個bytes
*********************************/
void CalculatePL(PAYLOAD* _payload, String _msg) {
  uint16_t _length = _msg.length();
  _msg.toCharArray(_payload->Content, _length + 1); // array 最後一個是\0, 要留這個byte的空間
  _payload->Length[0] = (byte)_length / 256;
  _payload->Length[1] = (byte)_length % 256;

  Serial.print("[MQTT-PL] "); Serial.print(_msg); Serial.print(" "); Serial.println(_msg.length());
  Serial.print("\t 0x"); Serial.println(_payload->Length[0], HEX);
  Serial.print("\t 0x"); Serial.println(_payload->Length[1], HEX);
  Serial.print("\t  "); Serial.println(_payload->Content);
}

uint8_t CalculateRL(uint32_t _size) {
  uint8_t _index = 0;
  do
  {
    uint8_t _d = _size % 128;    // 取餘數 _size & 0b01111111
    _size /= 128;                // 取除以128後的整數：_size = _size >> 7;
    if (_size > 0)  _d |= 0x80;  // 除128的整數 > 0 時，對_d + 128 (bit[7]為1)
    MQTT_RL[_index] = _d;        // 計算好的 _d 放到 buffer內
    if (_index < 3)    _index++; // 避免寫入buffer爆掉，限制_index不超過3
  } while (_size > 0);

  Serial.print("RL Nubmer: "); Serial.print(_size);
  Serial.print(" index: "); Serial.print(_index);
  Serial.print(", buffer-> 0x");
  Serial.print(MQTT_RL[0], HEX); Serial.print(", 0x");
  Serial.print(MQTT_RL[1], HEX); Serial.print(", 0x");
  Serial.print(MQTT_RL[2], HEX); Serial.print(", 0x");
  Serial.print(MQTT_RL[3], HEX); Serial.println();
  return _index;
}

uint16_t PackPublishBuffer(PUBLISH* _pack) {
  uint16_t _pl = 0;
  uint16_t _size = 0;

  // FixHeader
  MQTT_BUFFER[0] = (byte) _pack->Fix_Header;
  _size += 1; // 轉移下一個index

  // RL
  for (uint8_t _i = 0; _i < _pack->RL_size; _i++) {
    MQTT_BUFFER[_size + _i] = (byte) _pack->RL[_i];
  }
  _size += _pack->RL_size;

  // Topic
  _pl = _pack->Topic->Length[0] * 256 + _pack->Topic->Length[1];

  MQTT_BUFFER[_size] = (byte) _pack->Topic->Length[0];
  _size += 1;

  MQTT_BUFFER[_size] = (byte) _pack->Topic->Length[1];
  _size += 1;

  for (uint8_t _i = 0; _i < _pl; _i++) {
    MQTT_BUFFER[_size + _i] = (byte) _pack->Topic->Content[_i];
  }
  _size += _pl; // 從0 -> _pl 共 _pl+1個

  // Msg
  _pl = _pack->Msg->Length[0] * 256 + _pack->Msg->Length[1];

  MQTT_BUFFER[_size] = (byte) _pack->Msg->Length[0];
  _size += 1;

  MQTT_BUFFER[_size] = (byte) _pack->Msg->Length[1];
  _size += 1;

  for (uint8_t _i = 0; _i <= _pl; _i++) {
    MQTT_BUFFER[_size + _i] = (byte) _pack->Msg->Content[_i];
  }
  _size += _pl; // 從0 -> _pl 共 _pl+1個

  String _buffer_str;
  Serial.print("[Publish] Package Size:"); Serial.println(_size);
  for (uint8_t _i = 0; _i < _size; _i++) {
    _buffer_str = ZeroPadding(MQTT_BUFFER[_i]);
    Serial.print("0x" + _buffer_str + " ");
    if (_i % 10 == 9) Serial.print("\r\n");
  }
  Serial.println("\r\n");

  return _size;
}

uint16_t PackConnectBuffer(CONNECT* _pack) {
  uint16_t _pl = 0;
  uint16_t _size = 0;
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

  // FixHeader
  MQTT_BUFFER[0] = (byte) _pack->Fix_Header;
  _size += 1; // 轉移下一個index

  // RL
  for (uint8_t _i = 0; _i < _pack->RL_size; _i++) {
    MQTT_BUFFER[_size + _i] = (byte) _pack->RL[_i];
  }
  _size += _pack->RL_size;

  // Protocol
  for (uint8_t _i = 0; _i < 7; _i++) {
    MQTT_BUFFER[_size + _i] = (byte) _pack->Protocol[_i];
  }
  _size += 7;

  // Connect flags
  MQTT_BUFFER[_size] = (byte) _pack->Flags;
  _size += 1; // 轉移下一個index

  // KeepAlive
  for (uint8_t _i = 0; _i < 2; _i++) {
    MQTT_BUFFER[_size + _i] = (byte) _pack->KeepAlive[_i];
  }
  _size += 2;

  // Client ID
  _pl = _pack->ID->Length[0] * 256 + _pack->ID->Length[1];

  MQTT_BUFFER[_size] = (byte) _pack->ID->Length[0];
  _size += 1;

  MQTT_BUFFER[_size] = (byte) _pack->ID->Length[1];
  _size += 1;

  for (uint8_t _i = 0; _i <= _pl; _i++) {
    MQTT_BUFFER[_size + _i] = (byte) _pack->ID->Content[_i];
  }
  _size += _pl; // 從0 -> _pl 共 _pl+

  /*
    // User
    _pl = _pack->User->Length[0] * 256 + _pack->User->Length[1];

    MQTT_BUFFER[_size] = (byte) _pack->User->Length[0];
    _size += 1;

    MQTT_BUFFER[_size] = (byte) _pack->User->Length[1];
    _size += 1;

    for (uint8_t _i = 0; _i <= _pl; _i++) {
      MQTT_BUFFER[_size + _i] = (byte) _pack->User->Content[_i];
    }
    _size += _pl; // 從0 -> _pl 共 _pl+

    // Password
    _pl = _pack->Password->Length[0] * 256 + _pack->Password->Length[1];

    MQTT_BUFFER[_size] = (byte) _pack->Password->Length[0];
    _size += 1;

    MQTT_BUFFER[_size] = (byte) _pack->Password->Length[1];
    _size += 1;

    for (uint8_t _i = 0; _i <= _pl; _i++) {
      MQTT_BUFFER[_size + _i] = (byte) _pack->Password->Content[_i];
    }
    _size += _pl; // 從0 -> _pl 共 _pl+
  */

  String _buffer_str;
  Serial.print("[Concent] Package Size:"); Serial.println(_size);
  for (uint8_t _i = 0; _i < _size; _i++) {
    _buffer_str = ZeroPadding(MQTT_BUFFER[_i]);
    Serial.print("0x" + _buffer_str + " ");
    if (_i % 10 == 9) Serial.print("\r\n");
  }
  Serial.println("\r\n");

  return _size;
}

/***********<< 網路用 >>**********
  printWifiStatus 顯示網路狀態
  MacAddress      回傳MAC:自動補零
  ZeroPadding
*********************************/
void printWifiStatus(void) {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

String MacAddress(void) {
  String _result;
  WiFi.macAddress(mac);
  _result.concat(ZeroPadding(mac[5]));
  _result.concat(ZeroPadding(mac[4]));
  _result.concat(ZeroPadding(mac[3]));
  _result.concat(ZeroPadding(mac[2]));
  _result.concat(ZeroPadding(mac[1]));
  _result.concat(ZeroPadding(mac[0]));
  return _result;
}

String ZeroPadding(byte _value) {
  String _result;
  if (_value < 16)  _result = "0" + String(_value, HEX);
  else              _result = String(_value, HEX);
  return _result;
}


void setup() {
  Serial.begin(9600);

  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  //  CalculateRL(127);
  //  CalculateRL(128);
  //  CalculateRL(16383);
  //  CalculateRL(16384);
  //  CalculateRL(2097151);
  //  CalculateRL(2097152);
  //  CalculateRL(268435455);

  // 測試DEVICE iD
  DeviceID += MacAddress();
  //  Serial.print("[MQTT] Device ID: \t");
  //  Serial.print(DeviceID);
  //  Serial.print("\t");
  //  Serial.println(DeviceID.length(), HEX);

  // 以下待確認
  String id_str = DeviceID + "_" + (char)random(65, 90) + (char)random(65, 90) + (char)random(97, 122) + (char)random(97, 122);
  String topic_str = MQTT_LASS_Topic + DeviceID;

  String msg_str = (String)"{\"Device\":\"WaterBoxPro\",\"Ver\":2.2,\"FT\":\"UpStream\",\"DateTime\":\"2022-03-02\"}";

  /*
    |device=Linkit7697
    |device_id=9C65F920C020
    |ver_app=1.1.0
    |date = 2019-03-21
    |time = 06:53:55
    |tick=0
    |device=Linkit7697
    |FAKE_GPS=1
    |gps_lon=121.787
    |gps_lat=25.1933
    |s_ec=
    |s_ph=
    |s_t0=
    |s_nh4=
    |s_no3=
    |s_Tb＝
  */

  //  Serial.print("[MQTT] Add random symbols: ");
  //  Serial.print(id_str);
  //  Serial.print("\t");
  //  Serial.println(id_str.length(), HEX);

  // 先計算PL並打包對應的payload 內容
  CalculatePL(&client_id, id_str);
  CalculatePL(&mqtt_topic, topic_str);
  CalculatePL(&mqtt_msg, msg_str);

  Serial.print("[CONTENT] ID: "); Serial.println(client_id.Content);
  CONNECT_KPG.ID =  &client_id;
  MQTT_PL = 10 + 2 + id_str.length(); //計算RL長度 = 10+2+PL(ID)
  CONNECT_KPG.RL_size =  CalculateRL(MQTT_PL);  // 計算RL 用掉幾個bytes，並更新MQTT_RL 這個Buffer
  memcpy(CONNECT_KPG.RL, MQTT_RL, 4);  // 把MQTT_RL 這個Buffer 複製到PUBLIC_KPG裡面
  Serial.println();

  Serial.print("[CONTENT] Publish Topic: "); Serial.println(mqtt_topic.Content);
  // 打飽 publish 封包內容, RL 放4bytes, 要傳之前再看實際的大小塞進Buffer內？？
  PUBLISH_KPG.Msg =  &mqtt_msg;
  PUBLISH_KPG.Topic =  &mqtt_topic;
  MQTT_PL = 2 + topic_str.length() + 2 + msg_str.length(); //計算RL長度 = 2+PL(Topic)+2+PL(Payload)
  PUBLISH_KPG.RL_size =  CalculateRL(MQTT_PL);  // 計算RL 用掉幾個bytes，並更新MQTT_RL 這個Buffer
  memcpy(PUBLISH_KPG.RL, MQTT_RL, 4);  // 把MQTT_RL 這個Buffer 複製到PUBLIC_KPG裡面
  Serial.println();

  Serial.println("-------------------------------");
  Serial.println("[MQTT] Broker Connect content:");
  Serial.print("\t Fixed Header:\t"); Serial.println(CONNECT_KPG.Fix_Header, HEX);
  Serial.print("\t RL-size:"); Serial.print(CONNECT_KPG.RL_size);
  Serial.print("\t RL:");
  for (uint8_t _i = 0; _i < CONNECT_KPG.RL_size; _i++) {
    Serial.print(" "); Serial.print(CONNECT_KPG.RL[_i], HEX);
  }
  Serial.println();
  Serial.print("\t ID:\t");   Serial.print(CONNECT_KPG.ID->Length[0], HEX);
  Serial.print(" "); Serial.println(CONNECT_KPG.ID->Length[1], HEX);
  Serial.print("\t ID:\t"); Serial.println(CONNECT_KPG.ID->Content);
  Serial.println("-------------------------------");


  Serial.println("[MQTT] Publish package content:");
  Serial.print("\t Fixed Header:\t"); Serial.println(PUBLISH_KPG.Fix_Header, HEX);
  Serial.print("\t RL-size:"); Serial.print(PUBLISH_KPG.RL_size);
  Serial.print("\t RL:");
  for (uint8_t _i = 0; _i < PUBLISH_KPG.RL_size; _i++) {
    Serial.print(" "); Serial.print(PUBLISH_KPG.RL[_i], HEX);
  }
  Serial.println();
  Serial.print("\t TopicPL:\t");   Serial.print(PUBLISH_KPG.Topic->Length[0], HEX);
  Serial.print(" "); Serial.println(PUBLISH_KPG.Topic->Length[1], HEX);
  Serial.print("\t Topic:\t"); Serial.println(PUBLISH_KPG.Topic->Content);


  Serial.print("\t MsgPL:\t");   Serial.print(PUBLISH_KPG.Msg->Length[0], HEX);
  Serial.print(" "); Serial.println(PUBLISH_KPG.Msg->Length[1], HEX);
  Serial.print("\t Msg:\t"); Serial.println(PUBLISH_KPG.Msg->Content);
  Serial.println("-------------------------------\r\n\r\n");

  uint16_t _packageSize;

  //   attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(WifiSSID);
    status = WiFi.begin(WifiSSID, WifiPW);
  }
  Serial.println("Connected to wifi");
  printWifiStatus();

  //  Serial.println("\nStarting connection to server...");
  //  if (client.connect(Server_LinkItLogo, 80)) {
  //    Serial.println("connected to server (GET)");
  //    client.println("GET /linkit_7697_ascii.txt HTTP/1.0");
  //    client.println("Host: download.labs.mediatek.com");
  //    client.println("Accept: */*");
  //    client.println("Connection: close");
  //    client.println();
  //    delay(10);
  //    Serial.println("Waiting Response......");
  //    Serial.println("-- End of Setup --\r\n");
  //  }

  if (client.connect(Server_LASS, Port_LASS)) {

    Serial.println("[MQTT] Connecting Broker........");
    // Send MQTT CONCENT PACKAGE
//    _packageSize = sizeof(MQTT_CONCENT_package) / sizeof(MQTT_CONCENT_package[0]);
//    CalculateRL(_packageSize);
//    for (uint16_t _i = 0; _i < _packageSize; _i++) {
//      client.write(MQTT_CONCENT_package[_i]);
//    }
//    delay(500);

    //    // send MQTT CONCENT(BUFFER)
    _packageSize = PackConnectBuffer(&CONNECT_KPG);
    for (uint16_t _i = 0; _i < _packageSize; _i++) {
      client.write(MQTT_BUFFER[_i]);
    }
    delay(500);


    Serial.println("[MQTT] Publish Message");
    //     Send MQTT PUBLISH PACKAGE
    _packageSize = sizeof(MQTT_Test_package) / sizeof(MQTT_Test_package[0]);
    CalculateRL(_packageSize);
    for (uint16_t _i = 0; _i < _packageSize; _i++) {
      client.write(MQTT_Test_package[_i]);
    }
    delay(500);

    //    // send MQTT PACKAGE(BUFFER)
    //    _packageSize = PackPublishBuffer(&PUBLISH_KPG);
    //    for (uint16_t _i = 0; _i < _packageSize; _i++) {
    //      client.write(MQTT_BUFFER[_i]);
    //    }
    //    delay(500);
  }
}

void loop() {
  // get a HTTP Response
  while (client.available()) {
    char c = client.read();
    Serial.print(c, HEX);
    Serial.print(" ");
  }

  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    Serial.println();
    Serial.println("disconnecting from server.");
    client.stop();

    // do nothing forevermore:
    while (true);
  }

}
