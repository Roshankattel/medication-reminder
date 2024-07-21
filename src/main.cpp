#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <RTClib.h>

// Intervals
#define BTN_READ_TIME 2000    // 2 sec
#define TEMP_READ_TIME 300000 // 5 mins

#define BUZZER 13
#define DHT_PIN 32
#define PIR 27
#define BTN 18

#define DHT_TYPE DHT22

#define URL "http://192.168.1.133:8080/api/v1/" TOKEN "/telemetry"
#define TOKEN "gXvM5xBt0zrXHUpdfXlY"

const char *ssid = "bhojpure";
const char *password = "Bhojpure@4109123";

uint8_t mediTime[] = {6, 12, 18};

uint8_t ledPins[] = {26, 25, 33};

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 20, 4); // I2C address 0x27, 20 column and 4 rows
DHT dht(DHT_PIN, DHT_TYPE);

int t, h = 0;

uint8_t lastHour, lastMins, hour, mins = 0;

uint32_t lastPress = 0;
uint32_t lastReadTime = 0;

uint8_t medNotifyNum = 0;
DateTime now;

boolean readDHT(void)
{
  h = dht.readHumidity();
  t = dht.readTemperature();
  if (isnan(h) || isnan(t) || h > 100)
    return false;
  else
    return true;
}

char daysOfWeek[7][12] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"};

/*function prototypes*/
int sendHTTPReq(String payload);
void checkTime(void);
void checkSensor(void);
void checkMotion(void);
void checkHelpBtn(void);

void setup()
{
#if DEBUG == 1
  Serial.begin(115200);
#endif

  pinMode(BUZZER, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);
  pinMode(PIR, INPUT);

  /*Initalize the LED pins*/
  for (int i = 0; i < sizeof(ledPins) / sizeof(ledPins[0]); i++)
  {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  digitalWrite(BUZZER, LOW);

  lcd.init(); // initialize the lcd
  lcd.backlight();
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

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000);
  }
  lcd.setCursor(0, 0);
  lcd.print("WiFi");

  lcd.setCursor(15, 0);
  lcd.print("00:00");
  lcd.setCursor(0, 1);
  lcd.print("              ");
  lcd.setCursor(0, 2);
  lcd.print("T:" + String(t) + "C     H:" + String(h) + "%");
  lcd.setCursor(0, 3);
  now = rtc.now();
  hour = now.hour();
  for (int i = 0; i < sizeof(mediTime) / sizeof(mediTime[0]); i++)
  {
    if (hour < mediTime[i])
    {
      char nextMedString[32];
      sprintf(nextMedString, "Next Medicine: %02d:00", mediTime[i]);
      lcd.print(nextMedString);
      break;
    }
  }
}

void loop()
{
  /*Update Time*/
  checkTime();
  /*Temperatue and Humidity*/
  checkSensor();
  /*Motion Detection */
  if (medNotifyNum > 0)
    checkMotion();

  /*HELP BUTTON*/
  checkHelpBtn();

  delay(1000); // delay 1 seconds
}

int sendHTTPReq(String payload)
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

void checkTime(void)
{
  DateTime now = rtc.now();
  hour = now.hour();
  mins = now.minute();
  if (lastHour != hour)
  {
    char hourString[3];
    sprintf(hourString, "%02d", hour);
    lcd.setCursor(15, 0);
    lcd.print(hourString);
    lastHour = hour;
  }
  if (lastMins != mins)
  {
    char minString[3];
    sprintf(minString, "%02d", mins);
    lcd.setCursor(18, 0);
    lcd.print(minString);
    lastMins = mins;
  }

  // char timeString[6];
  // sprintf(timeString, "%02d:%02d", hour, mins);
  // Serial.println(timeString);

  if (mins != 0)
    return;

  for (int i = 0; i < sizeof(mediTime) / sizeof(mediTime[0]); i++)
  {
    if (hour == mediTime[i])
    {
      Serial.println("It's medication time");
      digitalWrite(ledPins[i], HIGH);
      digitalWrite(BUZZER, HIGH);
      medNotifyNum = i;
      break;
    }
  }
}

void checkSensor(void)
{
  if (millis() - lastReadTime < TEMP_READ_TIME)
    return;

  if (!readDHT())
    return;

  Serial.println("Reading temperature data");
  lcd.setCursor(0, 2);
  lcd.print("T:" + String(t) + "C     H:" + String(h) + "%");
  lastReadTime = millis();
}

void checkMotion(void)
{
  if (!digitalRead(PIR))
    return;

  Serial.println("Motion Detected!");
  digitalWrite(BUZZER, LOW);
  digitalWrite(ledPins[medNotifyNum], LOW);
  lcd.setCursor(0, 2);
  char nextMedString[32];
  sprintf(nextMedString, "Next Medicine: %02d:00", (mediTime[medNotifyNum + 1]));
  lcd.print(nextMedString);
  // Send HTTP Requestion
  medNotifyNum = 0;
}

void checkHelpBtn(void)
{
  if (digitalRead(BTN) == HIGH)
    return;

  if (lastPress == 0)
    lastPress = millis();
  else if (millis() - lastPress >= BTN_READ_TIME)
  {
    Serial.println("Help Requested");
    String jsonString = "{\"power\":false}";
    if (sendHTTPReq(jsonString) == 200)
    {
      lcd.setCursor(0, 1);
      lcd.print("   HELP REQUESTED   ");
    }
    else
    {
      Serial.println("Failed");
    }
    // digitalWrite(LED1, HIGH);
    // digitalWrite(LED2, HIGH);
    // digitalWrite(LED3, HIGH);
    lastPress = 0;
  }
}