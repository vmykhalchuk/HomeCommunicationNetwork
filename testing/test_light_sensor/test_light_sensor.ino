void setup() {
  digitalWrite(A5, HIGH);
  Serial.begin(9600);
  while (!Serial);
}

void loop() {
  int r = analogRead(A5);
  Serial.println(r);
  delay(1000);
}
