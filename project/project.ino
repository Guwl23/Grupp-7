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
  //float lon;
  //float lat;
  int key;
};

const City cities[] = {
  //{"Stockholm", 59.3293, 18.0686}, //byta ut lat, lon mot key
  //{"Malmo", 55.6050, 13.0038},
  //{"Goteborg", 57.7089, 11.9746},
  //{"Karlskrona", 56.1833, 15.6500}
  {"Stockholm", 97400}, //byta ut lat, lon mot key
  {"Malmö", 53360},
  {"Göteborg", 72630},
  {"Karlskrona", 65090}
};

City selectedCity;

void displayNext24H(City city);

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
      delay(300);
    }

    if (digitalRead(PIN_BUTTON_2) == LOW) {
      selectedCity = cities[currentIndex];
      chosen = true;
      delay(300);
    }

    //Bekräftar på displayen
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Choosen city: " + selectedCity.name, 60, 60);
    delay(1000);
  }
}

void drawTempGraph(float temps[24]) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Temperatur kommande 24 timmar", 10, 0);

  //Y-axeln mellan 0 och 30 grader
  int graphHeight = 100;
  int graphWidth = 240;
  int baseY = 130;
  int baseX = 10;

  //Ritar diagram axlarna
  tft.drawLine(baseX, baseY - graphHeight, baseX, baseY, TFT_WHITE); //Y-axeln
  tft.drawLine(baseX, baseY, baseX + graphWidth, baseY, TFT_WHITE); //X-axeln

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
      tft.drawString(String(i) + "h", x - 5, baseY + 5);
    }
  }
}

void displayNext24H(City city){
  String url = "https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/1/station/" + String(city.key)
     + ".json";
    // https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/1/station/17121000.json

    /*"https://opendata-download.metfcst.smhi.se/api/category/pmp3g/version/2/geotype/point/longitude/"
    + String(city.lon, 4) + "/lat/" + String(city.lat, 4) + "/data.json/" ;*/
    //här över då istället


  HTTPClient client;
  client.begin(url);
  int httpCode = client.GET();

  if (httpCode != 200) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Fel vid hämtning!", 10, 10);
    return;
  }

  String json = client.getString();
  DynamicJsonDocument doc(45000);
  deserializeJson(doc, json);

  JsonArray timeSeries = doc["timeSeries"];

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  tft.println("24h prognos i " + city.name);

  float temps[24];

  int line = 20;
  for (int i = 0; i < 24; i++) {
    String time = timeSeries[i]["validTime"];
    JsonArray params = timeSeries[i]["parameters"];

    float temp = NAN;
    for (JsonObject p : params) {
      if (p["name"] == "t") {
        temp = p["values"][0];
        break;
      }
    }

    if (!isnan(temp)) {
      temps[i] = temp;

      String hour = time.substring(11, 16);
      tft.setCursor(0, line);
      tft.print(hour + "  ");
      tft.println(String(temp, 1) + " °C");
      line += 10;
    }
  }

  drawTempGraph(temps);

  client.end();
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
  displayNext24H(selectedCity);
  delay(10000);

  //för att veta om vi får tillgång till APIn?
/**Vad jag tror så måste vi ta en liknande fil denna
(https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/1/station-set/all.json)
där vi sedan byter ut all(.json) och utifrån vilken stad man vill se så ändras keyn eller vad det nu är
för en liknande är den här
(https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/1/station/17121000.json) där
keyn är 17121000 däremot så tycks jag läsa att det bara är resultat från senaste timmen*/

  //måste hänvisa till SMHI för överstående också

}

/**
 * This is the main loop function that runs continuously after setup.
 * Add your code here to perform tasks repeatedly.
 */
void loop() {
  /*tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Hello world", 10, 10); */

  delay(1000);

  // U.S 2.1 - As a user, I want a menu to navigate between different screens using the two buttons,
  // like forecast and settings screen.

  static int currentPage = 0;
  static int lastPage = -1;

  if (digitalRead(PIN_BUTTON_1) == LOW) {
    currentPage = 0;
  }

  if (digitalRead(PIN_BUTTON_2) == LOW) {
    currentPage = 1;
  }


  if (currentPage != lastPage) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);

    if (currentPage == 0) {
      tft.drawString("Forecast", 10,10 );
    }   else if (currentPage == 1) {
      tft.drawString("Settings", 225, 10);
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