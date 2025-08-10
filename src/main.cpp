#include <ElegantOTA.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config_manager.h"
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include "esp_wifi.h" // for esp_wifi_set_ps()

#define SDA_PIN 21
#define SCL_PIN 22
#define LED_PIN 13
#define NUM_PIXELS 24
#define BUZZER_PIN 5   // or GPIO14
#define BATTERY_PIN 36 // Use GPIO36 / ADC1_CH0

Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);
Adafruit_NeoPixel pixels(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

DeviceConfig deviceConfig;

WebServer server(80); // ✅ Synchronous server

bool effectActive = false;
unsigned long effectStartTime = 0;
uint32_t currentColor = 0;
String lastCardUID = "";
String lastCard = "";

bool timeReady = false;
unsigned long ntpStartTime = 0;
const unsigned long ntpTimeout = 10000; // 10 seconds

unsigned long lastAPCheck = 0;
const unsigned long AP_CHECK_INTERVAL = 10000; // 10 seconds

float readBatteryVoltage()
{
  int raw = analogRead(BATTERY_PIN);
  float v = (raw / 4095.0) * 3.3; // measured at ADC pin
  const float divider = 2.0;      // update if resistor values imply different ratio
  return v * divider;
}

int batteryPercentage(float voltage)
{
  if (voltage >= 4.2)
    return 100;
  else if (voltage <= 3.0)
    return 0;
  else
    return (int)(((voltage - 3.0) / (4.2 - 3.0)) * 100);
}

void beep(int duration = 50)
{
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

uint32_t parseHexColor(const String &hexColor)
{
  if (hexColor.length() < 3)
    return pixels.Color(0, 0, 0); // Default black

  String hex = hexColor;
  if (hex.charAt(0) == '#')
    hex = hex.substring(1);

  // Handle 3-digit hex (#RGB -> RRGGBB)
  if (hex.length() == 3)
  {
    hex = String(hex[0]) + hex[0] + hex[1] + hex[1] + hex[2] + hex[2];
  }

  // Validate hex
  for (unsigned int i = 0; i < hex.length(); i++)
  {
    if (!isxdigit(hex[i]))
      return pixels.Color(0, 0, 0);
  }

  long number = strtol(hex.c_str(), NULL, 16);
  byte r = (number >> 16) & 0xFF;
  byte g = (number >> 8) & 0xFF;
  byte b = number & 0xFF;
  return pixels.Color(r, g, b);
}

String loadConfigAsString()
{
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile)
    return "{}";
  String content = configFile.readString();
  configFile.close();
  return content;
}

bool saveConfigFromString(const String &jsonString)
{
  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile)
    return false;
  configFile.print(jsonString);
  configFile.close();
  return true;
}

void logActivity(const String &uid, const String &status)
{
  File f = LittleFS.open("/activities.log", "a");
  if (!f)
  {
    Serial.println("❌ Failed to open activities.log for writing");
    return;
  }

  char ts[32];
  if (timeReady)
  {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
      strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &timeinfo);
    }
    else
    {
      strcpy(ts, "unknown");
    }
  }
  else
  {
    strcpy(ts, "unknown");
  }

  // One compact JSON object per line
  f.printf("{\"time\":\"%s\",\"uid\":\"%s\",\"status\":\"%s\"}\n",
           ts, uid.c_str(), status.c_str());

  f.flush(); // ensure data is written to flash
  f.close();
  Serial.printf("📄 Logged activity: %s %s\n", uid.c_str(), status.c_str());
}

void startSolidEffect(uint32_t color)
{
  // Ensure brightness is within safe bounds
  uint8_t brightness = constrain(deviceConfig.ledBrightness, 5, 255);
  pixels.setBrightness(brightness);

  for (int i = 0; i < NUM_PIXELS; i++)
  {
    pixels.setPixelColor(i, color);
  }
  pixels.show();
  effectStartTime = millis();
  effectActive = true;
  currentColor = color;
}

void clearLEDs()
{
  pixels.clear();
  pixels.show();
  effectActive = false;
  lastCardUID = "";
}

// Start the Access Point with static IP and stability tweaks
void startAP(const char *ssid, const char *pass)
{
  WiFi.mode(WIFI_AP_STA); // AP + STA mode

  esp_wifi_set_ps(WIFI_PS_NONE); // disable power save for better AP stability

  // Static IP for AP
  IPAddress apIP(192, 168, 4, 1);
  IPAddress gw(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gw, subnet);

  bool ok = WiFi.softAP(ssid, pass);
  if (ok)
  {
    Serial.print("SoftAP started: ");
    Serial.print(ssid);
    Serial.print("  IP: ");
    Serial.println(WiFi.softAPIP());
  }
  else
  {
    Serial.println("SoftAP start FAILED");
  }
}

