#include <Wire.h>

#define LinkIt7697  2
#define SIMCOM7000E 3
#define INA219      4

#define MPU_I2C     0x70


void setup() {
  Serial.begin(9600);
  Serial.println("序列埠初始化：9600");

  pinMode(INA219, OUTPUT);
  pinMode(SIMCOM7000E, OUTPUT);
  pinMode(LinkIt7697, OUTPUT);
  Serial.println("初始化控制腳位");

  digitalWrite(LinkIt7697, HIGH);
  digitalWrite(INA219, LOW);
  digitalWrite(SIMCOM7000E, LOW);

  Wire.begin(0x70);                // join i2c bus with address #8
  Wire.onReceive(receiveEvent); // register event

  Serial.println("Setup Done");         // print the integer
}

void loop() {
  delay(100);
}

void receiveEvent(int howMany) {
  while (1 < Wire.available()) { // loop through all but the last
    char c = Wire.read(); // receive byte as a character
    Serial.print(c);         // print the character
  }
  char x = Wire.read();    // receive byte as an integer
  Serial.println(x);      // print the integer
}
