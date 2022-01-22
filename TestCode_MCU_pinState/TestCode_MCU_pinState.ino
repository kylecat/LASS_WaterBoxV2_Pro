#include <Arduino.h>

#define ModulePower 14
#define ItemSwitch 6

enum _item {
  pH,
  EC
};

void Item(_item _type)
{
  pinMode(ItemSwitch, OUTPUT);
  switch ( _type )
  {
    case pH:
      digitalWrite(ItemSwitch, HIGH);
      Serial.println("Item: pH");
      break;
    case EC:
      digitalWrite(ItemSwitch, LOW);
      Serial.println("Item: EC");
      break;
    default:
      break;
  }
}


void setup(void) {
  pinMode(ModulePower, OUTPUT);
  digitalWrite(ModulePower, HIGH);
  Serial.begin(9600);

}

void loop(void) {
  Item(pH);
  delay(2000);
  Item(EC);
  delay(2000);
}