// Periodic AP check to ensure it stays alive
void ensureAPAlive(const char *ssid, const char *pass)
{
  if (millis() - lastAPCheck < AP_CHECK_INTERVAL)
    return;
  lastAPCheck = millis();

  if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0))
  {
    Serial.println("AP appears down — restarting...");
    startAP(ssid, pass);
  }
}

void connectToWiFi()
{
  Serial.println("Setting up AP + STA...");
  WiFi.mode(WIFI_AP_STA);

  WiFi.begin(deviceConfig.wifi.ssid.c_str(), deviceConfig.wifi.password.c_str());

  // unsigned long startTime = millis();
  // const unsigned long timeout = 10000;
  // while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout) {
  //   Serial.print(".");
  //   delay(500);
  // }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWi-Fi STA connected!");
    Serial.print("STA IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\nFailed to connect to Wi-Fi (STA), but AP remains active.");
  }
}

void handleRoot()
{
  String html = "<html><body><h2>Edit Config</h2>";
  html += "<form method='POST' action='/save'>";
  html += "<textarea name='config' rows='30' cols='80'>";
  html += loadConfigAsString();
  html += "</textarea><br><br>";
  html += "<input type='submit' value='Save & Reboot'>";
  html += "</form></body>";
  html += "<a href='/status'>See Main Details</a>&nbsp;<a href='/'>reload page</a>&nbsp;";
  html += "<a href='/card'>card raw editor</a>&nbsp;<a href='/cards'>see all cards</a>&nbsp;<a href='/cards/manage'>Manage cards</a>&nbsp;";
  html += "<a href='/activities'>See all logs</a>&nbsp;";
  html += "<a href='/activities/delete'>Delete all logs</a>&nbsp;";
  html += "<a href='/update'>Add update</a></html>";
  server.send(200, "text/html", html);
}

void handleSave()
{
  if (!server.hasArg("config"))
  {
    server.send(400, "text/plain", "Missing config data.");
    return;
  }

  String raw = server.arg("config");
  StaticJsonDocument<2048> test;
  auto err = deserializeJson(test, raw);
  if (err)
  {
    server.send(400, "text/plain", String("Invalid JSON: ") + err.c_str());
    return;
  }
  // optionally check test["wifi"]["ssid"] etc.
  // then write file

  if (saveConfigFromString(raw))
  {
    server.send(200, "text/html", "<html><body><h3>Saved! Rebooting... <a href='/'> <back</a></h3></body></html>");
    delay(500);
    server.client().stop();
    ESP.restart();
  }
  else
  {
    server.send(500, "text/plain", "Failed to save config.");
  }
}

void handleManageUI()
{
  String html = R"rawliteral(
    <html><body>
    <h2>Add New Card</h2>
    <form onsubmit="addCard(event)">
      UID: <input name="uid" id="uid"><br>
      Color: <input name="color" id="color" value="#00FF00"><br>
      Animation: 
      <select id="animation">
        <option value="solid">Solid</option>
      </select><br>
      <input type="submit" value="Add Card">
    </form>

    <hr>
    <h2>Registered Cards</h2>
    <div id="card-list">Loading...</div>

    <script>
    async function fetchLastUID() {
      const res = await fetch('/lastuid');
      const uid = await res.text();
      document.getElementById('uid').value = uid;
    }

    async function fetchCards() {
      const res = await fetch('/cards');
      const cards = await res.json();
      let html = "<ul>";
      for (let c of cards) {
        html += `<li><b>${c.uid}</b> - ${c.color} - ${c.animation}
          <button onclick="del('${c.uid}')">Delete</button></li>`;
      }
      html += "</ul>";
      document.getElementById('card-list').innerHTML = html;
    }

    async function addCard(e) {
      e.preventDefault();
      const uid = document.getElementById('uid').value;
      const color = document.getElementById('color').value;
      const animation = document.getElementById('animation').value;

      const params = new URLSearchParams({ uid, color, animation });
      await fetch('/cards/add', { method: 'POST', body: params });
      fetchCards();
    }

    async function del(uid) {
      const params = new URLSearchParams({ uid });
      await fetch('/cards/delete', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded'
        },
        body: params.toString()
      });
      fetchCards();
    }


    fetchLastUID();
    fetchCards();
    </script>
    </body></html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleLastUID()
{
  if (lastCard == "")
  {
    server.send(200, "text/plain", "NO CARD"); // or optionally send "none"
  }
  else
  {
    server.send(200, "text/plain", lastCard);
  }
}

