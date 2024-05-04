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

#define TONE_PIN 12

ModbusRTU mb;

bool cb(Modbus::ResultCode event, uint16_t transactionId, void* data) {
  // if (event != Modbus::EX_SUCCESS) {
  //   Serial.print("Err 0x");
  //   Serial.println(event, HEX);
  // }
  return true;
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
}

void loop() {
  tone(TONE_PIN, 5000, 100);
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

  // colocar o if aqui
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

  Serial.println();
  delay(1000);
  noTone(TONE_PIN);
}