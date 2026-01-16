#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>

// OLED display config
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  32
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C

// Pins
#define BUTTON_1_PIN   2   // drop button
#define GATE_PIN       3   // photogate
#define BUTTON_2_PIN   9   // ARM button (active LOW)
#define MAG_PIN        5   // electromagnet
#define GREEN_LED_PIN  8
#define RED_LED_PIN    6

// Volatile Interrupt Variables
volatile unsigned long tStart = 0;
volatile unsigned long tStop  = 0;
volatile bool waitingForGate  = false;
volatile bool done            = false;

// State Tracking
bool armed = false;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Debounce state for BUTTON 2 (non-blocking)
const unsigned long DEBOUNCE_MS = 20;
int button2LastReading          = HIGH;
int button2StableState          = HIGH;
unsigned long button2LastChange = 0;

unsigned long displayStartTime = 0; 
int displayTime = 8000; //display time in ms
bool showingResult = false;

// Button Interrupt
void onButton() {
  if (!waitingForGate && armed) {
    digitalWrite(MAG_PIN, LOW);  // release ball
    tStart = micros();
    waitingForGate = true;
    armed = false;
  }
}
// Gate Interrupt
void onGate() {
  if (waitingForGate) {
    tStop = micros();
    waitingForGate = false;
    done = true;
  }
}

// ---------- HELPER FUNCTIONS ----------
void showMessage(const char* msg) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(1, 8);
  display.print(msg);
  display.display();
}

void armSystem() {
  armed = true;
  waitingForGate = false;
  done = false;

  digitalWrite(MAG_PIN, HIGH);       // hold ball
  digitalWrite(GREEN_LED_PIN, HIGH); // armed indicator
  digitalWrite(RED_LED_PIN, LOW);
  //digitalWrite(BUTTON_2_PIN,HIGH);
  showMessage("READY4DROP");
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  // Pin Setup
  pinMode(BUTTON_1_PIN, INPUT);
  pinMode(GATE_PIN, INPUT);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);    // ARM button (active LOW)

  pinMode(MAG_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  // Initial States
  digitalWrite(MAG_PIN, LOW);       //mag off
  digitalWrite(GREEN_LED_PIN, LOW);  // not armed yet
  digitalWrite(RED_LED_PIN, HIGH);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    while (true) { } // trap if display fails
  }
  display.clearDisplay();
  display.display();
  showMessage("Ball Drop");

  // Attach interrupts
  attachInterrupt(digitalPinToInterrupt(BUTTON_1_PIN), onButton, RISING);
  attachInterrupt(digitalPinToInterrupt(GATE_PIN), onGate, RISING);
  Serial.println("Completed Setup");
  digitalWrite(RED_LED_PIN,LOW);
  delay(100);
  digitalWrite(RED_LED_PIN,HIGH);
}


void loop() {
  int reading = digitalRead(BUTTON_2_PIN);
  //Serial.println(digitalRead(BUTTON_2_PIN));
  //button 2 pin not changing state
  if (reading != button2LastReading) {
    button2LastChange = millis();
    button2LastReading = reading;
    Serial.println(reading);
  }
  
  if ((millis() - button2LastChange) > DEBOUNCE_MS) {
    // input has been stable long enough, treat it as the debounced state
    if (reading != button2StableState) {
      button2StableState = reading;
      // Detect a *press* (goes LOW with INPUT_PULLUP)
      if (button2StableState == LOW) {
        if (!armed && !waitingForGate && !done) {
          armSystem();
          Serial.println(button2StableState);
        }
      }
    }
  }

  if (done) {
    unsigned long dt;  
    dt = tStop - tStart;
    long conversion = 1000L; 
    dt = dt/conversion;
    done = false;
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 4);
    display.print(dt);
    display.print(F(" ms"));
    display.display();

    digitalWrite(MAG_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
    displayStartTime = millis();
    showingResult = true;
  }
  if(waitingForGate) {
    digitalWrite(GREEN_LED_PIN,LOW);
    //showMessage("DROPPING");
  }
  // Keep indicator LEDs/magnet in correct state when armed
  if (armed && !waitingForGate) {
    digitalWrite(MAG_PIN, HIGH);       // hold ball
    digitalWrite(GREEN_LED_PIN, HIGH); // armed
    digitalWrite(RED_LED_PIN, LOW);
    Serial.println("Still Armed");
  }
  if (showingResult){
    long displayDuration = 0; 
    displayDuration = millis()-displayStartTime;
    if (displayDuration >= displayTime) {
      showMessage("Ball Drop"); 
      showingResult = false; 
    }
  }
}
