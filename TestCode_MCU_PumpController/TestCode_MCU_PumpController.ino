/***** << WaterBox_V2.1 Pro:Controller with OLED and SD>> *****
  PCB borad：WaterBox V2.1
  功能：控制兩顆5V泵
  測試項目：
    1.OLED
    2.SD
    3.Tempture
**************************************************************/
#define mainPower 14
#define pumpSwitch 6

enum PUMP
{
  IN,
  OUT
};

void pump(uint16_t _t, uint8_t pump)
{
  digitalWrite(mainPower, HIGH);
  if (pump)
  {
    digitalWrite(pumpSwitch, HIGH);
    Serial.println("[SYSTEM] 厭氧 -> 魚缸");
  }
  else
  {
    digitalWrite(pumpSwitch, LOW);
    Serial.println("[SYSTEM] 魚缸 -> 厭氧");
  }
  delay(_t * 1000);
  digitalWrite(mainPower, LOW);
}


void setup() {
  Serial.begin(9600);
  Serial.println("[SYSTEM] Init");

  pinMode(mainPower, OUTPUT);
  pinMode(pumpSwitch, OUTPUT);
  
  pump(10, OUT);   // 抽回魚缸

}

void loop()
{
  pump(60, IN);     // 抽回厭氧缸
  delay(600000);
  pump(5, OUT);   // 抽回魚缸
  delay(600000);
  pump(5, OUT);   // 抽回魚缸
  delay(600000);
  pump(5, OUT);   // 抽回魚缸
  delay(600000);
  pump(5, OUT);   // 抽回魚缸
  delay(600000);

}
