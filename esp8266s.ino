#define BLYNK_PRINT Serial

#include <BlynkSimpleEsp8266.h>
#include <SimpleTimer.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// SCL GPIO5 | SDA GPIO4
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

// wemos d1 r2 /d1 mini pro
int offset = 0; // 0 10 20 30 40 50 60 70 80

int LICZ = V91;   // licznik
int TERM = V101;  // konfiguracja
int ALAS = V111;  // informacje tekstowe

WidgetTerminal terminal(V101);

// zakres temperatur dla DHT
const int min_temp = -40;
const int max_temp = 80;

char auth[] = "***";
const char* ssid = "***";
const char* password = "***";

int i = 0;
String als = "";

#include <dht.h>
dht DHT;
#define DHT22_PIN 2
float h = 0.0;
float t = 0.0;
double d = 0.0;
boolean as = true;

String dczas = "";

IPAddress ip;

#include <TimeLib.h>
#include <WidgetRTC.h>
BlynkTimer timer;
WidgetRTC rtc;

// int watchdog = 60*60*24; // 60s * 6m * 24h
int watchdog = 60*60*12; // 60s * 6m * 12h = restart co 12h
int cnt = 0;

void setup() {
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  //display.setRotation(1);  

  Serial.begin(115200);
  Serial.println("\r\nStart");

  screen_start(); delay(1000);
  Blynk.begin(auth, ssid, password);  
  //while (Blynk.connect() == false) { Serial.print("."); }

  // Other Time library functions can be used, like:
  // timeStatus(), setSyncInterval(interval)...
  // Read more: http://www.pjrc.com/teensy/td_libs_Time.html
  setSyncInterval(10 * 60); // Sync interval in seconds (10 minutes)

  ip = WiFi.localIP();
  screen_start(); delay(1000);

  timer.setInterval(30000, readTemp);     // odczyt raz na 30 sek
  timer.setInterval(1000, screen);        // zmien ekran raz na sekundę
  readTemp();
}

void loop() {
  Blynk.run();
  timer.run();
}

void readTemp() {
  int index = 0; i++;
  float selfheat = 0;
  String stat = "";
  
  int chk = DHT.read22(DHT22_PIN);

  if (chk == DHTLIB_OK) stat = "OK";
  else if (chk == DHTLIB_ERROR_CHECKSUM) stat = "Checksum";
  else if (chk == DHTLIB_ERROR_TIMEOUT) stat = "Time out,";
  else stat = "Unknown,";

  h = DHT.humidity;
  t = DHT.temperature - selfheat;
  d = dewPointAccurate(t, h);
  screen();
  Serial.println(String(i) + " " + dczas + " stat: " + stat);
  
  if (chk != DHTLIB_OK) return;

  char t_buffer[15];
  dtostrf(t, 4, 2, t_buffer);
  Blynk.virtualWrite(index + offset, t_buffer);
  Serial.println("V" + String(offset+index) + ": " + String(t) + " " + stat);
  dtostrf(h, 4, 2, t_buffer);
  Blynk.virtualWrite(index + offset + 1, t_buffer);
  Serial.println("V" + String(offset+index+1) + ": " + String(h) + " " + stat);
  dtostrf(d, 4, 2, t_buffer);
  Blynk.virtualWrite(index + offset + 2, t_buffer);
  Serial.println("V" + String(offset+index+2) + ": " + String(d) + " " + stat);
  
  Blynk.virtualWrite(index + offset + 5, dczas);
  Serial.println("V" + String(offset+index+5) + ": " + dczas);
}

BLYNK_CONNECTED() {
  // Synchronize time on connection
  rtc.begin();
}

BLYNK_WRITE(V100)
{
  int w = 0; // wybrany czujnik
  String p = String(param.asString());
  if (p == "ip") {
    ip = WiFi.localIP();
    terminal.println("Adres IP: " + IpAddress2String(ip));
  } else {
    terminal.println(F("Blynk v" BLYNK_VERSION ": "));
    terminal.println(String(ESP.getChipId()));
  }
  terminal.flush();
}

String IpAddress2String(const IPAddress& ipAddress) {
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ; 
}

void screen() {
  cnt++;
  setStringTime();
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.print("  ");
  display.println(dczas);
  display.println(" ");
  display.setTextSize(2);
  display.print(String(t));
  display.setTextSize(1);
  display.println("\n\n");
  String ofs = String() + (offset < 10 ? "0" : "") + offset;
  display.print(ofs);
  display.print("     ");
  String hh = String(h);
  hh = hh.substring(0,2);
  display.print(hh);
  display.print("%");
  display.display();

  if(cnt > watchdog){
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.println("Restart ...");
    display.println(dczas);
    display.display();
    delay(2000);
    Serial.println("Reset..");
    ESP.restart();
    cnt = 0;
  }
}

void screen_start() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.println(ssid);
  display.println(IpAddress2String(ip));
  display.println(" ");
  display.println(offset);
  display.display();
}

/*-----( Declare User-written Functions )-----*/
// dewPoint function NOAA
// reference (1) : http://wahiduddin.net/calc/density_algorithms.htm
// reference (2) : http://www.colorado.edu/geography/weather_station/Geog_site/about.htm
//
double dewPointAccurate (double celsius, double humidity)
{
  // (1) Saturation Vapor Pressure = ESGG(T)
  double RATIO = 373.15 / (273.15 + celsius);
  double RHS = -7.90298 * (RATIO - 1);
  RHS += 5.02808 * log10(RATIO);
  RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1 / RATIO ))) - 1) ;
  RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
  RHS += log10(1013.124); // NB = 1013 = absolute air pressure from BME280 sensor!!!!???????????????

  // factor -3 is to adjust units - Vapor Pressure SVP * humidity
  double VP = pow(10, RHS - 3) * humidity;

  // (2) DEWPOINT = F(Vapor Pressure)
  double T = log(VP / 0.61078); // temp var
  return (241.88 * T) / (17.558 - T);
}
/* ==== END Functions ==== */

void setStringTime() {
  dczas = String() + (hour() < 10 ? "0" : "") + hour() + ':' + (minute() < 10 ? "0" : "") + minute() + ':' + (second() < 10 ? "0" : "") + second();
}
