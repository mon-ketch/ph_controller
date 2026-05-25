#include <Preferences.h>

#define PH_PIN      34
#define BUTTON_PIN  27
#define BUZZER_PIN  25

#define BUFFER_SIZE 100

Preferences prefs;

// buffer
int samples[BUFFER_SIZE];
int sampleIndex = 0;
unsigned long lastSampleTime = 0;

// realtime
float voltage = 0;
float phValue = 0;

// calibration
float voltage7 = 2.50;
float voltage4 = 3.00;
float slope = 0;
float offset = 0;

// button
bool buttonPressed = false;
unsigned long buttonPressTime = 0;

void setup()
{
  Serial.begin(115200);

  pinMode(PH_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  // =========================
  // LOAD CALIBRATION FROM ROM
  // =========================
  prefs.begin("phcal", false);

  voltage7 = prefs.getFloat("v7", 2.50);
  voltage4 = prefs.getFloat("v4", 3.00);

  Serial.println("Loaded calibration:");
  Serial.print("V7: "); Serial.println(voltage7);
  Serial.print("V4: "); Serial.println(voltage4);

  // init buffer
  for (int i = 0; i < BUFFER_SIZE; i++)
    samples[i] = analogRead(PH_PIN);

  calculateCalibration();
  beep(1);
}

void loop()
{
  readPH();
  handleButton();
  calculatePH();

  static unsigned long lastPrint = 0;

  if (millis() - lastPrint > 1000)
  {
    lastPrint = millis();

    Serial.print("Voltage: ");
    Serial.print(voltage, 3);

    Serial.print(" | pH: ");
    Serial.println(phValue, 2);
  }
}

// =========================
// READ & AVERAGE
// =========================
void readPH()
{
  if (millis() - lastSampleTime >= 50)
  {
    lastSampleTime = millis();

    samples[sampleIndex] = analogRead(PH_PIN);
    sampleIndex = (sampleIndex + 1) % BUFFER_SIZE;

    long total = 0;

    for (int i = 0; i < BUFFER_SIZE; i++)
      total += samples[i];

    float adcAvg = total / (float)BUFFER_SIZE;

    voltage = adcAvg * (3.3 / 4095.0);
  }
}

// =========================
// BUTTON HANDLING
// =========================
void handleButton()
{
  bool state = (digitalRead(BUTTON_PIN) == LOW);

  if (state && !buttonPressed)
  {
    buttonPressed = true;
    buttonPressTime = millis();
  }

  if (!state && buttonPressed)
  {
    buttonPressed = false;

    unsigned long hold = millis() - buttonPressTime;

    // =========================
    // CAL pH4
    // =========================
    if (hold >= 8000)
    {
      voltage4 = voltage;

      prefs.putFloat("v4", voltage4);

      calculateCalibration();

      Serial.println("CAL pH4 SAVED");
      beep(2);
    }

    // =========================
    // CAL pH7
    // =========================
    else if (hold >= 3000)
    {
      voltage7 = voltage;

      prefs.putFloat("v7", voltage7);

      calculateCalibration();

      Serial.println("CAL pH7 SAVED");
      beep(1);
    }
  }
}

// =========================
// CALIBRATION
// =========================
void calculateCalibration()
{
  slope = (4.01 - 7.01) / (voltage4 - voltage7);
  offset = 7.01 - slope * voltage7;

  Serial.println("Calibration updated");
  Serial.print("Slope: "); Serial.println(slope);
  Serial.print("Offset: "); Serial.println(offset);
}

// =========================
// PH CALC
// =========================
void calculatePH()
{
  phValue = slope * voltage + offset;
}

// =========================
// BUZZER
// =========================
void beep(int times)
{
  for (int i = 0; i < times; i++)
  {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(120);
    digitalWrite(BUZZER_PIN, LOW);
    delay(120);
  }
}