// Find a card in CSV file
bool loadCardColorAndAnimation(String uidStr, String &colorHex, String &animation)
{
  File file = LittleFS.open("/cards.txt", "r");
  if (!file)
    return false;

  while (file.available())
  {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    int firstComma = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    if (firstComma == -1 || secondComma == -1)
      continue;

    String fileUID = line.substring(0, firstComma);
    if (fileUID.equalsIgnoreCase(uidStr))
    {
      colorHex = line.substring(firstComma + 1, secondComma);
      animation = line.substring(secondComma + 1);
      file.close();
      return true;
    }
  }

  file.close();
  return false;
}

// List all cards as JSON for UI
void handleListCards()
{
  File file = LittleFS.open("/cards.txt", "r");
  if (!file)
  {
    server.send(200, "application/json", "[]");
    return;
  }

  String output = "[";
  bool first = true;

  while (file.available())
  {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    int firstComma = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    if (firstComma == -1 || secondComma == -1)
      continue;

    String uid = line.substring(0, firstComma);
    String color = line.substring(firstComma + 1, secondComma);
    String animation = line.substring(secondComma + 1);

    if (!first)
      output += ",";
    first = false;

    output += "{\"uid\":\"" + uid + "\",\"color\":\"" + color + "\",\"animation\":\"" + animation + "\"}";
  }

  file.close();
  output += "]";
  server.send(200, "application/json", output);
}

// Add a card to CSV
void handleAddCard()
{
  if (!server.hasArg("uid") || !server.hasArg("color") || !server.hasArg("animation"))
  {
    server.send(400, "text/plain", "Missing uid/color/animation");
    return;
  }

  String uid = server.arg("uid");
  uid.toUpperCase();

  // Open in append mode
  File file = LittleFS.open("/cards.txt", "a");
  if (!file)
  {
    server.send(500, "text/plain", "Failed to open file for writing");
    return;
  }

  // ✅ Ensure file ends with a newline before appending
  if (file.size() > 0)
  {
    file.seek(file.size() - 1, SeekSet);
    if (file.read() != '\n')
    {
      file.println(); // Add newline if missing
    }
  }

  file.println(uid + "," + server.arg("color") + "," + server.arg("animation"));
  file.close();

  server.send(200, "text/plain", "Card saved.");
}

// Delete a card from CSV
void handleDeleteCard()
{
  if (!server.hasArg("uid"))
  {
    server.send(400, "text/plain", "Missing uid");
    return;
  }

  String uid = server.arg("uid");
  uid.toUpperCase();

  File file = LittleFS.open("/cards.txt", "r");
  if (!file)
  {
    server.send(404, "text/plain", "No cards found");
    return;
  }

  String newData = "";
  while (file.available())
  {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    String fileUID = line.substring(0, line.indexOf(','));
    if (!fileUID.equalsIgnoreCase(uid))
    {
      newData += line + "\n";
    }
  }
  file.close();

  file = LittleFS.open("/cards.txt", "w");
  file.print(newData);
  file.close();

  server.send(200, "text/plain", "Card deleted.");
}

// Show bulk CSV editor
// Show bulk CSV editor (stream large file safely)
void handleCardsPage()
{
  String html = R"rawliteral(
    <html>
    <body>
    <h2>Edit Cards CSV</h2>
    <textarea id="cards" rows="30" cols="80"></textarea><br>
    <button onclick="saveFile()">Save</button>

    <script>
    async function loadCards() {
        const res = await fetch('/cards.txt');
        const text = await res.text();
        document.getElementById('cards').value = text;
    }

    async function saveFile() {
        const text = document.getElementById('cards').value;
        const formData = new URLSearchParams();
        formData.append('cards', text);

        await fetch('/card', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: formData
        });

        alert('Saved!');
        loadCards(); // ✅ Refresh textarea after saving
    }

    loadCards();
    </script>
    </body>
    </html>
    )rawliteral";

  server.send(200, "text/html", html);
}

