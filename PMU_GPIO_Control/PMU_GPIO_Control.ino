#include "LowPower.h"

#define LinkIt7697  2
#define SIMCOM7000E 3
#define INA219      4

void setup() {
  Serial.begin(9600);
  Serial.println("序列埠初始化：9600");
  
  pinMode(INA219, OUTPUT);
  pinMode(SIMCOM7000E, OUTPUT);
  pinMode(LinkIt7697, OUTPUT);
  Serial.println("初始化控制腳位");

  digitalWrite(INA219, LOW);
  digitalWrite(SIMCOM7000E, LOW);
  digitalWrite(LinkIt7697, LOW);
}

void loop() {
  Serial.println("啟動LinkIt7697：5 秒");
  digitalWrite(LinkIt7697, HIGH);
  delay(5000);
  digitalWrite(LinkIt7697, LOW);
  delay(1000);
  
  Serial.println("啟動SIMCOM7000E：5 秒");
  digitalWrite(SIMCOM7000E, HIGH);
  delay(5000);
  digitalWrite(SIMCOM7000E, LOW);
  delay(1000);
  
  Serial.println("啟動INA219：5 秒");
  digitalWrite(INA219, HIGH);
  delay(5000);
  digitalWrite(INA219, LOW);
  delay(1000);

//  Serial.println("進入省電睡眠：8 秒");
//  LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_OFF);
  
}
