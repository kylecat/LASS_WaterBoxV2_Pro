#include <Wire.h>

#define SW_1   5
#define SW_2   6
#define LED 7
#define MainPower   14

#define MPU_I2C     0x70

uint8_t loop_x = 0;


void setup() {
  Serial.begin(9600);
  Serial.println("序列埠初始化：9600");
  
  pinMode(LED, OUTPUT);
  pinMode(MainPower, OUTPUT);
  digitalWrite(MainPower, HIGH);
  
  pinMode(SW_1, INPUT);
  pinMode(SW_2, INPUT);

  Wire.begin();                // join i2c bus with address #8
  Serial.println("Setup Done");         // print the integer
}

void loop() {
  Wire.beginTransmission(MPU_I2C); // transmit to device #8
  Wire.write("[7697]\t x is ");        // sends five bytes
  Wire.write(loop_x);              // sends one byte
  Wire.endTransmission();    // stop transmitting
  
  if(loop_x%2)  digitalWrite(LED, HIGH);
  else          digitalWrite(LED, LOW);
  
  loop_x++;
  delay(500);

}
