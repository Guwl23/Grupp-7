/*
 * Weather Station Firmware
 * Version: 1.03
 * Fetches and displays current and historical weather data from SMHI API
 * Features:
 * - 24-hour forecast graphs
 * - Historical data (30 days)
 * - Configurable settings saved to LittleFS
 * - Multi-city support
 */
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc_cal.h"
#include <SPI.h>
#include "pin_config.h"
#include <WiFi.h>
#include <HTTPClient.h>
// Libraries for persistent settings (LittleFS) and file operations
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>




// Remember to remove these before commiting in GitHub
String ssid = "iPhone";
String password = "12345670";

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
  tft.drawString("version 1.03", 20, 10);
  tft.drawString("Group 7", 100, 50);
  delay(4000);
}

// Defines the city structure format for proper API access
struct City {
  String name;
  float lon;
  float lat;
  int stationid;
};

// Defines city data structures for API access
const City cities[] = {
  {"Stockholm", 18.0686, 59.3293,97200},
  {"Malmo", 13.0038, 55.6050,52350},
  {"Goteborg", 11.9746, 57.7089,71420},
  {"Karlskrona", 15.6500, 56.1833,65090}
};
// Settings structure with boolean toggles for weather parameters
struct Settings {
  bool showTemperature;
  bool showHumidity;
  bool showWindSpeed;
  bool histShowTemperature;  // For historical temperature
  bool histShowHumidity;     // For historical humidity
  bool histShowWindSpeed;    //For historical wind speed
  City city;
};

// Create defaultSettings and currentSettings instances
Settings defaultSettings;
Settings currentSettings;
City selectedCity;

/* FUNCTION DECLARATIONS */
void displayNext24H(City city);
void displayHistoricalData(City city);
void fetchAndDrawParameter(City city, int parameter, String title, uint16_t color);

float temps[24];// Array to store 24 temperature values (in Celsius)
int symbols[24];// Array to store 24 weather symbol codes (Wsymb2 values from SMHI API)

// SMHI Parameter IDs
const int PARAM_TEMP = 1; // Temperature in Celsius
const int PARAM_HUMIDITY = 6;// Relative humidity in %
const int PARAM_WIND_SPEED = 4;// Wind speed in m/s

/*Väljer en stad för att få åtkost till rätt API:er från city,
den valda staden kan ändras vid kallelse av funktionen */
void chooseCity() {
  int currentIndex = 0;
  bool chosen = false;

  String city_options[] = {"Stockholm", "Malmo", "Goteborg", "Karlskrona"};
  int numCities = sizeof(city_options) / sizeof(city_options[0]); // Om vi ville lägga till fler städer senare

  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  delay(1000);
 // City selection menu loop
  while (!chosen) {
    tft.drawString("Choose City:", 20, 10); // Draw menu title
      // Display all city options
    for (int i = 0; i < numCities; i++) {
      int y = 40 + i * 20; // Calculate vertical position for each menu item (spaced 20 pixels apart)
      if (i == currentIndex) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);  // Highlight currently selected city
        tft.drawString("> " + city_options[i], 40, y);
      } else {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("  " + city_options[i], 40, y);
      }
    }
    // Handle button navigation (Button 1)
    if (digitalRead(PIN_BUTTON_1) == LOW) {
      currentIndex = (currentIndex + 1) % numCities;
      delay(300);
    }
     // Handle selection confirmation (Button 2)
    if (digitalRead(PIN_BUTTON_2) == LOW) {
      selectedCity = cities[currentIndex];
      chosen = true;
      delay(300);
    }
  }

  // Bekräftelsevisning
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Vald stad: " + selectedCity.name, 30, 60);
  delay(1000);
}



