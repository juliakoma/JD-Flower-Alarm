#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <TimeLib.h>

// BLE Variables
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool currentTimeSet = false; // Flag to check if time is set

// BLE UUIDs
#define SERVICE_UUID "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-12345678abcd"

// Servo and LCD Variables
Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;
int lcdColumns = 16;
int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

// Alarm Variables
int alarmHour = -1;  // No alarm set initially
int alarmMinute = -1;

const byte led_gpio = 32;
const int servo1Pin = 18;
const int servo2Pin = 19;
const int servo3Pin = 13;
const int servo4Pin = 27;
const int relayPin = 26;

// Helper Function to Notify via BLE
void sendBLEMessage(const char* message) {
  if (deviceConnected) {
    pCharacteristic->setValue(message);
    pCharacteristic->notify();
    Serial.println(message); // For debugging
  }
}

// Custom BLE Characteristic Callback
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = String(pCharacteristic->getValue().c_str());

    if (!currentTimeSet && rxValue.length() == 4) { // Setting the initial time
      int hour = rxValue.substring(0, 2).toInt();
      int minute = rxValue.substring(2, 4).toInt();
      setTime(hour, minute, 0, 1, 1, 2024); // Set time using TimeLib
      currentTimeSet = true;

      char buffer[50];
      sprintf(buffer, "Time set to %02d:%02d", hour, minute);
      sendBLEMessage(buffer); // Notify user of set time
      Serial.println(buffer);
    } else if (rxValue == "0"){
      servo1.write(0);
      servo2.write(0);
      servo3.write(0);
      servo4.write(0);
      digitalWrite(led_gpio, LOW);   // turn the LED on (HIGH is the voltage level)
      digitalWrite(relayPin, LOW);
    } else if (rxValue == "1") { // Clear alarms
      alarmHour = -1;
      alarmMinute = -1;
      sendBLEMessage("All alarms cleared.");
      Serial.println("All alarms cleared.");
    } else if (rxValue.length() == 4) { // Set alarm
      alarmHour = rxValue.substring(0, 2).toInt();
      alarmMinute = rxValue.substring(2, 4).toInt();

      char buffer[50];
      sprintf(buffer, "Alarm set for %02d:%02d", alarmHour, alarmMinute);
      sendBLEMessage(buffer); // Notify user of alarm time
      Serial.println(buffer);
    } else {
      sendBLEMessage("Invalid input. Please enter in HHMM format.");
      Serial.println("Invalid input.");
    }
  }
};

// BLE Server Callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Device connected");
    sendBLEMessage("Please enter the current time in HHMM format.");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected");
    pServer->getAdvertising()->start();
  }
};

void setup() {
  // Serial initialization
  Serial.begin(115200);

  // Initialize BLE
  BLEDevice::init("ESP32_BLE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("BLE Device is now advertising...");

  // Initialize Servo
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  servo3.attach(servo3Pin);
  servo4.attach(servo4Pin);

  servo1.write(0);
  servo2.write(0);
  servo3.write(0);
  servo4.write(0);

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  pinMode(led_gpio, OUTPUT);
  pinMode(relayPin, OUTPUT);
}

void loop() {
  // Update LCD with current time if set
  if (currentTimeSet) {
    int currentHour = hour();
    int currentMinute = minute();

    lcd.setCursor(0, 0);
    lcd.clear();
    lcd.print("Time: ");
    if (currentHour < 10) lcd.print("0");
    lcd.print(currentHour);
    lcd.print(":");
    if (currentMinute < 10) lcd.print("0");
    lcd.print(currentMinute);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Waiting for time");
  }

  // Display alarm status
  lcd.setCursor(0, 1);
  if (alarmHour == -1) {
    lcd.print("No alarm set   ");
  } else {
    lcd.print("Alarm at ");
    if (alarmHour < 10) lcd.print("0");
    lcd.print(alarmHour);
    lcd.print(":");
    if (alarmMinute < 10) lcd.print("0");
    lcd.print(alarmMinute);
  }

  // Check if it's time for the alarm
  if (currentTimeSet && hour() == alarmHour && minute() == alarmMinute) {
    char message[50];
    sprintf(message, "Alarm triggered at %02d:%02d", alarmHour, alarmMinute);
    sendBLEMessage(message); // Notify the client
    Serial.println("Alarm triggered!");

    digitalWrite(led_gpio, HIGH);   // turn the LED on (HIGH is the voltage level)
    digitalWrite(relayPin, HIGH);
    servo1.write(180);
    servo2.write(180);
    servo3.write(180);
    servo4.write(180);
    delay(5000); // Allow some time before resetting the alarm

    // Reset the alarm
    alarmHour = -1;
    alarmMinute = -1;
  }

  delay(1000);  // Update every second
}
