/**
 * @file ssr.ino
 *
 * solid state relay server
 * 
 * started from original code by
 * @author Pascal Gollor (http://www.pgollor.de/cms/)
 * @date 2015-09-18
 * 
 * changelog:
 * 2015-10-22: 
 * - Use new ArduinoOTA library.
 * - loadConfig function can handle different line endings
 * - remove mDNS studd. ArduinoOTA handle it.
 * 
 * 2015-12-08: 
 * - modified for SSR support
 *
 * todo:
 * [ ] immediately apply temporary changes of AP
 *     to test it before save. If it fails to connect,
 *     power off will load previously saved state
 * 
 * [ ] clean up the mess with global and local ssid/psk
 * 
 * [x] continuously read DHT sensor, humidity and temperature
 * [ ] emergency shutdown in case of excess heat or humidity
 *
 * [x] tabulated temperature display and apply/save buttons
 *
 * [ ] setup hostname
 * [ ] one click for all on/off
 */

// remote updates over the air
// (>1M flash required)
#define USE_OTA 1

// includes
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <FS.h>
#if USE_OTA
#include <ArduinoOTA.h>
#endif
#include <ESP8266WebServer.h>
#include <DHT.h>

#define DHTTYPE DHT22
#define DHTPIN  4

/**
 * @brief mDNS and OTA Constants
 * @{
 */
#define HOSTNAME "kabel-" ///< Hostename. The setup function adds the Chip ID at the end.
/// @}

/**
 * @brief Default WiFi connection information.
 * @{
 */
const char* ap_default_ssid = "kabel"; ///< Default SSID.
const char* ap_default_psk = "produzni"; ///< Default PSK.
//const char* ap_default_psk = ""; ///< Default PSK.
const char* config_name = "./ssr.conf";

String current_ssid = ap_default_ssid;
String current_psk = ap_default_psk;

float humidity = 50.0, temp_c = 20.0;  // readings from sensor

int ssr_cols = 2, ssr_rows = 3; // ssr display shown as table 2x3
#define SSR_N 6

uint8_t relay_state[] = 
{
  1, 1,
  1, 1,
  1, 1,
};

// this value will be XORed with relay state to create 
// hardware output value
#define NORMAL 0
#define INVERT 1

struct s_relay_wiring
{
  uint8_t pin, logic;  
};

struct s_relay_wiring relay_wiring[] = 
{
  {  2, INVERT }, {  5, INVERT },
  { 16, INVERT }, { 13, INVERT },
  { 14, INVERT }, { 12, INVERT },
};
// onboard led is PIN 16 INVERT

String message = "";
ESP8266WebServer server(80);
String webString="";     // String to display (runtime modified)

#ifdef DHTPIN
// Initialize DHT sensor 
// NOTE: For working with a faster than ATmega328p 16 MHz Arduino chip, like an ESP8266,
// you need to increase the threshold for cycle counts considered a 1 or 0.
// You can do this by passing a 3rd parameter for this threshold.  It's a bit
// of fiddling to find the right value, but in general the faster the CPU the
// higher the value.  The default for a 16mhz AVR is a value of 6.  For an
// Arduino Due that runs at 84mhz a value of 30 works.
// This is for the ESP8266 processor on ESP-01 
DHT dht(DHTPIN, DHTTYPE, 11); // 11 works fine for ESP8266
#endif

int emergency = 0; // in case of emergency shutdown everything

/// Uncomment the next line for verbose output over UART.
#define SERIAL_VERBOSE

// output relay state to hardware pins
void output_state()
{
  for(int i = 0; i < SSR_N; i++)
    digitalWrite(relay_wiring[i].pin, (relay_state[i] & ~emergency) ^ relay_wiring[i].logic);
}

void read_sensor()
{
  #ifdef DHTPIN
  static unsigned long previousMillis = 0;        // will store last temp was read
  static int old_emergency = -1;
  const long interval = 2000;              // interval at which to read sensor
  // Wait at least 2 seconds seconds between measurements.
  // if the difference between the current time and last time you read
  // the sensor is bigger than the interval you set, read the sensor
  // Works better than delay for things happening elsewhere also
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis >= interval)
  {
    // save the last time you read the sensor 
    previousMillis = currentMillis;   
     
    // Reading temperature for humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
    humidity = dht.readHumidity();          // Read humidity (percent)
    temp_c = dht.readTemperature(false);     // Read temperature as Fahrenheit
    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temp_c))
    {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
    emergency = temp_c > 60 || humidity > 85 ? 1 : 0;
    if(emergency != old_emergency)
    {
      old_emergency = emergency;
      output_state();
    }
  }
  #endif
}

