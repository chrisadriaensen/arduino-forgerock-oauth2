/*  
 *  FORGEROCK ARDUINO MKR1000 IOT DEMO
 *  
 *  Features:
 *  - Connect to WiFi (display status on LCD + LED).
 *  - Get OAuth2 device code (diaplay on LCD). 
 *  
 *  CHRIS ADRIAENSEN @ ForgeRock, 2016
 */

#include <SPI.h>
#include <WiFi101.h>
#include <LiquidCrystal.h>
#include <ArduinoJson.h>

/*
 * Set global constant attributes.
 */
static const char WIFI_SSID[] = "WiFi-2.4-19C8"; // ADJUST TO SETUP
static const char WIFI_PASSWORD[] = "4aMp69QnkQ1W"; // ADJUST TO SETUP
static const String OPENAM_HOST = "openam.example.com:8080"; // ADJUST TO SETUP
static const IPAddress OPENAM_SERVER(192,168,1,5); // ADJUST TO SETUP
static const int OPENAM_PORT = 8080; // ADJUST TO SETUP
static const char OAUTH_CLIENT_ID[] = "client"; // ADJUST TO SETUP
static const char OAUTH_CLIENT_SECRET[] = "forgerock"; // ADJUST TO SETUP
static const int SERIAL_PORT = 9600;
static const int NO_LED = -1;
static const int RED_LED = 0;
static const int GREEN_LED = 1;
static const int LCD_WIDTH = 16;
static const int LCD_HEIGTH = 2;
static const int ERROR_STATE = -1;
static const int INITIAL_STATE = 0;
static const int WAIT_STATE = 1;
static const int END_STATE = 2;

/*
 * Set global variable attributes.
 */
static int CURRENT_STATE = INITIAL_STATE;
static String OAUTH_USER_CODE;
static String OAUTH_DEVICE_CODE;
static int OAUTH_INTERVAL;
static int OAUTH_EXPIRES_IN;
static String OAUTH_VERIFICATION_URL;
unsigned long OAUTH_RECEIVED;
static String OAUTH_ACCESS_TOKEN;

/*
 * Initialize WiFi client and LCD display.
 */
WiFiClient WIFI_CLIENT;
LiquidCrystal LCD(12, 11, 5, 4, 3, 2);

/*
 * Setup demo (called once initially).
 */
void setup() {
  
  /*
   * Initialize serial port and wait to open.
   */
  Serial.begin(SERIAL_PORT);
  while (!Serial) delay(1000);
  Serial.println("Serial intialized.");

  /*
   * Initialize board.
   */
  LCD.begin(LCD_WIDTH, LCD_HEIGTH);
  LCD.setCursor(0,0);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  /*
   * Connect to Wifi (if fails wait 1 minute).
   */
  while (!connect()) {

    Serial.println("Can't connect to WiFi network! >> sleeping 60 seconds");
    
    delay(60000);
    
  }
  
}

/*
 * Connect to WiFi network. 
 */
boolean connect() {
  
  /*
   * Check for WiFi hardware.
   */
  if (WiFi.status() == WL_NO_SHIELD) {

    output("No WiFi hardware found!", RED_LED);
    
    return false;
    
  }

  /*
   * Connect to WiFi network.
   */
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {

    output("Connecting...   " + String(WIFI_SSID), RED_LED);

    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("Waiting for connection to WiFi network! >> sleeping 5 seconds");

    /*
     * Wait 5 seconds.
     */
    delay(5000);
  
  }

  output("Connected       " + String(WIFI_SSID), NO_LED);

  return true;

}

/*
 * Loop demo (called continously after setup).
 */
void loop() {

/*
 * Continue based on current state.
 */
  switch (CURRENT_STATE) {

    case INITIAL_STATE:
      
      executeInitialState();
      Serial.println("Waiting for next loop! >> sleeping 5 seconds");
      delay(5000);
      
      break;
    case WAIT_STATE:
      
      executeWaitState();
      Serial.println("Waiting for next loop! >> sleeping 5 seconds");
      delay(5000);
      
      break;
    case END_STATE:
    
      executeEndState();
      Serial.println("Waiting for next loop! >> sleeping 1 minute");
      delay(60000);
      
      break;
    default:
      //executeErrorState();
      Serial.println("ERROR");
      break;
    
  }

}