// Save bulk CSV edits
// Save bulk CSV edits without huge RAM usage
void handleCardsSave()
{
  if (!server.hasArg("cards"))
  {
    server.send(400, "text/plain", "Missing cards content");
    return;
  }

  // Open file for writing (will overwrite old file)
  File file = LittleFS.open("/cards.txt", "w");
  if (!file)
  {
    server.send(500, "text/plain", "Failed to open cards.txt for writing");
    return;
  }

  // Stream-write in chunks to save RAM
  const String &cardsData = server.arg("cards");
  const size_t chunkSize = 512; // bytes per write
  for (size_t i = 0; i < cardsData.length(); i += chunkSize)
  {
    size_t len = min(chunkSize, cardsData.length() - i);
    file.write((const uint8_t *)cardsData.c_str() + i, len);
  }
  file.close();

  server.send(200, "text/html",
              "<html><body><h2>Saved!</h2><a href='/card'>Back</a></body></html>");
}

void handleStatus()
{
  DynamicJsonDocument doc(512);

  doc["device_name"] = deviceConfig.deviceName;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["mac_address"] = WiFi.macAddress();
  doc["wifi"] = WiFi.SSID();
  doc["wifi_strength"] = WiFi.RSSI();

  struct tm timeinfo;
  if (timeReady && getLocalTime(&timeinfo))
  {
    char timeString[64];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    doc["time"] = timeString;
  }
  else
  {
    doc["time"] = "unknown";
  }

  unsigned long uptime = millis() / 1000;
  doc["uptime_seconds"] = uptime;
  doc["uptime_hms"] = String(uptime / 3600) + "h " + String((uptime % 3600) / 60) + "m " + String(uptime % 60) + "s";

  doc["light_duration"] = deviceConfig.light.lightDuration;
  float vbat = readBatteryVoltage();
  int percent = batteryPercentage(vbat);
  doc["battery_level"] = percent; // Placeholder for now

  doc["server_address"] = deviceConfig.server.address;

  doc["free_heap"] = ESP.getFreeHeap();
  doc["flash_size"] = ESP.getFlashChipSize();

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void showReadyAnimation(){
  uint8_t brightness = constrain(deviceConfig.ledBrightness, 5, 255);
  pixels.setBrightness(brightness);

  // Spinning LED
  for (int i = 0; i < NUM_PIXELS * 2; i++)
  {
    pixels.clear();
    pixels.setPixelColor(i % NUM_PIXELS, pixels.Color(0, 150, 0)); // green spin
    pixels.show();
    delay(20);
  }

  // Solid glow at the end (optional)
  uint32_t color = parseHexColor(deviceConfig.light.knownDefaultColor);
  for (int i = 0; i < NUM_PIXELS; i++)
  {
    pixels.setPixelColor(i, color);
  }
  pixels.show();
  delay(100); // Keep it lit for a moment
  clearLEDs();
}

void handleCardFileUpload(){
  HTTPUpload &upload = server.upload();
  static File uploadFile;

  if (upload.status == UPLOAD_FILE_START)
  {
    Serial.printf("Upload Start: %s\n", upload.filename.c_str());
    uploadFile = LittleFS.open("/cards.txt", "w");
    if (!uploadFile)
    {
      Serial.println("Failed to open /cards.txt for writing");
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    // Write incoming chunk directly to file
    if (uploadFile)
    {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (uploadFile)
    {
      uploadFile.close();
      Serial.printf("Upload Complete: %s, %u bytes\n", upload.filename.c_str(), upload.totalSize);
    }
  }
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("📶 Wi-Fi connected, waiting for IP...");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("✅ IP acquired: ");
      Serial.println(WiFi.localIP());
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      Serial.println("⏳ Syncing NTP time...");
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("⚠️ Wi-Fi lost, reconnecting...");
      WiFi.reconnect();
      break;

    default:
      break;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting NFC + LED Ring...");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure it's off

  analogReadResolution(12);                       // 0..4095
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db); // extend range; calibrate later

  pixels.begin();
  pixels.clear();
  pixels.setBrightness(128);
  pixels.show();

  if (!LittleFS.begin())
  {
    Serial.println("LittleFS mount failed!");
    return;
  }

  if (!LittleFS.exists("/cards.txt"))
  {
    File f = LittleFS.open("/cards.txt", "w");
    f.close();
  }

  // Use config values if loaded, otherwise defaults
  const char *apSSID = deviceConfig.deviceName.length() > 0 ? deviceConfig.deviceName.c_str() : "JasTapBox 1";
  const char *apPASS = deviceConfig.hotspotPassword.length() > 0 ? deviceConfig.hotspotPassword.c_str() : "12345678";

  // Start AP before STA connection
  startAP(apSSID, apPASS);

  if (loadDeviceConfig(deviceConfig))
  {
    printDeviceConfig(deviceConfig);
    pixels.setBrightness(deviceConfig.ledBrightness);
    connectToWiFi();
  }

  Wire.begin(SDA_PIN, SCL_PIN);
  nfc.begin();
  if (!nfc.getFirmwareVersion())
  {
    Serial.println("PN532 not found.");
    int retries = 0;
    while (!nfc.getFirmwareVersion() && retries++ < 5)
    {
      Serial.println("PN532 not found, retrying...");
      delay(500);
    }
    if (!nfc.getFirmwareVersion())
    {
      Serial.println("PN532 not found after retries. Continuing without NFC.");
      // set a flag nfcAvailable = false and skip NFC operations
    }
  }

  ElegantOTA.begin(&server, "admin", "admin@123");
  Serial.println("HTTP server started with ElegantOTA");

  // ✅ Replace Async handlers with sync server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  Serial.println("HTTP server started");
  server.on("/lastuid", HTTP_GET, handleLastUID);
  server.on("/cards", HTTP_GET, handleListCards);
  server.on("/cards/add", HTTP_POST, handleAddCard);
  server.on("/cards/delete", HTTP_POST, handleDeleteCard);
  server.on("/cards/manage", HTTP_GET, handleManageUI);
  server.on("/card", HTTP_GET, handleCardsPage);
  server.on("/card", HTTP_POST, handleCardsSave);
  server.on("/card/upload", HTTP_POST, []()
            { server.send(200, "text/plain", "Upload complete"); }, handleCardFileUpload);
  server.on("/cards.txt", HTTP_GET, []()
            {
        if (!LittleFS.exists("/cards.txt")) {
            server.send(404, "text/plain", "File not found");
            return;
        }
        File file = LittleFS.open("/cards.txt", "r");
        server.streamFile(file, "text/plain");
        file.close(); });
  server.on("/activities", HTTP_GET, []()
            {
    File f = LittleFS.open("/activities.log", "r");
    if (!f) {
      server.send(500, "application/json", "{\"error\":\"No log file\"}");
      return;
    }

    String output = "[";
    bool first = true;

    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;
      if (!first) output += ",";
      output += line;
      first = false;
    }

    output += "]";
    f.close();

    server.send(200, "application/json", output); });

  server.on("/activities/delete", HTTP_GET, []()
            {
    LittleFS.remove("/activities.json");
    server.send(200, "text/plain", "Deleted"); });
  server.on("/status", handleStatus);
  server.begin();

  nfc.SAMConfig();

  Serial.println("Ready to read NFC cards...");

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
               {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.println("Got IP - starting NTP");
    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    ntpStartTime = millis();
    } });

  showReadyAnimation();
}

