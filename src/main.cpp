#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define WIFI_SSID "SaoManoel"
#define WIFI_PASSWORD "sm070780b"

#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "7FU6sgkZlc8dRMCGen9rA3LHu2r4ATw-lRCZSY5zUVBfH1R5pVKgSaTGicu5k7DkDLDO4nI2uKz5JgAQuynfng=="
#define INFLUXDB_ORG "a226763e581ce3a4"
#define INFLUXDB_BUCKET "tcc"

#define TZ_INFO "UTC-3"
InfluxDBClient influxClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

Point sensorReadings("measurements");

#include "ThingSpeak.h"

#include <ModbusRTU.h> // https://github.com/emelianov/modbus-esp8266
#include <SoftwareSerial.h> // https://github.com/plerup/espsoftwareserial

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <max6675.h>

#define SLAVE_ID 2
#define FIRST_REG 0
#define REG_COUNT 1

#define RX 26
#define TX 27
#define DE_RE 5
SoftwareSerial RS485(RX, TX);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define DHTPIN 14
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define THERMO_SO 25
#define THERMO_CS 33
#define THERMO_CLK 32
MAX6675 thermocouple(THERMO_CLK, THERMO_CS, THERMO_SO);

#define TONE_PIN 13
#define TONE 500
#define TONE_TIME 50

ModbusRTU mb;

bool cb(Modbus::ResultCode event, uint16_t transactionId, void* data) {
  // if (event != Modbus::EX_SUCCESS) {
  //   Serial.print("Err 0x");
  //   Serial.println(event, HEX);
  // }
  return true;
}

void alarm(){
  tone(TONE_PIN, TONE, TONE_TIME);
}

void setup() {
  Serial.begin(115200);
  RS485.begin(9600, SWSERIAL_8N1);
  mb.begin(&RS485, DE_RE);
  mb.master();
  
  dht.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);

  pinMode(TONE_PIN, OUTPUT);

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  if (influxClient.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(influxClient.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(influxClient.getLastErrorMessage());
  }
}

void loop() {
  uint16_t res[REG_COUNT];
  if (!mb.slave()) {    // Check if no transaction in progress
    mb.readHreg(SLAVE_ID, FIRST_REG, res, REG_COUNT, cb); // Send Read Hreg from Modbus Server
    while(mb.slave()) { // Check if transaction is active
      mb.task();
      delay(10);
    }
    Serial.print("Temperatura (Controlador): ");
    Serial.println(res[0]);
  }

  //read temperature and humidity
  float tempDHT = dht.readTemperature();
  float humidity = dht.readHumidity();
  float tempC = thermocouple.readCelsius();

  if (abs(tempC - res[0]) > 5) {
    alarm();
  }

  if (isnan(humidity) || isnan(tempDHT)) {
    Serial.println("Erro ao ler o sensor DHT22!");
  } else {
    Serial.print("Temperatura (DHT22): ");
    Serial.println(tempDHT);
    Serial.print("Umidade relativa (%): ");
    Serial.println(humidity);
  }

  if (isnan(tempC)) {
    Serial.println("Erro ao ler o sensor MAX6675!");
  } else {
    Serial.print("Temperatura (MAX6675): ");
    Serial.println(tempC);
  }

  if (isnan(humidity) || isnan(tempDHT) || isnan(tempC)) {
    Serial.println("algum nulo");
  } else {
    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(0,0);
    display.println("TermoSync");

    display.setTextSize(1);
    display.print("Controlador: ");
    display.print(res[0]);
    display.print(" ");
    display.cp437(true);
    display.write(248);
    display.println("C");

    display.setTextSize(1);
    display.print("Supervisorio: ");
    display.print(tempC);
    display.print("");
    display.cp437(true);
    display.write(248);
    display.println("C");

    display.setTextSize(1);
    display.print("Parametros d ambiente");
    display.print("Temperatura: ");
    display.print(tempDHT);
    display.print(" ");
    display.cp437(true);
    display.write(248);
    display.println("C");

    display.setTextSize(1);
    display.print("Umidade: ");
    display.print(humidity);
    display.println(" %"); 

    display.display();
  }

  // Clear fields for reusing the point. Tags will remain the same as set above.
  sensorReadings.clearFields();
  sensorReadings.addField("controlador", res[0]);
  sensorReadings.addField("supervisorio", tempC);
  sensorReadings.addField("ambiente", tempDHT);

  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(sensorReadings.toLineProtocol());

  // Check WiFi connection and reconnect if needed
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
  // Write point
  if (!influxClient.writePoint(sensorReadings)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(influxClient.getLastErrorMessage());
  }

  Serial.println();
  delay(20000);
}