#include <Arduino.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc_cal.h"
#include <SPI.h>
#include "pin_config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>


// Remember to remove these before commiting in GitHub
String ssid = "";
String password = "";

// "tft" is the graphics libary, which has functions to draw on the screen
TFT_eSPI tft = TFT_eSPI();

// Display dimentions
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 170

WiFiClient wifi_client;

/**
  U.S 1.1
  This is the bootup screen that tells the programversion and group number.
  It is dalayed for 4 seconds
*/
void bootScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("version 1.01", 20, 10);
  tft.drawString("Group 7", 100, 50);
  delay(4000);
}


//skriver functioner här och hoppas
struct City {
  String name;
  float lon;
  float lat;
};

const City cities[] = {
  {"Stockholm", 18.0686, 59.3293},
  {"Malmo", 13.0038, 55.6050},
  {"Goteborg", 11.9746, 57.7089},
  {"Karlskrona", 15.6500, 56.1833}
};

City selectedCity;

void displayNext24H(City city);

float temps[24];

void chooseCity() {
  int currentIndex = 0;
  bool chosen = false;

  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  while (!chosen) {
    tft.fillRect(0, 40, 240, 60, TFT_BLACK);
    tft.drawString("Choose city:", 20, 10);
    tft.drawString(cities[currentIndex].name, 60, 50);

    if (digitalRead(PIN_BUTTON_1) == LOW) {
      currentIndex = (currentIndex + 1) % 4;
      delay(1000);
    }

    if (digitalRead(PIN_BUTTON_2) == LOW) {
      selectedCity = cities[currentIndex - 1]; //ändrade till -1 här om det inte fungerar bara ta bort igen
      chosen = true;
      delay(1000);
    }

    //Bekräftar på displayen
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Choose city: " + selectedCity.name, 30, 60);
    //vill vi lägga till en lista över alla städer?
    delay(1000);
  }
}

void drawTempGraph(float temps[24]) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.drawString("Temperatur kommande 24 timmar", 10, 0);

  //Y-axeln mellan 0 och 30 grader
  int graphHeight = 100;
  int graphWidth = 220;
  int baseY = 130;
  int baseX = 20;

  //Ritar diagram axlarna
  tft.drawLine(baseX, baseY - graphHeight, baseX, baseY, TFT_WHITE); //Y-axeln
  tft.drawLine(baseX, baseY, baseX + graphWidth, baseY, TFT_WHITE); //X-axeln

  //Ritar temperaturen längs y axeln
  for (int t = -10; t <= 30; t += 10) {
    int y = baseY - map(t, -10, 30, 0, graphHeight);
    tft.drawLine(baseX - 5, y, baseX, y, TFT_WHITE);
    tft.setCursor(0, y - 6);
    tft.setTextSize(1);
    tft.print(String(t));
  }

  for (int i = 0; i < 24; i++) {
    int x = baseX + (i * (graphWidth / 24));
    int y = baseY - map(temps[i], -10, 30, 0, graphHeight); //tempskala från -10 till 30 grader


    if (i > 0) {
      int prevX = baseX + ((i - 1) * (graphWidth / 24));
      int prevY = baseY - map(temps[i - 1], -10, 30, 0, graphHeight);
      tft.drawLine(prevX, prevY, x, y, TFT_BLUE);
    }

    //Visar var 3:e timme
    if (i % 3 == 0) {
      tft.setTextSize(1);
      tft.drawString(String(i) + "h", x - 5, baseY + 5);
    }
  }
}

void displayNext24H(City city){
  String url = "https://opendata-download-metfcst.smhi.se/api/category/pmp3g/version/2/geotype/point/lon/"
    + String(city.lon, 0) + "/lat/" + String(city.lat, 0) + "/data.json" ;
    //ändrade antalet decimaler från 4 till 0 i string såg ut som att den avrundade talen till heltal

  HTTPClient client;
  client.begin(url);
  int httpCode = client.GET();

  if (httpCode != 200) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Fel vid hämtning!", 10, 10);
    return;
  }

  String json = client.getString();
  JsonDocument doc;
  deserializeJson(doc, json);

  JsonArray timeSeries = doc["timeSeries"];

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 25); //Ändrade till 25 för att fllytta ner diagrammet lite så att den inte krockar med "Forecast"
  tft.setTextSize(2);
  tft.println("24h prognos i " + city.name);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Kunde inte hämta tid!");
    return;
  }

  char currentTimeStr[20];
  strftime(currentTimeStr, sizeof(currentTimeStr), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  String nowISO = String(currentTimeStr); // detta matchar SMHI:s "validTime"-format

  int count = 0;
  int line = 45; //Även här för att flytta ner Temperaturen lite.
  for (JsonObject item : timeSeries) {
    String time = item["validTime"];
    if (time < nowISO) continue; // hoppa över gamla tider
    if (count >= 24) break;

    JsonArray params = item["parameters"];
    float temp = NAN;

    for (JsonObject p : params) {
      if (p["name"] == "t") {
        temp = p["values"][0];
        break;
      }
    }

    if (!isnan(temp)) {
      temps[count] = temp;

      String hour = time.substring(11, 16);
      tft.setCursor(0, line);
      tft.print(hour + "  ");
      tft.println(String(temp, 1) + " °C");
      line += 10;
      count++;
    }
  }

  drawTempGraph(temps);

  client.end();
}