/* Ritar upp grafaxlarna samt axelnumreringen för grafen på displayen.
Den ritar även upp punkterna för temperaturen och vädersymbolen över temperatur punkten.
Denna kallas sedan på från displayNext24H*/
void drawTempGraph(float temps[], int symbols[]) {
  tft.setTextColor(TFT_WHITE);
  tft.fillScreen(TFT_BLACK);
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
    int y = baseY - ((temps[i] + 10) * (graphHeight / 40.0)); //tempskala från -10 till 30 grader


    if (i > 0) {
      int prevX = baseX + ((i) * (graphWidth / 24));
      int prevY = baseY - ((temps[i - 1] + 10) * graphHeight / 40.0);
      tft.drawLine(prevX, prevY, x, y, TFT_BLUE);
    }

    int textY = y - 20; //Distansen mellan temp koordinat och symbol koordinat
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(x, textY);
    tft.print(symbols[i]);

    //Visar var 3:e timme
    if (i % 3 == 0) {
      tft.setTextSize(1);
      tft.drawString(String(i) + "h", x - 5, baseY + 5);
    }
  }
}


void drawMonthlyGraph(float data[], int numDays, String title, uint16_t color) {
  // Rensa bara diagramområdet (behåll menytexter etc)
  tft.fillRect(0, 40, 320, 100, TFT_BLACK);

  // Rita titel med angiven färg
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString(title + " senaste " + String(numDays) + " dagar", 10, 40);

  // Diagraminställningar
  int graphHeight = 80;
  int graphWidth = 220;
  int baseY = 120;
  int baseX = 20;

  // Hitta min/max-värden för automatisk skalning
  float minVal = data[0];
  float maxVal = data[0];
  for (int i = 1; i < numDays; i++) {
    if (data[i] < minVal) minVal = data[i];
    if (data[i] > maxVal) maxVal = data[i];
  }

  // Justera skalningen för bättre visning
  minVal = floor(minVal) - 2;  // Rundar ner och ger lite marginal
  maxVal = ceil(maxVal) + 2;   // Rundar upp och ger lite marginal
  if (maxVal - minVal < 5) maxVal = minVal + 5; // Förhindrar för platta diagram

  // Rita Y-axel med skalmarkeringar
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  for (int i = 0; i <= 5; i++) {
    float value = minVal + (maxVal - minVal) * i / 5;
    int y = baseY - (graphHeight * i / 5);
    tft.drawLine(baseX - 5, y, baseX, y, TFT_WHITE);
    tft.setCursor(0, y - 6);
    tft.print(String(value, 1)); // Visar med 1 decimal
  }

  // Rita X-axel
  tft.drawLine(baseX, baseY, baseX + graphWidth, baseY, TFT_WHITE);

  // Rita datalinjen med angiven färg
  for (int i = 0; i < numDays; i++) {
    int x = baseX + (i * (graphWidth / numDays));
    int y = baseY - map(data[i], minVal, maxVal, 0, graphHeight);

    // Rita linje till föregående punkt
    if (i > 0) {
      int prevX = baseX + ((i - 1) * (graphWidth / numDays));
      int prevY = baseY - map(data[i - 1], minVal, maxVal, 0, graphHeight);
      tft.drawLine(prevX, prevY, x, y, color);
    }

    // Rita datapunkter som små cirklar
    tft.fillCircle(x, y, 2, color);

    // Visa dagsetiketter
    if (i % 5 == 0 || i == numDays - 1) {
      tft.drawString(String(i + 1) + "d", x - 5, baseY + 5);
    }
  }

  // Rita enkel förklaring
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Data: SMHI Open Data", 5, 155);
}