// greate html table
// that displays ssr state
// in color and has submit buttons
void create_message()
{
  int n = 0;
  message =
"<style type=\"text/css\">"
"input[type=checkbox]"
"{"
 "/* Double-sized Checkboxes */"
 "-ms-transform: scale(2); /* IE */"
 "-moz-transform: scale(2); /* FF */"
 "-webkit-transform: scale(2); /* Safari and Chrome */"
 "-o-transform: scale(2); /* Opera */"
 "padding: 10px;"
"}"
"</style>"
"<head>"
"<meta http-equiv=\"cache-control\" content=\"max-age=0\" />"
"<meta http-equiv=\"cache-control\" content=\"no-cache\" />"
"<meta http-equiv=\"expires\" content=\"0\" />"
"<meta http-equiv=\"expires\" content=\"Tue, 01 Jan 1980 1:00:00 GMT\" />"
"<meta http-equiv=\"pragma\" content=\"no-cache\" />"
"</head>"
            "<a href=\"/\">refresh</a> "
            "<a href=\"setup\">setup</a><p/>"
            "<form action=\"/update\" method=\"get\" autocomplete=\"off\">"
            "<table>"
            // on top is row with temperature and humidity tabulated
            "<tr>"
            "<td>" + String((int)temp_c)+"&deg;C" + "</td>"
            "<td>" + String((int)humidity)+"%RH" + "</td>"
            "</tr>";
  for(int y = 0; y < ssr_rows; y++)
  {
    message += "<tr>";
    for(int x = 0; x < ssr_cols; x++)
    {
      String input_name = "name=\"check" + String(n) + "\"";
      message += String("<td bgcolor=\"") + String(relay_state[n] ? "#00FF00" : "#FF0000") + "\">"
               + String("<input type=\"checkbox\" ") + input_name + String(relay_state[n] ? " checked" : "") + "> </input>"
               + "<button type=\"submit\" name=\"button"
               + String(n) 
               + "\" value=\"" 
               + String(relay_state[n] ? "0" : "1") 
               + "\">" // toggle when clicked 
               + String(relay_state[n] ? "ON" : "OFF") // current state
               + "</button>"
                 "</td>";
      n++; // increment ssr number
    }
    message += "</tr>";
  }
  message += "<tr>"
             "<td>"
             "<button type=\"submit\" name=\"apply\" value=\"1\">Apply</button>"
             "</td>"
             "<td>"
             "<button type=\"submit\" name=\"save\" value=\"1\">Save</button>"
             "</td>"
             "</tr>"
             "</table>"
             "</form>";
}

/**
 * @brief Read WiFi connection information from file system.
 * @param ssid String pointer for storing SSID.
 * @param pass String pointer for storing PSK.
 * @return True or False.
 * 
 * The config file have to containt the WiFi SSID in the first line
 * and the WiFi PSK in the second line.
 * Line seperator can be \r\n (CR LF) \r or \n.
 */
bool loadConfig(String *ssid, String *pass)
{
  // open file for reading.
  File configFile = SPIFFS.open(config_name, "r");
  if (!configFile)
  {
    Serial.print("Failed to load ");
    Serial.println(config_name);
    return false;
  }

  // Read content from config file.
  String content = configFile.readString();
  configFile.close();
  
  content.trim();

  // Check if there is a second line available.
  int8_t pos = content.indexOf("\r\n");
  uint8_t le = 2;
  // check for linux and mac line ending.
  if (pos == -1)
  {
    le = 1;
    pos = content.indexOf("\n");
    if (pos == -1)
      pos = content.indexOf("\r");
  }

  // If there is no second line: Some information is missing.
  if (pos == -1)
  {
    Serial.println("Infvalid content.");
    Serial.println(content);
    return false;
  }

  // check for the third line
  // Check if there is a second line available.
  int8_t pos2 = content.indexOf("\r\n", pos + le + 1);
  uint8_t le2 = 2;
  // check for linux and mac line ending.
  if (pos2 == -1)
  {
    le2 = 1;
    pos2 = content.indexOf("\n", pos + le + 1);
    if (pos2 == -1)
      pos2 = content.indexOf("\r", pos + le + 1);
  }
  
  // If there is no third line: Some information is missing.
  if (pos2 == -1)
  {
    Serial.println("Invalid content.");
    Serial.println(content);
    return false;
  }

  // Store SSID and PSK into string vars.
  *ssid = content.substring(0, pos);
  *pass = content.substring(pos + le, pos2);

  // get relay state
  String ssr_state = content.substring(pos2 + le2);
  for(int i = 0; i < ssr_state.length() && i < SSR_N; i++)
    relay_state[i] = (ssr_state.substring(i,i+1) == "1" ? 1 : 0);
  output_state();
  ssid->trim();
  pass->trim();

#ifdef SERIAL_VERBOSE
  Serial.println("----- file content -----");
  Serial.println(content);
  Serial.println("----- file content -----");
  Serial.println("ssid: " + *ssid);
  Serial.println("psk:  " + *pass);
  Serial.println("ssr:  " +  ssr_state);
#endif

  return true;
} // loadConfig


