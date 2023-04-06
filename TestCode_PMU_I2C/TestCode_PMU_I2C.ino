#include <Wire.h>

#define LinkIt7697  2
#define SIMCOM7000E 3
#define INA219      4

#define MPU_I2C     0x70

int _loop_c = 0;
bool i2c_master = true;

void scaner(){
  int nDevices = 0;
  
  for (byte address = 1; address < 127; ++address) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      Serial.println("  !");

      ++nDevices;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("序列埠初始化：9600");

  pinMode(INA219, OUTPUT);
  pinMode(SIMCOM7000E, OUTPUT);
  pinMode(LinkIt7697, OUTPUT);
  Serial.println("初始化控制腳位");

  digitalWrite(LinkIt7697, HIGH);
//  digitalWrite(INA219, LOW);
  digitalWrite(SIMCOM7000E, LOW);

  Wire.begin(0x70);                // join i2c bus with address #8
  Wire.onReceive(receiveEvent); // register event

  Serial.println("Setup Done");         // print the integer
}

bool _buffer;

void loop() {
  Serial.print(i2c_master);
  Serial.print("\t");
  
  if(i2c_master) scaner();
  
  if(_loop_c<50) {
    i2c_master=true;
    Wire.end();
    delay(100);
    digitalWrite(LinkIt7697, LOW);
    digitalWrite(INA219, HIGH);
    Wire.begin();
  }
  else{
    i2c_master=false;
    if(_buffer!=i2c_master){
      Wire.end();
      delay(100);
      digitalWrite(LinkIt7697, HIGH);
      Wire.begin(0x70);                // join i2c bus with address #8
      Wire.onReceive(receiveEvent); // register event
    }
  }
  
  _buffer=i2c_master;
  
  if(_loop_c>500) _loop_c=0;
  delay(100);
  _loop_c++;
}

void receiveEvent(int howMany) {
  while (1 < Wire.available()) { // loop through all but the last
    char c = Wire.read(); // receive byte as a character
    Serial.print(c);         // print the character
  }
  char x = Wire.read();    // receive byte as an integer
  Serial.println(x);      // print the integer
}
