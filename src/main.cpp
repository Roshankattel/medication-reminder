#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <RTClib.h>
#include <WiFiManager.h>

/*Intervals*/
#define BTN_READ_TIME 2000   // 2 sec
#define TEMP_READ_TIME 60000 // 1 mins

/*Pinout*/
#define BUZZER 13
#define DHT_PIN 32
#define PIR 27
#define BTN 18

#define DHT_TYPE DHT22

/*Thingsboard Parameters*/
#define TOKEN "ljwFFrRvXs79bdNe6Ekg"
#define URL "http://192.168.1.90:8080/api/v1/" TOKEN "/telemetry"

#define AP_SSID "Medical-Kit-1"
#define AP_PASS "123456789"

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 20, 4); // I2C address 0x27, 20 column and 4 rows
DHT dht(DHT_PIN, DHT_TYPE);
WiFiManager wm;

DateTime now;

bool helpReq = false;

uint8_t mediTime[] = {13, 14, 15};
uint8_t ledPins[] = {26, 25, 33};
uint8_t nextMedicationTime = 0;
uint8_t lastHour = 0, lastMins = 0, hour = 0, mins = 0;

uint32_t lastPress = 0;
uint32_t lastReadTime = 0;

int temp = 0, humid = 0;
int medNotifyNum = -1;

/*function declarations*/
bool readDHT(void);
void checkTime(void);
void checkMotion(void);
void checkHelpBtn(void);
int sendHTTPReq(String payload);
void checkSensor(bool firstWrite);
int findIndex(uint8_t array[], int size, int element);

void setup()
{
#if DEBUG == 1
  Serial.begin(115200);
#endif

  pinMode(BUZZER, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);
  pinMode(PIR, INPUT);
  /*Initalize the LED pins*/
  for (int led : ledPins)
  {
    pinMode(led, OUTPUT);
    digitalWrite(led, LOW);
  }

  digitalWrite(BUZZER, LOW);

  lcd.init(); // initialize the lcd
  lcd.backlight();

  if (digitalRead(BTN) == LOW)
  {
    lcd.setCursor(0, 0);
    lcd.print("Please connect Wi-Fi");
    lcd.setCursor(0, 1);
    lcd.printf("SSID:%s", AP_SSID);
    lcd.setCursor(0, 2);
    lcd.printf("PASS:%s", AP_PASS);
    wm.setDarkMode(true);
    Serial.println(F("Trigger button is pressed"));
    // wm.resetSettings();
    wm.setConfigPortalTimeout(240);
    if (!wm.startConfigPortal(AP_SSID, AP_PASS))
    {
      ESP.restart();
    }
  }
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Starting....");

  delay(100);
  if (!rtc.begin())
  {
    Serial.println("Could not find RTC! Check circuit.");
    lcd.setCursor(0, 1);
    lcd.print("RTC module error!");
    while (1)
      ;
  }

  // rtc.adjust(DateTime(__DATE__, __TIME__));
  dht.begin();
  if (!readDHT())
  {
    lcd.setCursor(0, 1);
    lcd.print("Temp sensor error!");
  }

  wm.setConnectTimeout(60);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(AP_SSID); // Define hostname
  bool result = wm.autoConnect(AP_SSID, "123456789");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi");

  lcd.setCursor(15, 0);
  lcd.print("00:00");
  checkSensor(true);

  now = rtc.now();
  hour = now.hour();

  lcd.setCursor(0, 3);
  nextMedicationTime = mediTime[0];
  for (int time : mediTime)
  {
    if (hour < time)
    {
      nextMedicationTime = time;
      break;
    }
  }
  lcd.printf("Next Medicine: %02d:00", nextMedicationTime);
}

void loop()
{
  /*Update Time*/
  checkTime();

  /*Temperatue and Humidity*/
  checkSensor(false);

  /*Motion Detection */
  if (medNotifyNum >= 0)
    checkMotion();

  /*HELP BUTTON*/
  checkHelpBtn();

  delay(1000); // delay 1 seconds
}

