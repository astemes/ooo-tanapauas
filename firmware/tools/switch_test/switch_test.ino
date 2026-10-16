// Wiring check for the phone slots. Flash to the GIGA R1 (M7 core),
// open Serial Monitor at 115200 and press each microswitch in turn.

const uint8_t kSlotPins[] = {2, 3, 4, 5};
const int kSlots = sizeof(kSlotPins) / sizeof(kSlotPins[0]);
const uint8_t kBuzzerPin = 7;

uint8_t last = 0xFF;

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < kSlots; i++) pinMode(kSlotPins[i], INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  delay(1000);
  Serial.println("Switch test: press each slot's switch (LOW = docked)");
  tone(kBuzzerPin, 880, 150);  // you should hear a beep if the buzzer is wired
}

void loop() {
  uint8_t mask = 0;
  for (int i = 0; i < kSlots; i++) {
    if (digitalRead(kSlotPins[i]) == LOW) mask |= 1 << i;
  }
  if (mask != last) {
    for (int i = 0; i < kSlots; i++) {
      Serial.print("slot");
      Serial.print(i + 1);
      Serial.print(mask & (1 << i) ? "=DOCKED " : "=empty  ");
    }
    Serial.println();
    if (last != 0xFF && mask > last) tone(kBuzzerPin, 1320, 60);
    last = mask;
  }
  // GIGA's built-in LED is active-low: lit when every slot is docked.
  digitalWrite(LED_BUILTIN, mask == (1 << kSlots) - 1 ? LOW : HIGH);
  delay(20);
}