void SettingsLayout() {

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  int startY = 50;
  int spacing = 12;

  tft.drawString("Weather Parameters:", 40, startY);
  tft.drawString("Temperature", 40, startY + spacing * 1);
  tft.drawString("Humidity", 40, startY + spacing * 2);
  tft.drawString("Wind Speed", 40, startY + spacing * 3);
  tft.drawString("Choose City", 40, startY + spacing * 4); // till chooseCity() ?
  tft.drawString("Apply Defaults", 40, startY + spacing * 5);
  tft.drawString("Configure Defaults", 40, startY + spacing * 6);

}



/**
 * Setup function
 * This function is called once when the program starts to initialize the program
 * and set up the hardware.
 * Carefull when modifying this function.
 */
void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  // Wait for the Serial port to be ready
  while (!Serial);
  Serial.println("Starting ESP32 program...");
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  pinMode(PIN_BUTTON_1, INPUT_PULLUP);
  pinMode(PIN_BUTTON_2, INPUT_PULLUP);

  // Connect to WIFI
  WiFi.begin(ssid, password);

  // Will be stuck here until a proper wifi is configured
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Connecting to WiFi...", 10, 10);
    Serial.println("Attempting to connect to WiFi...");
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Connected to WiFi", 10, 10);
  Serial.println("Connected to WiFi");
  // Add your code bellow

  bootScreen();

  chooseCity();
  //displayNext24H(selectedCity);
  //delay(10000);

  //måste hänvisa till SMHI för överstående också

}

/**
 * This is the main loop function that runs continuously after setup.
 * Add your code here to perform tasks repeatedly.
 */
void loop() {
  delay(1000);

  // U.S 2.1 - As a user, I want a menu to navigate between different screens using the two buttons,
  // like forecast and settings screen.

  static int currentPage = -1;
  static int lastPage = -2;

  /*Lägger till en extra sida så att vi har en startsida som man alltid kan gå tillbaka till
  genom meny knappen och sen en av settings och en av forecast*/

  if (digitalRead(PIN_BUTTON_2) == LOW) {
    if (currentPage == -1) currentPage = 0; //Går till Forcast
  }
  if(digitalRead(PIN_BUTTON_1) == LOW && currentPage == 0) {
    currentPage = -1;
    delay(200);  // Förhindra snabb växling (debounce)
  }

  if (digitalRead(PIN_BUTTON_1) == LOW) {
    if (currentPage == -1) currentPage = 1; //Gå till Settings
    else if (currentPage == 1) currentPage = -1; //Går tillbaka till startsidan
    delay(200);  // Förhindra snabb växling (debounce)
  }


  if (currentPage != lastPage) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);

    if (currentPage == -1) {
      displayNext24H(selectedCity);  //Ritar grafen för 24 kommande timmar på startsidan
      tft.drawString("Forecast", 225, 10); //Knappen för forecast
      tft.drawString("Settings", 225, 140); //Knappen för settings
    }

    else if (currentPage == 0) {
      tft.drawString("Forecast", 10, 10);
      displayNext24H(selectedCity); ////Ritar grafen för 24 kommande timmar
      tft.drawString("Menu", 225, 150);  // Meny-knapp för att gå tillbaka till huvudmenyn
      }

    else if (currentPage == 1) {
      tft.drawString("Settings", 20, 10);
      tft.drawString("Menu", 225, 150);  // Meny-knapp för att gå tillbaka till huvudmenyn
      SettingsLayout();
    }

    lastPage = currentPage;
    delay(400);
  }




}




// TFT Pin check
  //////////////////
 // DO NOT TOUCH //
//////////////////
#if PIN_LCD_WR  != TFT_WR || \
    PIN_LCD_RD  != TFT_RD || \
    PIN_LCD_CS    != TFT_CS   || \
    PIN_LCD_DC    != TFT_DC   || \
    PIN_LCD_RES   != TFT_RST  || \
    PIN_LCD_D0   != TFT_D0  || \
    PIN_LCD_D1   != TFT_D1  || \
    PIN_LCD_D2   != TFT_D2  || \
    PIN_LCD_D3   != TFT_D3  || \
    PIN_LCD_D4   != TFT_D4  || \
    PIN_LCD_D5   != TFT_D5  || \
    PIN_LCD_D6   != TFT_D6  || \
    PIN_LCD_D7   != TFT_D7  || \
    PIN_LCD_BL   != TFT_BL  || \
    TFT_BACKLIGHT_ON   != HIGH  || \
    170   != TFT_WIDTH  || \
    320   != TFT_HEIGHT
#error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
#error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
#error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
#error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
#error  "The current version is not supported for the time being, please use a version below Arduino ESP32 3.0"
#endif