void checkTime(void)
{
  DateTime now = rtc.now();
  hour = now.hour();
  mins = now.minute();

  if (lastHour != hour)
  {
    lcd.setCursor(15, 0);
    lcd.printf("%02d", hour);
    lastHour = hour;
  }

  if (lastMins != mins)
  {
    lcd.setCursor(18, 0);
    lcd.printf("%02d", mins);
    lastMins = mins;
  }

  if (mins != 0)
    return;

  if (nextMedicationTime == hour)
  {
    int index = findIndex(mediTime, sizeof(mediTime) / sizeof(mediTime[0]), nextMedicationTime);
    lcd.setCursor(0, 1);
    lcd.print("Pls take Medicine:" + String(index + 1));
    digitalWrite(ledPins[index], HIGH);
    digitalWrite(BUZZER, HIGH);
    medNotifyNum = index;
  }
}

void checkSensor(bool firstWrite)
{
  if (millis() - lastReadTime < TEMP_READ_TIME && !firstWrite)
    return;
  if (!readDHT())
    return;

  Serial.println("Reading temperature data");

  lcd.setCursor(0, 2);
  lcd.printf("T:%dC,H:%d%%-> ", temp, humid);

  String conditon;
  if (temp < 15)
    conditon = "Low T";
  else if (temp > 35)
    conditon = "High T";
  else if (humid <= 35)
    conditon = "Low H";
  else if (humid >= 70)
    conditon = "High H";
  else
    conditon = "Good";

  lcd.print(conditon);
  String payload = "{\"temperature\":" + String(temp) + ",\"humidity\":" + String(humid) + "}";
  sendHTTPReq(payload);

  lastReadTime = millis();
}

void checkMotion(void)
{
  if (!digitalRead(PIR))
    return;

  Serial.println("Motion Detected!");
  digitalWrite(BUZZER, LOW);
  digitalWrite(ledPins[medNotifyNum], LOW);

  lcd.setCursor(0, 1);
  lcd.print("Medicine " + String(medNotifyNum + 1) + " Taken   ");

  lcd.setCursor(0, 3);
  uint8_t count = sizeof(mediTime) / sizeof(mediTime[0]);
  nextMedicationTime = (medNotifyNum + 1) >= count ? mediTime[0] : mediTime[medNotifyNum + 1];
  lcd.printf("Next Medicine: %02d:00", nextMedicationTime);

  String payload = "{\"med_taken\":" + String(medNotifyNum + 1) + ",\"next_med_time\":" + String(nextMedicationTime) + "}";
  sendHTTPReq(payload);

  medNotifyNum = -1;
}

void checkHelpBtn(void)
{
  if (digitalRead(BTN) == HIGH)
    return;

  unsigned long currentMillis = millis();
  if (lastPress == 0)
  {
    lastPress = currentMillis;
    return;
  }

  if (currentMillis - lastPress < BTN_READ_TIME)
    return;

  if (!helpReq)
  {
    Serial.println("Help Requested");
    String jsonString = "{\"help_request\":true}";
    lcd.setCursor(0, 1);
    if (sendHTTPReq(jsonString) == 200)
      lcd.print("   HELP REQUESTED   ");
    else
      lcd.print(" HELP REQUEST FAIL! ");

    for (int pin : ledPins)
      digitalWrite(pin, HIGH);
    digitalWrite(BUZZER, HIGH);
  }
  else
  {
    lcd.setCursor(0, 1);
    lcd.print("                    ");
  }

  delay(1000);
  helpReq = !helpReq;
  for (int pin : ledPins)
    digitalWrite(pin, helpReq);
  digitalWrite(BUZZER, helpReq);

  lastPress = 0;
}

int sendHTTPReq(const String payload)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi Disconnected");
    return -1;
  }
  WiFiClient client;
  HTTPClient http;

  if (!http.begin(client, URL))
  {
    Serial.println("HTTP begin error!");
    return -1;
  }

  int httpResponseCode = http.POST(payload);

  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);

  http.end();
  return httpResponseCode;
}

int findIndex(uint8_t array[], int size, int element)
{
  for (int i = 0; i < size; i++)
  {
    if (array[i] == element)
      return i;
  }
  return -1;
}

bool readDHT(void)
{
  humid = dht.readHumidity();
  temp = dht.readTemperature();
  if (isnan(humid) || isnan(temp) || humid > 100)
    return false;
  else
    return true;
}
