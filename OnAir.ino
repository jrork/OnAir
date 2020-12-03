// On Air Display control
// Light up the On Air sign using a strip of 10 WS2812s
// Button controls if sign is on or off
// Webpage also available to control sign

#include <Adafruit_NeoPixel.h>  // For controling the Light Strip
#include <WiFiManager.h>        // For managing the Wifi Connection by TZAPU
#include <ESP8266WiFi.h>        // For running the Web Server
#include <ESP8266WebServer.h>   // For running the Web Server
#include <ESP8266mDNS.h>        // For running OTA and Web Server
#include <WiFiUdp.h>            // For running OTA
#include <ArduinoOTA.h>         // For running OTA
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson


// Digital IO pin connected to the button. This will be driven with a
// pull-up resistor so the switch pulls the pin to ground momentarily.
// On a high -> low transition the button press logic will execute.
//#define BUTTON_PIN   4 //D3

#define PIXEL_PIN    2 //

#define PIXEL_COUNT 100  // Number of NeoPixels

// Device Info
const char* devicename = "LilED";
const char* devicepassword = "onairadmin";

// Declare NeoPixel strip object:
// Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, NEO_BRG + NEO_KHZ400);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

#include <Ticker.h>
Ticker ticker;
boolean ledState = LOW;   // Used for blinking LEDs when WifiManager in Connecting and Configuring

// State of the light and it's color
boolean lightOn = false;
uint8_t lightBrightness = 255;

uint32_t colorList[] =  {
  strip.Color(255,   0,   0),
  strip.Color(  0, 255,   0),
  strip.Color(  0,   0, 255),
  strip.Color(  0,   0,   0)    // Last one is to hold color set by web
};
int MAX_COLORS = sizeof(colorList) / sizeof(colorList[0]);
int maxColors = MAX_COLORS - 1; // default to not showing the last color unless set by web
int currentColor = 0;


// For Web Server
ESP8266WebServer server(80);

// Main Page
static const char MAIN_PAGE[] PROGMEM = R"====(
<HTML>
<HEAD>
<link rel="icon" href="data:,">
<SCRIPT>

var light_on = false;
var light_color = '#000000';
var light_brightness = 255;

//
// Print an Error message
//
function displayError (errorMessage) {
  document.getElementById('errors').style.visibility = 'visible';
  document.getElementById('errors').innerHTML = document.getElementById('errors').innerHTML + errorMessage + '<BR>';
  
}

//
// Print a Debug message
//
function displayDebug (debugMessage) {
  document.getElementById('debug').style.visibility = 'visible';
  document.getElementById('debug').innerHTML = document.getElementById('debug').innerHTML + debugMessage + '<BR>';
  
}


//
// Function to make a REST call
//
function restCall(httpMethod, url, cFunction, bodyText=null) {
  contentType = 'text/plain';
  if (httpMethod == 'POST') {
    contentType = 'application/json';
  }
  fetch (url, {
    method: httpMethod,
    headers: {
      'Content-Type': contentType
    },
    body: bodyText,
  })
  .then (response => {
    // Check Response Status
    if (!response.ok) {
      throw new Error('Error response: ' + response.status + ' ' + response.statusText);
    }
    return response;
  })
  .then (response => {
    // process JSON response
    const contentType = response.headers.get('content-type');
    if (!contentType || !contentType.includes('application/json')) {
      throw new TypeError("No JSON returned!");
    }
    return response.json();
  })
  .then (jsonData => {
    // Send JSON to callback function if present
    if (cFunction != undefined) {
      displayDebug(JSON.stringify(jsonData));
      cFunction(jsonData);
    }
  })
  .catch((err) => {
    displayError(err.message);
  });
}


//
// Handling displaying the current status
//
function statusLoaded (jsonResponse) {
  light_on = jsonResponse.lightOn;
  light_color = jsonResponse.color;
  light_brightness = jsonResponse.brightnessValue;
  document.getElementById('light_color').value = light_color;
  document.getElementById('brightness_state').value = 'Howdy!';

  if (light_on) {
    document.getElementById('light_state').innerHTML = 'ON';
    document.getElementById('light_button').value = 'Turn Light OFF';
    document.getElementById('state').style.color = light_color;
  }
  else {
    document.getElementById('light_state').innerHTML = 'OFF';
    document.getElementById('light_button').value = 'Turn Light ON';
    document.getElementById('state').style.color = '#000000';
  }

  next_light_color = jsonResponse.nextColor;
  document.getElementById('set_next_light_color').style.borderColor = next_light_color;
  prev_light_color = jsonResponse.prevColor;
  document.getElementById('set_prev_light_color').style.borderColor = prev_light_color;
}


