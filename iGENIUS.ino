#include <SPI.h>
#include <SD.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>

#include <DHT.h>

#define RED_LED 0
#define GREEN_LED 4
#define BLUE_LED 5

#define DHT_DATA_PIN 2
#define DHT_TYPE DHT22

ESP8266WebServer server(3030);
WebSocketsServer webSocket = WebSocketsServer(3131);

//WiFiClient client1;
WiFiClient client2;

File uploadFile;

String xmlString;

DHT dht(DHT_DATA_PIN, DHT_TYPE);

const char HOST_NAME[] = "nodemcu";

const char SSID_SERVER[] = "TECHNOLOGIES";
const char PASS_SERVER[] = "SERVER_PASSWORD";

const char SSID_CLIENT[] = "DECADE";
const char PASS_CLIENT[] = "CLIENT_PASSWORD";

const char THING_SPEAK_URL[] = "api.thingspeak.com";
//const byte THING_SPEAK_URL[]  = {184, 106, 153, 149};

const int NODEMCU_CHANNEL = 12345678;

const int HOLD_ON = 500;
long previousMillis = 0;
const long INTERVAL = 1800000;

static float humidity;
static float temperature;

static int roundPlayer;

static boolean hasSD = false;

void loading() {
  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, LOW);

  delay(HOLD_ON);

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(BLUE_LED, LOW);

  delay(HOLD_ON);

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, HIGH);

  delay(HOLD_ON);
}

void setFlashLight(byte redLed, byte greenLed, byte blueLed) {
  digitalWrite(RED_LED, (redLed > 0) ? HIGH:LOW);
  digitalWrite(GREEN_LED, (greenLed > 0) ? HIGH:LOW);
  digitalWrite(BLUE_LED, (blueLed > 0) ? HIGH:LOW);

  analogWrite(RED_LED, redLed);
  analogWrite(GREEN_LED, greenLed);
  analogWrite(BLUE_LED, blueLed);

  delay(HOLD_ON);

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, LOW);

  analogWrite(RED_LED, 0);
  analogWrite(GREEN_LED, 0);
  analogWrite(BLUE_LED, 0);

  delay(HOLD_ON);
}

boolean dht22() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    return false;
  }

  return true;
}

void thingSpeak(int channel) {
  if (client2.connect(THING_SPEAK_URL,80)) {
    switch (channel) {
      case NODEMCU_CHANNEL:
        if (dht22()) {
            String postStr = "THING_SPEAK_API";
                   postStr +="&amp;field1=";
                   postStr += String(humidity);
                   postStr +="&amp;field2=";
                   postStr += String(temperature);
                   postStr +="&amp;field3=";
                   postStr += String(roundPlayer);
                   postStr += "\r\n\r\n";

            client2.print("POST /update HTTP/1.1\n");
            client2.print("Host: api.thingspeak.com\n");
            client2.print("Connection: close\n");
            client2.print("X-THINGSPEAKAPIKEY: " + (String) "THING_SPEAK_API" + "\n");
            client2.print("Content-Type: application/x-www-form-urlencoded\n");
            client2.print("Content-Length: ");
            client2.print(postStr.length());
            client2.print("\n\n");
            client2.print(postStr);
        }

      break;
    }

    client2.stop();
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  switch(type) {
    case WStype_DISCONNECTED:
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);

      webSocket.sendTXT(num, "CONNECTED");
      }

      break;
    case WStype_TEXT:
      if(payload[0] == '#') {
        uint32_t rgb = (uint32_t) strtol((const char *) &payload[1], NULL, 16);

        analogWrite(RED_LED,    ((rgb >> 16) & 0xFF));
        analogWrite(GREEN_LED,  ((rgb >> 8) & 0xFF));
        analogWrite(BLUE_LED,   ((rgb >> 0) & 0xFF));
      }

      break;
  }
}

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

boolean loadFromSdCard(String path){
  String dataType = "text/plain";

  if (path.endsWith("/")) {
    path += "index.htm";
  }

  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".htm")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".xml")) {
    dataType = "text/xml";
  } else if (path.endsWith(".pdf")) {
    dataType = "application/pdf";
  } else if (path.endsWith(".zip")) {
    dataType = "application/zip";
  }

  File dataFile = SD.open(path.c_str());

  if (dataFile.isDirectory()) {
    path += "/index.htm";

    dataType = "text/html";

    dataFile = SD.open(path.c_str());
  }

  if (!dataFile) {
    return false;
  }

  if (server.hasArg("download")) {
    dataType = "application/octet-stream";
  }

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    setFlashLight(255,0,0);
  }

  dataFile.close();

  return true;
}

