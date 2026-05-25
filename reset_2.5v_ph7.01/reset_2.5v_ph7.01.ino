#define PH_PIN 34

void setup()
{
  Serial.begin(115200);
  pinMode(PH_PIN, INPUT);
}

void loop()
{
  int adc = analogRead(PH_PIN);

  float voltage = adc * (3.3 / 4095.0);

  Serial.print("ADC: ");
  Serial.print(adc);

  Serial.print(" | Voltage: ");
  Serial.println(voltage, 3);

  delay(500);
}