/*
 * Execute initial state.
 */
void executeInitialState() {

  Serial.println("Intial state started.");

  /*
   * Stop all connections.
   */
  WIFI_CLIENT.stop();

  /*
   * Connect to OpenAM server.
   */
  while (!WIFI_CLIENT.connect(OPENAM_SERVER, OPENAM_PORT)) {

    Serial.println("Waiting for connection to OpenAM! >> sleeping 5 seconds");
    
    delay(5000);
  
  }

  /*
   * Send OAuth device code request.
   */
  String data = "response_type=token&scope=cn%20mail&client_id=" + String(OAUTH_CLIENT_ID);
  
  WIFI_CLIENT.println("POST /openam/oauth2/device/code HTTP/1.1");
  WIFI_CLIENT.println("Host: " + OPENAM_HOST);
  WIFI_CLIENT.println("User-Agent: ArduinoWiFi/1.1");
  WIFI_CLIENT.println("Content-Type: application/x-www-form-urlencoded");
  WIFI_CLIENT.println("Content-Length: " + String(data.length()));
  WIFI_CLIENT.println("");
  WIFI_CLIENT.println(data);
  WIFI_CLIENT.println("Connection: close");
  WIFI_CLIENT.println();

 /*
  * Wait for response.
  */
  while (!WIFI_CLIENT.available()) {

    Serial.println("Waiting for response from OpenAM! >> sleeping 5 seconds");
    
    delay(5000);
  
  }

  /*
   * Parse response.
   */
  char responseBuffer[250];
  int i = 0;
  
  while (WIFI_CLIENT.available() && i < 249) {
    char c = WIFI_CLIENT.read();
    if (i > 0 || c == '{') {
      responseBuffer[i] = c;
      Serial.write(c);
      i++;
    }
  }
  WIFI_CLIENT.flush();

  StaticJsonBuffer<250> jsonBuffer;
  JsonObject& response = jsonBuffer.parseObject(responseBuffer);

  /*
   * Get OAuth device code.
   */
  String user_code = response["user_code"];
  String device_code = response["device_code"];
  int interval = response["interval"];
  int expires_in = response["expires_in"];
  String verification_url = response["verification_url"];

  OAUTH_USER_CODE = user_code;
  OAUTH_DEVICE_CODE = device_code;
  OAUTH_INTERVAL = interval;
  OAUTH_EXPIRES_IN = expires_in;
  OAUTH_VERIFICATION_URL = verification_url;
  OAUTH_RECEIVED = millis();

  /*
   * Set state.
   */
   CURRENT_STATE = WAIT_STATE;
  
}

/*
 * Execute wait state.
 */