void handleFileUpload(){
  if (server.uri() != "/edit") {
    return;
  }

  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    if (SD.exists((char *)upload.filename.c_str())) {
      SD.remove((char *)upload.filename.c_str());
    }

    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if(uploadFile) {
      uploadFile.close();
    }
  }
}

void deleteRecursive(String path){
  File file = SD.open((char *)path.c_str());

  if (!file.isDirectory()) {
    file.close();

    SD.remove((char *)path.c_str());

    return;
  }

  file.rewindDirectory();

  while(true) {
    File entry = file.openNextFile();

    if (!entry) {
      break;
    }

    String entryPath = path + "/" +entry.name();

    if (entry.isDirectory()) {
      entry.close();

      deleteRecursive(entryPath);
    } else {
      entry.close();

      SD.remove((char *)entryPath.c_str());
    }

    yield();
  }

  SD.rmdir((char *)path.c_str());

  file.close();
}

void handleDelete(){
  if (server.args() == 0) {
    return returnFail("BAD REQUEST");
  }

  String path = server.arg(0);

  if (path == "/" || !SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");

    return;
  }

  deleteRecursive(path);

  returnOK();
}

void handleCreate(){
  if (server.args() == 0) {
    return returnFail("BAD REQUEST");
  }

  String path = server.arg(0);

  if (path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");

    return;
  }

  if (path.indexOf('.') > 0) {
    File file = SD.open((char *)path.c_str(), FILE_WRITE);

    if (file) {
      file.write((const char *)0);

      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }

  returnOK();
}

void printDirectory() {
  if (!server.hasArg("dir")) {
    return returnFail("BAD REQUEST");
  }

  String path = server.arg("dir");

  if (path != "/" && !SD.exists((char *)path.c_str())) {
    return returnFail("BAD PATH");
  }

  File dir = SD.open((char *)path.c_str());

  path = String();

  if (!dir.isDirectory()) {
    dir.close();

    return returnFail("NOT DIR");
  }

  dir.rewindDirectory();

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);

  server.send(200, "text/json", "");

  server.sendContent("[");

  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();

    if (!entry) {
      break;
    }

    String output;
  
    if (cnt > 0) {
      output = ',';
    }

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
 }

 server.sendContent("]");

 dir.close();
}

void handleResponse() {
  xmlString = "";
  xmlString += "<?xml version='1.0' encoding='UTF-8' ?>";
  xmlString += "<response>";
  xmlString += "  <temperature>" + (String) temperature + "</temperature>";
  xmlString += "  <humidity>" + (String) humidity + "</humidity>";
  xmlString += "</response>";

  server.send(200,"text/xml",xmlString);
}

void handleRequest() {
  byte redLed = byte(server.arg("redLed").toInt());
  byte greenLed = byte(server.arg("greenLed").toInt());
  byte blueLed = byte(server.arg("blueLed").toInt());

  roundPlayer = server.arg("roundPlayer").toInt();

  setFlashLight(redLed, greenLed, blueLed);

  if (roundPlayer != 0) {
    thingSpeak(NODEMCU_CHANNEL);

    roundPlayer = 0;
  }

  handleResponse();
}

void handleNotFound(){
  if (hasSD && loadFromSdCard(server.uri())) {
    return;
  }

  String message = "SDCARD Not Detected\n\n";

  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i=0; i<server.args(); i++) {
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

void setup() {
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(SSID_SERVER, PASS_SERVER);
  WiFi.begin(SSID_CLIENT, PASS_CLIENT);

  setFlashLight(0,0,0);

  while (WiFi.status() != WL_CONNECTED) {
    loading();
  }

  setFlashLight(0,0,0);

  if (MDNS.begin(HOST_NAME)) {
    MDNS.addService("http", "tcp", 3030);
    MDNS.addService("ws", "tcp", 3131);
  }

  server.on("/list", HTTP_GET, printDirectory);
  server.on("/edit", HTTP_DELETE, handleDelete);
  server.on("/edit", HTTP_PUT, handleCreate);
  server.on("/edit", HTTP_POST, [](){ returnOK(); }, handleFileUpload);
  server.on("/response", HTTP_PUT, handleResponse);
  server.on("/request", HTTP_GET, handleRequest);
  server.onNotFound(handleNotFound);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  WiFiClient client2 = server.client();

  if (SD.begin(SS)) {
    hasSD = true;
  } else {
    hasSD = false;
  }

  dht.begin();

  thingSpeak(NODEMCU_CHANNEL);
}

void loop() {
  webSocket.loop();

  if ((millis() - previousMillis) > INTERVAL) {
    thingSpeak(NODEMCU_CHANNEL);

    previousMillis = millis();
  }

  server.handleClient();
}
