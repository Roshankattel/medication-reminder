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

#define LED1 26
#define LED2 25
#define LED3 33
#define BUZZER 13
#define DHT_PIN 32
#define PIR 27
#define BTN 18

#define DHT_TYPE DHT22

const char *ssid = "bhojpure";
const char *password = "Bhojpure@4109123";

#define TOKEN "gXvM5xBt0zrXHUpdfXlY"

#define URL "http://192.168.1.133:8080/api/v1/" TOKEN "/telemetry"

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 20, 4); // I2C address 0x27, 20 column and 4 rows
DHT dht(DHT_PIN, DHT_TYPE);

int t, h = 0;

uint8_t lastHour, lastMins, hour, mins = 0;

uint32_t lastPress = 0;
uint32_t lastReadTime = 0;

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

int sendHTTPReq(String payload);

void setup()
{
#if DEBUG == 1
  Serial.begin(115200);
#endif
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);
  pinMode(PIR, INPUT);

  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
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

  lcd.setCursor(0, 0);                                     // move cursor to the third row
  lcd.print("T:" + String(t) + "C  H:" + String(h) + "%"); // print message at the third row
  lcd.setCursor(15, 0);                                    // move cursor the first row
  lcd.print("00:00");                                      // print message at the second row
  lcd.setCursor(0, 1);                                     // move cursor to the second row
  lcd.print("I2C Address: 0x27");                          // print message at the second row
  lcd.setCursor(0, 2);                                     // move cursor to the fourth row
  lcd.print("Next Medication in:");                        // print message the fourth row
}

void loop()
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

  char timeString[6];
  sprintf(timeString, "%02d:%02d", hour, mins);
  Serial.println(timeString);

  /*Temperatue and Humidity*/
  if (millis() - lastReadTime >= TEMP_READ_TIME)
  {
    if (readDHT())
    {
      Serial.println("Reading temperature data");
      lcd.setCursor(0, 0); // move cursor to the third row
      lcd.print("T:" + String(t) + "C  H:" + String(h) + "%");
      lastReadTime = millis();
    }
  }

  /*Motion Detection */

  if (digitalRead(PIR) == HIGH)
  {
    Serial.println("Motion Detected!");
  }
  /*HELP BUTTON*/
  if (digitalRead(BTN) == LOW)
  {
    if (lastPress == 0)
      lastPress = millis();
    else if (millis() - lastPress >= BTN_READ_TIME)
    {
      Serial.println("Help Requested");
      String jsonString = "{\"power\":false}";
      if (sendHTTPReq(jsonString) == 200)
      {
        lcd.setCursor(0, 2);
        lcd.print("   HELP REQUESTED   ");
      }
      else
      {
        Serial.println("Failed");
      }
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, HIGH);
      digitalWrite(LED3, HIGH);
      lastPress = 0;
    }
  }
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
    Serial.println("HTTP begine error!");
    return -1;
  }
  int httpResponseCode = http.POST(payload);
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  http.end();
  return httpResponseCode;
}