void executeWaitState() {

  Serial.println("Wait state started.");

  /*
   * Check wether code has expired.
   */
  if ((millis() - OAUTH_RECEIVED) > (OAUTH_EXPIRES_IN * 1000L)) {

    /*
     * Set state.
     */
     CURRENT_STATE = INITIAL_STATE;

     return;
    
  }

  /*
   * Display code and verification URL.
   */
  for (int i = 0; i <= (OAUTH_VERIFICATION_URL.length() - 16); i++) {

    output("Code: " + OAUTH_USER_CODE + "  " + OAUTH_VERIFICATION_URL.substring(i,i+16), NO_LED);

    /*
     * Sleep 500 milliseconds.
     */
    delay(500);
    
  }

  /*
   * Stop all connections.
   */
  WIFI_CLIENT.stop();

  /*
   * Connect to OpenAM server.
   */
  while (!WIFI_CLIENT.connect(OPENAM_SERVER, OPENAM_PORT)) {

    Serial.println("Waiting for connection to OpenAM! >> sleeping 5 seconds");
    
    delay(5000);
  
  }

  /*
   * Check OAuth device code validation.
   */
  String data =
    "client_id=" + String(OAUTH_CLIENT_ID) +
    "&client_secret=" + String(OAUTH_CLIENT_SECRET) +
    "&code=" + OAUTH_DEVICE_CODE +
    "&grant_type=http%3A%2F%2Foauth.net%2Fgrant_type%2Fdevice%2F1.0";
   
  WIFI_CLIENT.println("POST /openam/oauth2/access_token HTTP/1.1");
  WIFI_CLIENT.println("Host: " + OPENAM_HOST);
  WIFI_CLIENT.println("User-Agent: ArduinoWiFi/1.1");
  WIFI_CLIENT.println("Content-Type: application/x-www-form-urlencoded");
  WIFI_CLIENT.println("Content-Length: " + String(data.length()));
  WIFI_CLIENT.println("");
  WIFI_CLIENT.println(data);
  WIFI_CLIENT.println("Connection: close");
  WIFI_CLIENT.println();

 /*
  * Wait for response.
  */
  while (!WIFI_CLIENT.available()) {

    Serial.println("Waiting for response from OpenAM! >> sleeping 5 seconds");
    
    delay(5000);
  
  }

  /*
   * Parse response.
   */
  char responseBuffer[250];
  int i = 0;
  
  while (WIFI_CLIENT.available() && i < 250) {
    char c = WIFI_CLIENT.read();
    if (i > 0 || c == '{') {
      responseBuffer[i] = c;
      Serial.write(c);
      i++;
    }
  }
  WIFI_CLIENT.flush();

  StaticJsonBuffer<250> jsonBuffer;
  JsonObject& response = jsonBuffer.parseObject(responseBuffer);

  String access_token = response["access_token"];

  if (access_token.length() > 0) {

    OAUTH_ACCESS_TOKEN = access_token;

    /*
     * States state.
     */
    CURRENT_STATE = END_STATE;
    
  }
  
}

/*
 * Execute end state.
 */
void executeEndState() {

  Serial.println("End state started.");

  /*
   * Stop all connections.
   */
  WIFI_CLIENT.stop();

  /*
   * Connect to OpenAM server.
   */
  while (!WIFI_CLIENT.connect(OPENAM_SERVER, OPENAM_PORT)) {

    Serial.println("Waiting for connection to OpenAM! >> sleeping 5 seconds");
    
    delay(5000);
  
  }

  /*
   * Get OAuth token information.
   */   
  WIFI_CLIENT.println("GET /openam/oauth2/tokeninfo?access_token=" + OAUTH_ACCESS_TOKEN + " HTTP/1.1");
  WIFI_CLIENT.println("Host: " + OPENAM_HOST);
  WIFI_CLIENT.println("User-Agent: ArduinoWiFi/1.1");
  WIFI_CLIENT.println("");
  WIFI_CLIENT.println("Connection: close");
  WIFI_CLIENT.println();

 /*
  * Wait for response.
  */
  while (!WIFI_CLIENT.available()) {

    Serial.println("Waiting for response from OpenAM! >> sleeping 5 seconds");
    
    delay(5000);
  
  }

  /*
   * Parse response.
   */
  char responseBuffer[250];
  int i = 0;
  
  while (WIFI_CLIENT.available() && i < 250) {
    char c = WIFI_CLIENT.read();
    if (i > 0 || c == '{') {
      responseBuffer[i] = c;
      Serial.write(c);
      i++;
    }
  }
  WIFI_CLIENT.flush();

  StaticJsonBuffer<250> jsonBuffer;
  JsonObject& response = jsonBuffer.parseObject(responseBuffer);

  String mail = response["mail"];

  output("User Connected  " + mail, GREEN_LED);
  
}

/*
 * Output message on LCD and activate led.
 */
void output(String message, int led) {

  LCD.clear();

  if (message.length() > LCD_WIDTH) {

    LCD.print(message.substring(0,LCD_WIDTH));

    for (int i = 1; (i < LCD_HEIGTH) && (message.length() > LCD_WIDTH*i); i++) {
    
      LCD.setCursor(0,i);
      LCD.print(message.substring(LCD_WIDTH*i, LCD_WIDTH*(i+1)));

    }
    
  } else {

    LCD.print(message);
    
  }

  if (led == RED_LED) {

    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
    
  } else if (led == GREEN_LED) {

    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    
  } else {

    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    
  }

}