/*Tar in vilken stad det är från city för att sedan hämta rätt API via lon och lat.
Därefter görs detta till ett läsbart dokument av data. Där timeSeries hittas och
vidare sökes sedan 24 första validTimes för att få ut de 24 första temperaturerna
samt vädersymbolerna.*/
void displayNext24H(City city){
  // Väderdata hämtas från SMHI:s öppna API (https://opendata.smhi.se)
  // Licens: Creative Commons Attribution 4.0 (CC BY 4.0)
  String url = "https://opendata-download-metfcst.smhi.se/api/category/pmp3g/version/2/geotype/point/lon/"
    + String(city.lon, 0) + "/lat/" + String(city.lat, 0) + "/data.json" ;

  HTTPClient client;
  client.begin(url);
  int httpCode = client.GET();

  if (httpCode != 200) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Fel vid hämtning!", 10, 10);
    return;
  }

  String json = client.getString();
  DynamicJsonDocument doc(80000);
  deserializeJson(doc, json);

  JsonArray timeSeries = doc["timeSeries"];
  Serial.println("Number of time series data: " + String(timeSeries.size()));

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 15);
  tft.setTextSize(1);
  tft.println(city.name);

  int count = 0;

  for (JsonObject item : timeSeries) {
    String time = item["validTime"];
    if (count >= 24) break;

    JsonArray params = item["parameters"];
    float temp = NAN;
    int symbol = NAN;

    for (JsonObject p : params) {
      if (p["name"] == "t") {
        temp = p["values"][0];
      }
      if (p["name"] == "Wsymb2") {
        symbol = p["values"][0];
      }
    }

    if (!isnan(temp)) {
      temps[count] = temp;
      symbols[count] = symbol;
      count++;
    }
  }

  drawTempGraph(temps, symbols);

  client.end();
}
/*
  Displays historical weather data for a given city
  Shows temperature, humidity, and wind speed based on user settings
  Data is fetched from SMHI's open data API

 * @param city The city object containing station ID and name
 */
void displayHistoricalData(City city) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Historical Data", 10, 10);
  tft.setCursor(0, 15);
  tft.setTextSize(1);
  tft.println(city.name);
 // Display each parameter if enabled in settings
  if (currentSettings.histShowTemperature) {
      fetchAndDrawParameter(city, 1, "Temp (°C)", TFT_RED);
  }
  if (currentSettings.histShowHumidity) {
      fetchAndDrawParameter(city, 6, "Humidity (%)", TFT_BLUE);
  }
  if (currentSettings.histShowWindSpeed) {
      fetchAndDrawParameter(city, 4, "Wind (m/s)", TFT_GREEN);
  }
// Draw footer menu items
  tft.drawString("Menu", 270, 150);
  tft.drawString("Data: SMHI Open Data", 5, 155);
}
/*
  Fetches historical weather data from SMHI API and draws it as a graph
  Handles one weather parameter at a time (temp, humidity, etc.)

  @param city The city to get data for
  @param parameter The SMHI parameter ID to fetch
  @param title The display title for the parameter
  @param color The color to use when drawing the graph
 */
void fetchAndDrawParameter(City city, int parameter, String title, uint16_t color) {
  String url = "https://opendata-download-metobs.smhi.se/api/version/latest/parameter/" +
             String(parameter) + "/station/" + String(city.stationid) +
             "/period/latest-months/data.json";

  HTTPClient client;
  client.begin(url);
  client.setTimeout(10000);

  int httpCode = client.GET();

  if (httpCode == HTTP_CODE_OK) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, client.getString());

      if (!error) {
          float data[30] = {0};
          int count = 0;
          String lastDate = "";

          JsonArray values = doc["value"];
          for (JsonObject v : values) {
              if (count >= 30) break;

              String dateTime = v["date"].as<String>();
              String dateOnly = dateTime.substring(0, 10);
             // Only take one value per day
              if (dateOnly != lastDate) {
                  float value = v["value"];
                  if (!isnan(value)) {
                      data[count] = value;
                      count++;
                      lastDate = dateOnly;
                  }
              }
          }
          // Draw the graph with the collected data
          drawMonthlyGraph(data, count, title, color);
          delay(1500);
      } else {
          Serial.print("JSON error: ");
          Serial.println(error.c_str());
      }
  } else {
      Serial.print("HTTP error: ");
      Serial.println(httpCode);
  }

  client.end();
}