//
// Turn the Light on or off
//
function changeLight() {
  if (light_on) {
    // Light is on -> turn it off
    restCall('DELETE', '/light', statusLoaded);
  }
  else {
    // Light is off -> turn it on
    restCall('PUT', '/light', statusLoaded);
  }
}

//
// Set the Next color of the light
//
function setNextLightColor() {
  restCall('PUT', '/light?next', statusLoaded);
}

//
// Set the Prev color of the light
//
function setPrevLightColor() {
  restCall('PUT', '/light?prev', statusLoaded);
}

//
// Increase the light brightness
//
function increaseBrightness() {
  restCall('PUT', '/brightness?up', statusLoaded);
}

//
// Decrease the light brightness
//
function decreaseBrightness() {
  restCall('PUT', '/brightness?down', statusLoaded);
}
//
// Set the color of the light
//
function setLightColor() {
  var postObj = new Object();
  postObj.color = document.getElementById('light_color').value;
  restCall('POST', '/light', statusLoaded, JSON.stringify(postObj));
}

//
// Turns on rainbow effect
//
function setRainbow() {
  restCall('PUT', '/light?rainbow', statusLoaded);
}

//
// Turns on Christmas effect
//
function setChristmas() {
  restCall('PUT', '/light?christmas', statusLoaded);
}

//
// actions to perform when the page is loaded
//
function doOnLoad() {
  restCall('GET', '/light', statusLoaded);
}

//
// Set the brightness of the strip
//
function setBrightness(brightVal) {
  var postObj = new Object();
  postObj.brightnessValue = brightVal;
  restCall('POST', '/brightness', statusLoaded, JSON.stringify(postObj));
}


</SCRIPT>
</HEAD>
<BODY style='max-width: 960px; margin: auto;' onload='doOnLoad();'>
<CENTER><H1>Welcome to Lil's LED Bulletin Board Management Page</H1></CENTER>
<BR>
<BR>
<DIV style='position: relative; width: 500px; height: 200px; margin: auto; background-color: #000000; outline-style: solid; outline-color: light_color; outline-width: 10px;'>
  <DIV style='position: absolute; top: 50%; -ms-transform: translateY(50%); transform: translateY(-50%); width: 100%; text-align: center; background-color: #000000; font-size: 8vw; font-weight: bold;' id='state'>Light is On!</DIV>
</DIV> 
<BR>
<BR>
Light is currently <span id='light_state'></span><BR>
Brightness is currently set to <span id='brightness_state'></span><BR>
<HR style='margin-top: 20px; margin-bottom: 10px;'>
<form>
<DIV style='overflow: hidden; margin-top: 10px; margin-bottom: 10px;'>
  <DIV style='text-align: center; float: left;'>
    <label for='light_button'>Change Light:</label><BR>
    <input type='button' id='light_button' name='light_state' style='width: 160px; height: 40px; margin-bottom: 10px;' onClick='changeLight();'><BR>
    <input type='button' id='set_prev_light_color' name='set_prev_light_color' style='width: 80px; height: 40px; border-style: solid; border-width: 5px; border-radius: 10px;' value='<-- Prev' onClick='setPrevLightColor();'>
    <input type='button' id='set_next_light_color' name='set_next_light_color' style='width: 80px; height: 40px; border-style: solid; border-width: 5px; border-radius: 10px;' value='Next -->' onClick='setNextLightColor();'>
  </DIV>
  <DIV style='text-align: center; overflow: hidden;'>
    <label for='light_color'>New Light Color:</label><BR>
    <input type='color' id='light_color' name='light_color' style='width: 120px; height: 40px; margin-bottom: 10px;'><BR>
    <input type='button' id='set_light_color' name='set_light_color' style='width: 120px; height: 40px;' value='Set Color' onClick='setLightColor();'><BR>
  </DIV>
  <DIV style='text-align: center; overflow: hidden;'>
    <label for='rainbow'>Rainbow</label><BR>
    <input type='button' id='set_rainbow' name='set_rainbow' style='width: 120px; height: 40px;' value='Rainbow' onClick='setRainbow();'><BR>
  </DIV>
  <DIV style='text-align: center; overflow: hidden;'>
    <label for='christmas'>Christmas</label><BR>
    <input type='button' id='set_christmas' name='set_christmas' style='width: 120px; height: 40px;' value='Christmas' onClick='setChristmas();'><BR>
  </DIV>
  <DIV style='text-align: center; overflow: hidden;'>
    <label for='brightness'>Brightness</label><BR>
    <input id="slide" type="range" min="1" max="254" step="1" value="254"><BR>
    <input type='button' id='increase_brightness' name='increase_brightness' style='width: 120px; height: 40px;' value='Brightness+' onClick='increaseBrightness();'><BR>
    <input type='button' id='decrease_brightness' name='decrease_brightness' style='width: 120px; height: 40px;' value='Brightness-' onClick='decreaseBrightness();'><BR>

  </DIV>
