// please install Adafruit_BMP280 lib first
// Adafruit_BMP280 lib in Sketch->Includ Library->Library Manager

#include <M5StickCPlus.h>

#define LGFX_AUTODETECT
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
static LGFX lcd;

#include <WiFi.h>
#include <stdlib.h>
#include <stdio.h>
#include "time.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Ticker.h>

Ticker tickerWeatherUpdate;

const char *ntpServer = "ntp.jst.mfeed.ad.jp";
const long gmtOffset_sec = 9 * 3600;
const int daylightOffset_sec = 0;

//NTP関連
struct tm timeInfo;
int now_hour;
char date[20], hour_minute_sec[20];

WiFiClient client;

//ユーザーによって変更する必要のあるもの
#define WIFI_SSID "******" //Wi-FiのSSID
#define WIFI_PASS "******" //Wi-Fiのパスワード

#define LOCATE "130010" //地域コード(https://weather.tsukumijima.net/primary_area.xml)より6桁の数字
//ここまで

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

//天気関連
String now_weather, now_temp, tom_weather, tom_temp;

#define TODAY "https://weather.tsukumijima.net/api/forecast/city/"


const String endpoint          = TODAY;
const String loc = LOCATE;


#include <Adafruit_BMP280.h>
#include "SHT20.h"
#include "yunBoard.h"
#include <math.h>
#include "display.h"

SHT20 sht20;
Adafruit_BMP280 bmp;

uint32_t update_time = 0;
float tmp, hum;
float pressure;
uint16_t light;
extern uint8_t  lightR;
extern uint8_t  lightG;
extern uint8_t  lightB;

boolean tt = false;

//Wi-Fi接続、できなかったら再起動
void WiFiConnect() {
  int cnt = 0;
  lcd.setCursor(2, 91);
  lcd.print("Wi-Fi Connecting");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    cnt++;
    delay(1000);
    Serial.print(".");
    lcd.print(".");
    if (cnt % 10 == 0) {
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      Serial.println("");
    }
    if (cnt >= 150) {
      ESP.restart();
    }
  }
  Serial.print("\nWiFi connected\n");
  Serial.println(WiFi.localIP());
  lcd.fillRect      ( 2, 91, 240, 40   , BLACK);
}

//天気を取得する関数
void get_weather_data() {
  HTTPClient http;

  http.begin(endpoint + loc); //URLを指定
  int httpCode = http.GET();  //GETリクエストを送信

  if (httpCode > 0) { //返答がある場合

    String payload = http.getString();  //返答（JSON形式）を取得
    Serial.println(httpCode);
    Serial.println(payload);

    //jsonオブジェクトの作成
    DynamicJsonBuffer jsonBuffer;
    String json = payload;
    JsonObject& weatherdata = jsonBuffer.parseObject(json);

    //パースが成功したかどうかを確認
    if (!weatherdata.success()) {
      Serial.println("parseObject() failed");
    }

    //データを抜き出し（今日）
    const char* td_weather = weatherdata["forecasts"][0]["telop"].as<char*>();
    const double td_tempMin = weatherdata["forecasts"][0]["temperature"]["min"]["celsius"].as<double>();
    const double td_tempMax = weatherdata["forecasts"][0]["temperature"]["max"]["celsius"].as<double>();
    //データを抜き出し（明日）
    const char* tm_weather = weatherdata["forecasts"][1]["telop"].as<char*>();
    const double tm_tempMin = weatherdata["forecasts"][1]["temperature"]["min"]["celsius"].as<double>();
    const double tm_tempMax = weatherdata["forecasts"][1]["temperature"]["max"]["celsius"].as<double>();

    //表示用変数に各要素をセット
    now_weather = td_weather;
    now_temp = String(td_tempMax, 0) + "/" + String(td_tempMin, 0);
    tom_weather = tm_weather;
    tom_temp = String(tm_tempMax, 0) + "/" + String(tm_tempMin, 0);
  }


  else {
    Serial.println("Error on HTTP request");
  }

  http.end(); //Free the resources
}


void get_time() {
  getLocalTime(&timeInfo);//tmオブジェクトのtimeInfoに現在時刻を入れ込む
  sprintf(date, "%04d/%02d/%02d ", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday); //日付に変換
  sprintf(hour_minute_sec, "%02d:%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec); //時間に変換
  now_hour = timeInfo.tm_hour;
}


void setup() {
  lcd.init();
  //lcd.setFont(&fonts::lgfxJapanGothic_12);
  int8_t i, j;
  M5.begin();
  Wire.begin(0, 26, 100000);
  lcd.setRotation(1);
  lcd.setTextSize(2);

  // RGB888
  // led_set(uint8_t 1, 0x080808);

  if (!bmp.begin(0x76)) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
  }
  WiFiConnect();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  //Tickerの設定
  tickerWeatherUpdate.attach(900, get_weather_data);
  get_weather_data();



  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_1000); /* Standby time. */

  // put your setup code here, to run once:
  //  display_light4();
}

uint8_t light_flag = 1;
String disp_weather;

void loop() {

  if (millis() > update_time) {
    update_time = millis() + 1000;
    tmp = sht20.read_temperature() - 6;
    hum = sht20.read_humidity();
    light = light_get();
    pressure = bmp.readPressure() / 100;
    get_time();

    lcd.setFont(&fonts::Font0);

    lcd.setCursor(5, 5);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.print(date);
    lcd.print(hour_minute_sec);

    lcd.setCursor(5, 5 + 20);
    lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    lcd.printf("%.1fC\r\n", tmp);
    lcd.setCursor(90, 5 + 20);
    lcd.setTextColor(TFT_RED, TFT_BLACK);
    lcd.printf("%d", int(hum));
    lcd.print("%\r\n");
    lcd.setCursor(140, 5 + 20);
    lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    lcd.printf("%d hPa\r\n", int(pressure));

    lcd.setFont(&fonts::lgfxJapanGothic_16);

    lcd.setCursor(5, 25 + 25);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    if (tt == false) {
      disp_weather = now_weather;
      lcd.setTextColor(0xFFFF00U, 0x000000U);
      lcd.print("今日:");
      lcd.setTextColor(0xFFFFFFU, 0x000000U);
      lcd.print(now_weather);
      lcd.print("    ");
    }
    else {
      disp_weather = tom_weather;
      lcd.setTextColor(0x0000FFU, 0x000000U);
      lcd.print("明日:");
      lcd.setTextColor(0xFFFFFFU, 0x000000U);
      lcd.print(tom_weather);
      lcd.print("    ");
    }

    lcd.setCursor(120, 25 + 20 + 40);

    if (tt == false) {
      lcd.print(now_temp);
      lcd.print("  ");

    }
    else {
      lcd.print(tom_temp);
      lcd.print("  ");
    }

    if (disp_weather == "晴れ") {
      lightR = 8;
      lightG = 8;
      lightB = 0;
    }
    else if (disp_weather == "雨") {
      lightR = 0;
      lightG = 0;
      lightB = 10;
    }
    else if (disp_weather == "曇") {
      lightR = 0;
      lightG = 10;
      lightB = 0;
    }
    else if (disp_weather == "雪") {
      lightR = 6;
      lightG = 6;
      lightB = 6;
    }
    else {
      lightR = 0;
      lightG = 10;
      lightB = 0;
    }
  }
  led_set_all(lightR << 16 | lightG << 8 | lightB);

  M5.update();

  if (M5.BtnA.wasPressed()) {
    tt = !tt;
  }

  delay(10);


}
