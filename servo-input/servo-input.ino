#include <Servo.h>

Servo myservo;
int currentPosition = 90;

void setup() {
  myservo.attach(9, 500, 2400); 
  Serial.begin(9600);

  // Sweep to confirm physical range
  myservo.write(0);
  delay(1000);
  myservo.write(180);
  delay(1000);
  myservo.write(90);  // back to center
}

void loop() {
  if (Serial.available() > 0) {
    int inputPosition = Serial.parseInt();  // correctly parses "90" as 90
    
    if (inputPosition < 1 || inputPosition > 180) return; // guard bad values
    
    Serial.print("Moving to: ");
    Serial.println(inputPosition);

    if (inputPosition > currentPosition) {
      for (int pos = currentPosition; pos <= inputPosition; pos += 1) {
        myservo.write(pos);
        delay(1);
      }
    } else if (inputPosition < currentPosition) {
      for (int pos = currentPosition; pos >= inputPosition; pos -= 1) {
        myservo.write(pos);
        delay(1);
      }
    }

    currentPosition = inputPosition;
  }
}