</DIV>
</form>
<HR style='margin-top: 10px; margin-bottom: 10px;'>
<DIV id='debug' style='font-family: monospace; color:blue; outline-style: solid; outline-color:blue; outline-width: 2px; visibility: hidden; padding-top: 10px; padding-bottom: 10px; margin-top: 10px; margin-bottom: 10px;'></DIV><BR>
<DIV id='errors' style='color:red; outline-style: solid; outline-color:red; outline-width: 2px; visibility: hidden; padding-top: 10px; padding-bottom: 10px; margin-top: 10px; margin-bottom: 10px;'></DIV><BR>
</BODY>
<SCRIPT>

//Slider for brightness
var slide = document.getElementById('slide')

slide.onchange = function() { 
    setBrightness(this.value);   
}
</SCRIPT>
</HTML>
)====";


/*************************************************
 * Setup
 *************************************************/
void setup() {
  Serial.begin(115200);

  strip.begin(); // Initialize NeoPixel strip object (REQUIRED)
  strip.show();  // Initialize all pixels to 'off'
  ticker.attach(0.6, tick); // start ticker to slow blink LED strip during Setup


  //
  // Set up the Wifi Connection
  //
  WiFi.hostname(devicename);
  WiFi.mode(WIFI_STA);      // explicitly set mode, esp defaults to STA+AP
  WiFiManager wm;
  // wm.resetSettings();    // reset settings - for testing

  // Set static IP to see if it fixes my problem - joe
  IPAddress _ip = IPAddress(192, 168, 1, 19);
  IPAddress _gw = IPAddress(192, 168, 1, 1);
  IPAddress _sn = IPAddress(255, 255, 255, 0);
  wm.setSTAStaticIPConfig(_ip, _gw, _sn);
  
  wm.setAPCallback(configModeCallback); //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  //if it does not connect it starts an access point with the specified name here  "AutoConnectAP"
  if (!wm.autoConnect()) {
    //Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
  //Serial.println("connected");


  //
  // Set up the Multicast DNS
  //
  MDNS.begin(devicename);


  //
  // Set up OTA
  //
  // ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(devicename);
  ArduinoOTA.setPassword(devicepassword);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    //Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      //Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      //Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      //Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      //Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      //Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();


  //
  // Setup Web Server
  //
  server.on("/", handleRoot);
  server.on("/light", handleLight);
  server.on("/brightness", handleBrightness);
  server.onNotFound(handleNotFound);
  server.begin();
  //Serial.println("HTTP server started");


  //
  // Done with Setup
  //
  ticker.detach();          // Stop blinking the LED strip
  colorSet(strip.Color(  0, 255,   0)); // Use Green to indicate the setup is done.
  delay(2000);
  turnLightOff();
}


/*************************************************
 * Loop
 *************************************************/
void loop() {
  // Handle any requests
  ArduinoOTA.handle();
  server.handleClient();
  MDNS.update();

}


/******************************
 * Callback Utilities during setup
 ******************************/

/*
 * Blink the LED Strip.
 * If on  then turn off
 * If off then turn on
 */
void tick()
{
  //toggle state
  ledState = !ledState;
  if (ledState) {
    colorSet(strip.Color(255, 255, 255));
  }
  else {
    colorSet(strip.Color(  0,   0,   0));
  }
}

/*
 * gets called when WiFiManager enters configuration mode
 */
void configModeCallback (WiFiManager *myWiFiManager) {
  //Serial.println("Entered config mode");
  //Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  //Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}


/*************************************
 * Light management functions
 *************************************/

/*
 * Turn On the next Color in the rotation
 */
void turnNextLightOn() {
  turnLightOn(currentColor+10);
}

/*
 * Turn On the previous Color in the rotation
 */
void turnPrevLightOn() {
  turnLightOn(currentColor-10);
}


/*
 * Turn the Light on to the color specified
 */
void turnLightOn() {
  turnLightOn(currentColor);
}
void turnLightOn(int colorNum) {
  if(colorNum >= maxColors) colorNum = 0; // If out of range, wrap around after max
  if(colorNum < 0) colorNum = maxColors - 1; // If out of range, wrap around to max
  currentColor = colorNum;
  lightOn = true;
  //colorWipe(colorList[currentColor],10);
  colorSet(colorList[currentColor]);
}


/*
 * Turn the Light off
 */
void turnLightOff() {
  lightOn = false;
  colorSet(strip.Color(  0,   0,   0));    // Black/off
}

void setBrightnessValue(uint8_t bright_value) {
  lightBrightness = bright_value;
  strip.setBrightness(bright_value%255);  //valid brightness values are 0<->255
  strip.show();
}

/*
 * Turn On the next Color in the rotation
 */
void increaseBrightness() {
  setBrightnessValue(++lightBrightness);
}

/*
 * Turn On the previous Color in the rotation
 */
void decreaseBrightness() {
  setBrightnessValue(--lightBrightness);
}

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}

// Fill strip pixels at once. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above)
void colorSet(uint32_t color) {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
  }
  strip.show();                          //  Update strip to match
}


// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(int wait) {
  // Hue of first pixel runs 3 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 3*65536. Adding 256 to firstPixelHue each time
  // means we'll make 3*65536/256 = 768 passes through this outer loop:
  for(long firstPixelHue = 0; firstPixelHue < 3*65536; firstPixelHue += 256) {
    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}

// Alternate each pixel between green and red.
void christmas() {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    if(i%2) {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    } else {
      strip.setPixelColor(i, strip.Color(0, 255, 0));
    }
  }
    strip.show(); // Update strip with new contents
}

/******************************************
 * Web Server Functions
 ******************************************/

//
// Handle a request for the root page
//
void handleRoot() {
  server.send_P(200, "text/html", MAIN_PAGE, sizeof(MAIN_PAGE));
}

//
// Handle service for Light
//
void handleLight() {
  switch (server.method()) {
    case HTTP_POST:
      if (setLightColor()) {
        sendStatus();
      }
      break;
    case HTTP_PUT:
      if (server.hasArg("next")) {
        turnNextLightOn();
      }
      else if (server.hasArg("prev")) {
        turnPrevLightOn();
      }
      else if (server.hasArg("rainbow")) {
        rainbow(100);
      }
      else if (server.hasArg("christmas")) {
        christmas();
      }      
      else {
        turnLightOn();
      }
      sendStatus();
      break;
    case HTTP_DELETE:
      turnLightOff();
      sendStatus();
      break;
    case HTTP_GET:
      sendStatus();
      break;
    default:
      server.send(405, "text/plain", "Method Not Allowed");
      break;
  }
}


//
// Handle service for Light
//
void handleBrightness() {
  switch (server.method()) {
    case HTTP_POST:
      if (setBrightness()) {
        sendStatus();
      }
      break;
    case HTTP_PUT:
      if (server.hasArg("up")) {
        increaseBrightness();
      }
      else if (server.hasArg("down")) {
        decreaseBrightness();
      }     
      else {
      }
      sendStatus();
      break;
    case HTTP_GET:
      sendStatus();
      break;
    default:
      server.send(405, "text/plain", "Method Not Allowed");
      break;
  }
}
  