// Skapar funktion för att spara användarens valda default settings till en fil
void saveDefaultsToFile() {
  File file = LittleFS.open("/defaults.json", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  DynamicJsonDocument doc(256);
  doc["showTemperature"] = defaultSettings.showTemperature;
  doc["showHumidity"] = defaultSettings.showHumidity;
  doc["showWindSpeed"] = defaultSettings.showWindSpeed;
  doc["histShowTemperature"] = defaultSettings.histShowTemperature;
  doc["histShowHumidity"] = defaultSettings.histShowHumidity;
  doc["histShowWindSpeed"] = defaultSettings.histShowWindSpeed;
  doc["city"] = defaultSettings.city.name;

  serializeJson(doc, file);
  file.close();
  Serial.println("Defaults saved to LittleFS");
}


// Skapar funktion för att ladda in användarens valda default settings
void loadDefaultsFromFile() {
  if (!LittleFS.exists("/defaults.json")) {
    Serial.println("Defaults file not found, using hardcoded defaults");
    return;
  }


  File file = LittleFS.open("/defaults.json", "r");  // Öppna filen för att läsa den
  if (!file) {
    Serial.println("Failed to open defaults file");
    return;
  }

  DynamicJsonDocument doc(256);

  DeserializationError error = deserializeJson(doc, file);

  // Stäng filen efter att ha läst den
  file.close();

  if (error) {
    Serial.println("Failed to parse defaults file");
    return;
  }

  // Ge värden till defaultSettings
  defaultSettings.showTemperature = doc["showTemperature"] | false;
  defaultSettings.showHumidity = doc["showHumidity"] | false;
  defaultSettings.showWindSpeed = doc["showWindSpeed"] | false;
  defaultSettings.histShowTemperature = doc["histShowTemperature"] | true;
  defaultSettings.histShowHumidity = doc["histShowHumidity"] | false;
  defaultSettings.histShowWindSpeed = doc["histShowWindSpeed"] | false;


  // Matcha city name till hela City struct objektet
  String cityName = doc["city"].as<String>();
  for (City c : cities) {
    if (c.name == cityName) {
      defaultSettings.city = c;
      break;
    }
  }

  // Gör värden till currentSettings
  currentSettings = defaultSettings;
  Serial.println("Defaults loaded from LittleFS");
}




/**
 * Displays a temporary message on screen and refreshes the settings menu
 *
 * @param message The text message to display (centered on screen)
 * @param selectedOption Reference to currently selected menu option (to maintain selection state)
 */
void flashMessage(String message, int & selectedOption) {
  tft.setTextSize(2);
   // Calculate message positioning (centered horizontally and vertically)
  int textWidth = message.length() * 12; // Approximate pixel width of text
  int x = (tft.width() - textWidth) / 2; // Center X position
  int y = (tft.height() - 16) / 2; // Center Y position
// Clear message area with black rectangle (slightly larger than text)
  tft.fillRect(0, y - 5, tft.width(), 30, TFT_BLACK);
   // Display message in green text
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(message, x, y);
  delay(1000);
  tft.fillRect(0, y - 5, tft.width(), 30, TFT_BLACK);
 // Refresh settings menu to previous state
  SettingsLayout(selectedOption);
}


/*Uppbyggnaden av settings funktionen där en bläddningsfunktion används
...*/
//U.S 4.1 - As a user, I want to access a settings menu to configure weather data display options.
void SettingsLayout(int selectedOption) {

  tft.fillRect(0, 50, 240, 100, TFT_BLACK); // Rensa settings-listan

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  int startY = 50;
  int spacing = 12;

  String options[] = {
  "Temperature",
  "Humidity",
  "Wind Speed",
  "Choose City",
  "Historical Data",
  "Hist. Temperature",
  "Hist. Humidity",
  "Hist. Wind Speed",
  "Apply Defaults",
  "Configure Defaults"
  };

  for (int i = 0; i < 10; i++) {
    String optionText = "  " + options[i];

    // Add ON/OFF markers
    if (i == 0) {
        optionText += currentSettings.showTemperature ? " [ON]" : " [OFF]";
    } else if (i == 1) {
        optionText += currentSettings.showHumidity ? " [ON]" : " [OFF]";
    } else if (i == 2) {
        optionText += currentSettings.showWindSpeed ? " [ON]" : " [OFF]";
    } else if (i == 5) {
        optionText += currentSettings.histShowTemperature ? " [ON]" : " [OFF]";
    } else if (i == 6) {
        optionText += currentSettings.histShowHumidity ? " [ON]" : " [OFF]";
    } else if (i == 7) {
        optionText += currentSettings.histShowWindSpeed ? " [ON]" : " [OFF]";
    }

    int yPos = startY + spacing * (i + 1);
    if (i == selectedOption) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("> " + optionText, 40, yPos);
    } else {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(optionText, 40, yPos);
    }
}
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

  // Startar LittleFS biblioteket för att kunna spara default settings i separat fil
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount (and format) failed.");
  }

  // Debugging
  Serial.println("FS Contents:");
  File root = LittleFS.open("/", "r");
  File entry;
  while (entry = root.openNextFile()) {
    Serial.print("  ");
    Serial.println(entry.name());
    entry.close();
  }
  root.close();

  // Visa bootscreen
  bootScreen();

  bool firstRun = !LittleFS.exists("/defaults.json");
  Serial.print("firstRun = "); Serial.println(firstRun);

  if (firstRun) {
    // Första gången som programmet körs och användaren får frågan om choosecity
    chooseCity();

    defaultSettings.city = selectedCity;
    defaultSettings.showTemperature = false;
    defaultSettings.showHumidity    = false;
    defaultSettings.showWindSpeed   = false;
    defaultSettings.histShowTemperature = true;  // Default to showing temperature
    defaultSettings.histShowHumidity = false;
    defaultSettings.histShowWindSpeed = false;

    currentSettings = defaultSettings;
    saveDefaultsToFile();     // Skriv ut dem så att nästa boot inte är firstrun
    Serial.println("-> saved defaults.json");
  }
  else {
    // Alla andra boots: Starta med de inställningarna som användaren har valt
    loadDefaultsFromFile();
    selectedCity = defaultSettings.city;
  }

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
  static int selectedOption = 0;

    if (digitalRead(PIN_BUTTON_1) == LOW && currentPage == -1) {
      currentPage    = 1;      // Gå till Settings
      selectedOption = 0;      // Reset:a pilen till översta alternativet i Settings screen
      delay(200);              // debounce
      while (digitalRead(PIN_BUTTON_1) == LOW) delay(10);  // Vänta på att användaren ska släppa taget
      return;                 // Förhindra att annan kod i settings screen som choose city körs direkt
    }

    if (digitalRead(PIN_BUTTON_2) == LOW && currentPage == -1) {
      currentPage = 0;        // Go to Forecast
      delay(200);
      while (digitalRead(PIN_BUTTON_2) == LOW) delay(10);
      return;                 // Förhindra att annan kod i settings screen som choose city körs direkt
    }

  /*US2.2B: As a user, I want to access the menu from anywhere in the program
  by holding both buttons simultaneously. */
  if (digitalRead(PIN_BUTTON_1) == LOW && digitalRead(PIN_BUTTON_2) == LOW) {
    if (currentPage != -1) currentPage = -1;
    delay(200);
  }

  if (currentPage == 1) {
    // Navigera nedåt längs Settings med översta knappen
    if (digitalRead(PIN_BUTTON_1) == LOW) {
      delay(300);
      selectedOption = (selectedOption + 1) % 10;  // Gå tillbaka till översta inställningen om du trycker på knappen när du är vid nedersta inställningen
      SettingsLayout(selectedOption);  // Rita om settings screen med de nya valen
      delay(200);
    }

    // Välj alternativ med nedersta knappen
    //Del av US 4.1
    if (digitalRead(PIN_BUTTON_2) == LOW) {
      if (selectedOption == 0) {  // "Show Temperature"
        currentSettings.showTemperature = !currentSettings.showTemperature;
        SettingsLayout(selectedOption); // Uppdatera displayen med nya värden
      }

      else if (selectedOption == 1) {  // "Show Humidity"
        currentSettings.showHumidity= !currentSettings.showHumidity;
      SettingsLayout(selectedOption); // Uppdatera displayen med nya värden
      }

      else if (selectedOption == 2) {  // "Show Wind Speed"
        currentSettings.showWindSpeed = !currentSettings.showWindSpeed;
        SettingsLayout(selectedOption); // Uppdatera displayen med nya värden
      }
      /*U.S 4.3 - As a user, I want to select different cities to view their
      weather data for the historical data and starting screen forecast. */
      else if (selectedOption == 3) {  // "Choose City"
        chooseCity();
        currentSettings.city = selectedCity;
        currentPage = -1;
      }
      else if (selectedOption == 4) { // "Historical Data"
        currentPage = 2;
      }
      else if (selectedOption == 5){
         currentSettings.histShowTemperature = !currentSettings.histShowTemperature;
         SettingsLayout(selectedOption); // Uppdatera displayen med nya värden
      }

      else if (selectedOption == 6){
         currentSettings.histShowHumidity = !currentSettings.histShowHumidity;
         SettingsLayout(selectedOption); // Uppdatera displayen med nya värden
      }
      else if (selectedOption == 7){
         currentSettings.histShowWindSpeed = !currentSettings.histShowWindSpeed;
         SettingsLayout(selectedOption); // Uppdatera displayen med nya värden
      }

      //U.S 4.4 - As a user, I want to reset settings to default via a menu option.
      else if (selectedOption == 8) {  // "Apply Defaults"
        currentSettings = defaultSettings;
        selectedCity = defaultSettings.city; // Reset city också
        Serial.println("Defaults applied.");
        currentPage = -1;
      }
      else if (selectedOption == 9) {  //"Configure Defaults"
        defaultSettings = currentSettings;  // Update default settings with current runtime settings
        defaultSettings.city = selectedCity; // Ensure selected city is also saved to defaults
        saveDefaultsToFile();  // Spara default settings till LittleFS
        Serial.println("New defaults saved.");  // Log confirmation to serial monitor
        flashMessage("New defaults saved.", selectedOption);  // Show visual confirmation to user
      }
      else {
        Serial.println("Selected Option: " + String(selectedOption));
      }
      delay(200);
      while(digitalRead(PIN_BUTTON_2) == LOW);
    }
  }