/**
 * @brief Save WiFi SSID and PSK to configuration file.
 * @param ssid SSID as string pointer.
 * @param pass PSK as string pointer,
 * @return True or False.
 */
bool saveConfig(String *ssid, String *pass)
{
  // Open config file for writing.
  File configFile = SPIFFS.open(config_name, "w");
  if (!configFile)
  {
    Serial.print("Failed to save ");
    Serial.println(config_name);
    return false;
  }

  // Save SSID and PSK.
  configFile.println(*ssid);
  configFile.println(*pass);

  for(int i = 0; i < SSR_N; i++)
    configFile.print(relay_state[i] ? "1" : "0");
  configFile.println("");

  configFile.close();
  
  return true;
} // saveConfig

// format filesystem (erase everything)
// place default password file
void format_filesystem(void)
{
  String station_ssid = ap_default_ssid;
  String station_psk = ap_default_psk;

  Serial.println("Formatting"); // erase everything
  SPIFFS.format();
  
  Serial.println("Saving factory default");
  saveConfig(&station_ssid, &station_psk);
}

/**
 * @brief Arduino setup function.
 */
void setup()
{
  String station_ssid = "";
  String station_psk = "";

  Serial.begin(115200);
  delay(100);

  Serial.println("\r\n");
  Serial.print("Chip ID: 0x");
  Serial.println(ESP.getChipId(), HEX);

  // Set Hostname.
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);

  // Print hostname.
  Serial.println("Hostname: " + hostname);
  //Serial.println(WiFi.hostname());

  // Initialize file system.
  if (!SPIFFS.begin())
  {
    Serial.println("Failed to mount file system");
    return;
  }

  // Load wifi connection information.
  if (! loadConfig(&station_ssid, &station_psk))
  {
    station_ssid = "";
    station_psk = "";

    Serial.println("No WiFi connection information available.");
    format_filesystem();
    Serial.println("Trying again");
    
    if (! loadConfig(&station_ssid, &station_psk))
    {
      station_ssid = "";
      station_psk = "";

      Serial.println("Second time failed. Cannot create filesystem.");
    }
  }

  for(int i = 0; i < SSR_N; i++)
    pinMode(relay_wiring[i].pin, OUTPUT);

  // Check WiFi connection
  // ... check mode
  if (WiFi.getMode() != WIFI_STA)
  {
    WiFi.mode(WIFI_STA);
    delay(10);
  }

  // ... Compare file config with sdk config.
  if (WiFi.SSID() != station_ssid || WiFi.psk() != station_psk)
  {
    Serial.println("WiFi config changed.");

    // ... Try to connect to WiFi station.
    WiFi.begin(station_ssid.c_str(), station_psk.c_str());

    // ... Pritn new SSID
    Serial.print("new SSID: ");
    Serial.println(WiFi.SSID());

    // ... Uncomment this for debugging output.
    //WiFi.printDiag(Serial);
  }
  else
  {
    // ... Begin with sdk config.
    WiFi.begin();
  }

  Serial.println("Wait for WiFi connection.");

  // ... Give ESP 10 seconds to connect to station.
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000)
  {
    Serial.write('.');
    //Serial.print(WiFi.status());
    delay(500);
  }
  Serial.println();

  // Check connection
  if(WiFi.status() == WL_CONNECTED)
  {
    // ... print IP Address
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("No connection to remote AP. Becoming AP itself.");
    Serial.println(ap_default_ssid);
    Serial.println(ap_default_psk);
    // Go into software AP mode.
    WiFi.mode(WIFI_AP);

    delay(10);

    WiFi.softAP(ap_default_ssid, ap_default_psk);

    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
  }
  current_ssid = station_ssid;
  current_psk = station_psk;