//
// Handle returning the status of the sign
//
void sendStatus() {
  DynamicJsonDocument jsonDoc(1024);

  // Send back current state of Light
  jsonDoc["lightOn"] = lightOn;
  jsonDoc["lightBrightness"] = lightBrightness;

  // Send back current state of Color
  //uint32_t pixelColor = colorList[currentColor] & 0xFFFFFF; // remove any extra settings - only want RGB
  //String pixelColorStr = "#000000" + String(pixelColor,HEX);
  //pixelColorStr.setCharAt(pixelColorStr.length()-7, '#');
  //pixelColorStr.remove(0,pixelColorStr.length()-7);
  String currColorStr = "";
  color2String(&currColorStr, currentColor);
  jsonDoc["color"] = currColorStr;

  int nextColor = currentColor + 1;
  if(nextColor >= maxColors) nextColor = 0; // If out of range, wrap around after max
  String nextColorStr = "";
  color2String(&nextColorStr, nextColor);
  jsonDoc["nextColor"] = nextColorStr;

  int prevColor = currentColor - 1;
  if(prevColor < 0) prevColor = maxColors - 1; // If out of range, wrap around to max
  String prevColorStr = "";
  color2String(&prevColorStr, prevColor);
  jsonDoc["prevColor"] = prevColorStr;
  
  jsonDoc["currentColor"] = currentColor;
  jsonDoc["maxColors"] = maxColors;
  jsonDoc["MAX_COLORS"] = MAX_COLORS;

  String payload;
  serializeJson(jsonDoc, payload);
  server.send(200, "application/json", payload);
}

//
// Handle setting a new color for the sign
//
boolean setLightColor() {
  if ((!server.hasArg("plain")) || (server.arg("plain").length() == 0)) {
    server.send(400, "text/plain", "Bad Request - Missing Body");
    return false;
  }
  StaticJsonDocument<200> requestDoc;
  DeserializationError error = deserializeJson(requestDoc, server.arg("plain"));
  if (error) {
    server.send(400, "text/plain", "Bad Request - Parsing JSON Body Failed");
    return false;
  }
  if (!requestDoc.containsKey("color")) {
    server.send(400, "text/plain", "Bad Request - Missing Color Argument");
    return false;
  }
  String colorStr = requestDoc["color"];
  if (colorStr.charAt(0) == '#') {
    colorStr.setCharAt(0, '0');
  }
  char color_c[10] = "";
  colorStr.toCharArray(color_c, 8);
  uint32_t color = strtol(color_c, NULL, 16);
  maxColors = MAX_COLORS;
  colorList[maxColors-1] = color;
  turnLightOn(maxColors-1);
  return true;
}

//
// Handle setting a new color for the sign
//
boolean setBrightness() {
  if ((!server.hasArg("plain")) || (server.arg("plain").length() == 0)) {
    server.send(400, "text/plain", "Bad Request - Missing Body");
    return false;
  }
  StaticJsonDocument<200> requestDoc;
  DeserializationError error = deserializeJson(requestDoc, server.arg("plain"));
  if (error) {
    server.send(400, "text/plain", "Bad Request - Parsing JSON Body Failed");
    return false;
  }
  if (!requestDoc.containsKey("brightnessValue")) {
    server.send(400, "text/plain", "Bad Request - Missing Color Argument");
    return false;
  }
  uint8_t brightnessValue = requestDoc["brightnessValue"];
  setBrightnessValue(brightnessValue);
//  String colorStr = requestDoc["color"];
//  if (colorStr.charAt(0) == '#') {
//    colorStr.setCharAt(0, '0');
//  }
//  char color_c[10] = "";
//  colorStr.toCharArray(color_c, 8);
//  uint32_t color = strtol(color_c, NULL, 16);
//  maxColors = MAX_COLORS;
//  colorList[maxColors-1] = color;
//  turnLightOn(maxColors-1);
  return true;
}

//
// Display a Not Found page
//
void handleNotFound() {
  //digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  //digitalWrite(led, 0);
}


//
// Convert uint32_t color to web #RRGGBB string
//
void color2String (String* colorString, int colorNum) {
  uint32_t pixelColor = colorList[colorNum] & 0xFFFFFF; // remove any extra settings - only want RGB
  colorString->concat("#000000" + String(pixelColor,HEX));
  colorString->setCharAt(colorString->length()-7, '#');
  colorString->remove(0,colorString->length()-7);
}