// Check if we need to redraw the screen due to page change
  if (currentPage != lastPage) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
   // Main home screen view (page -1)
    if (currentPage == -1) {
      displayNext24H(selectedCity);  // Display 24-hour forecast graph as the primary content
      // Uses the currently selected city's data
      tft.drawString("Forecast", 225, 10); // Top button - Forecast (redundant on home screen but shown for consistency)
      tft.drawString("Settings", 225, 140); // Bottom button - Settings
      tft.drawString("Data: SMHI Open Data", 5, 150);
    }
    // Forecast page (current weather view)
    else if (currentPage == 0) {
      tft.drawString("Forecast", 10, 10);
      displayNext24H(selectedCity); ////Ritar grafen för 24 kommande timmar
      tft.drawString("Menu", 290, 10);
      tft.drawString("Data: SMHI Open Data", 5, 150);
      }
    // Settings page rendering
    else if (currentPage == 1) {
      tft.drawString("Settings", 20, 10);
      tft.setTextSize(float(1.5));
      tft.drawString("Menu", 290, 150);
      SettingsLayout(selectedOption);
    }

    //U.S 3.1 - As a user, I want to have a menu option and screen to view historical weather data
    else if (currentPage == 2) {
      displayHistoricalData(selectedCity);
      tft.setCursor(0, 15);
      tft.setTextSize(1);
      tft.println(selectedCity.name);
      tft.drawString("Menu", 270, 150);
      tft.drawString("Data: SMHI Open Data", 5, 155);
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