#if USE_OTA
  // Start OTA server.
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.begin();
#endif

  server.on("/", handle_root);
  server.on("/read", handle_read);
  server.on("/setup", handle_setup);
  server.on("/update", handle_update);
  create_message();
  server.begin();
  Serial.println("HTTP server started");
}

// when user requests root web page
void handle_root() {
  create_message();
  server.send(200, "text/html", message);
}

// when user requests /read web page
void handle_read() {
  webString = message 
            + "Temperature: " + String((int)temp_c)+"&deg;C"
            + " Humidity: "   + String((int)humidity)+"%";   // Arduino has a hard time with float to string
  server.send(200, "text/html", webString);            // send to someones browser when asked
}

void handle_setup() {
  String new_ssid = "", new_psk = "";
  webString = "<form action=\"/setup\" method=\"get\" autocomplete=\"off\">"
              "Access point: <input type=\"text\" name=\"ssid\"><br>"
              "Password: <input type=\"text\" name=\"psk\"><br>"
              "<button type=\"submit\" name=\"apply\" value=\"1\">Apply</button>"
              "<button type=\"submit\" name=\"discard\" value=\"1\">Discard</button>"
              "</form>";
  for(int i = 0; i < server.args(); i++)
  {
    if(server.argName(i) == "discard")
    {
      loadConfig(&current_ssid, &current_psk);
      create_message();
      webString = message;
    }
    if(server.argName(i) == "apply")
    {
      for(int j = 0; j < server.args(); j++)
      {
        if(server.argName(j) == "ssid")
        {
          new_ssid = server.arg(j);
        }
        if(server.argName(j) == "psk")
        {
          new_psk = server.arg(j);
        }
      }
      if(new_ssid.length() > 0 && new_psk.length() >= 8)
      {
        //Serial.println("Save config");
        current_ssid = new_ssid;
        current_psk = new_psk;
        //saveConfig(&current_ssid, &current_psk);
        //reboot = 1;
        //loadConfig(&current_ssid, &current_psk);
        create_message();
        webString = message 
          + String("Click Save for new login:<p/>")
          + "Access point: " + current_ssid + "<br/>"
          + "Password: " + current_psk + "<p/>"
          + "Settings will be active after next power up.";
      }
    }
  }
  server.send(200, "text/html", webString);            // send to someones browser when asked
}

void handle_update() {
  // Apply or Save button
  for(int i = 0; i < server.args(); i++)
  {
    if(server.argName(i) == "apply" || server.argName(i) == "save")
    {
      // assume all are off
      for(int j = 0; j < SSR_N; j++)
        relay_state[j] = 0;
      // checkboxes on
      for(int j = 0; j < server.args(); j++)
      {
        if(server.argName(j).startsWith("check"))
        {
          int n = server.argName(j).substring(5).toInt();
          if(n >= 0 && n < SSR_N)
            if(server.arg(j) == "on")
              relay_state[n] = 1;
        }
      }
    }
    if(server.argName(i) == "save")
      saveConfig(&current_ssid, &current_psk);
  }
  // ON/OFF buttons
  for(int i = 0; i < server.args(); i++)
  {
    if(server.argName(i).startsWith("button"))
    {
      int n = server.argName(i).substring(6).toInt();
      if(n >= 0 && n < SSR_N)
        relay_state[n] = server.arg(i).toInt();
    }
  };
  output_state();
  create_message();
  webString = message;
  #if 0
  // some debugging print post/get messages
  webString += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  for (int i = 0; i < server.args(); i++ )
  {
    webString += " " + server.argName(i) + ": " + server.arg(i);
  };
  #endif
  server.send(200, "text/html", webString);
}

/**
 * @brief Arduino loop function.
 */
void loop()
{
  #if USE_OTA
  // Handle OTA server.
  ArduinoOTA.handle();
  #endif
  // Handle web server
  server.handleClient();
  read_sensor();
  yield();
}

