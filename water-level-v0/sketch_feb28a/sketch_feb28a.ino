const int WATER_SENSOR_PIN = A0;
const int WATER_SENSOR_PIN2 = A5;
const int LED_PIN = 10;
const int WATER_THRESHOLD = 200;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  int waterLevel = analogRead(WATER_SENSOR_PIN);
  int waterLevel2 = analogRead(WATER_SENSOR_PIN2);
  
  Serial.print("Water Level: ");
  Serial.println(waterLevel);
  
  if (waterLevel < WATER_THRESHOLD || waterLevel2 < WATER_THRESHOLD) {
    digitalWrite(LED_PIN, HIGH);  // Turn LED on
  } else {
    digitalWrite(LED_PIN, LOW);   // Turn LED off
  }
  
  delay(2000);
}