void loop()
{

  server.handleClient(); // ✅ Required for WebServer to handle requests

  // Check NTP time availability once
  if (!timeReady && millis() - ntpStartTime < ntpTimeout)
  {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
      char ts[64];
      strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &timeinfo);
      Serial.print("✅ NTP time received: ");
      Serial.println(ts);
      timeReady = true;
    }
  }

  if (!effectActive && nfc.inListPassiveTarget())
  {

    uint8_t uid[10];
    uint8_t uidLength;

    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100))
    {
      String uidStr = "";
      char buf[3];
      for (uint8_t i = 0; i < uidLength; i++)
      {
        sprintf(buf, "%02X", uid[i]); // uppercase, 2 digits
        uidStr += buf;
      }
      uidStr.toUpperCase();
      Serial.println("Card UID: " + uidStr);
      lastCard = uidStr;

      if (uidStr == lastCardUID)
        return;
      lastCardUID = uidStr;

      beep(); // make the beep sound
      String colorHex, animation;
      bool known = loadCardColorAndAnimation(uidStr, colorHex, animation);

      if (!known)
      {
        colorHex = deviceConfig.light.unknownDefaultColor;
        animation = deviceConfig.light.unknownCardAnimation;
      }

      if (animation == "solid")
      {
        uint32_t color = parseHexColor(colorHex);
        startSolidEffect(color);
      }

      logActivity(uidStr, known ? "allowed" : "unknown");
    }
    delay(10);
  }

  if (effectActive)
  {
    unsigned long now = millis();
    if (now - effectStartTime >= deviceConfig.light.lightDuration)
    {
      clearLEDs();
    }
  }
}
