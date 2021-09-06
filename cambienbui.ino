#include<string.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <NTPtimeESP.h>
#include <Arduino.h>
#include <Nokia_5110.h>

#include "DHTesp.h" 
#include <Ticker.h>

#ifndef ESP32
#pragma message
#error Select ESP32 board.
#endif

#define DEBUG_ON
#define LENG 31  //0x42 + 31 bytes equal to 32 bytes
unsigned char buf[LENG];
NTPtime NTPch("ch.pool.ntp.org");   // Choose server pool as required  get the time from the Internet.
//khai bao chan LCD 
#define RST 4
#define CE 32
#define DC 21
#define DIN 18
#define CLK 19

Nokia_5110 lcd = Nokia_5110(RST, CE, DC, DIN, CLK);

DHTesp dht;
bool getTemperature();
ComfortState cf;

int dhtPin = 34;
//khoi tao  ID ,pass wifi
#define WIFI_SSID "Lêk Xù"
#define WIFI_PASSWORD "20141582"

//chi den dia chi firebase
#define FIREBASE_HOST "enviroment-64adc-default-rtdb.firebaseio.com"

/** The database secret is obsoleted, please use other authentication methods, 
 * see examples in the Authentications folder. 
*/
#define FIREBASE_AUTH "r7ojs5Wzdud2WpvLdo8BJBDqLB2YEOdL58iAeHpY"
//Define FirebaseESP32 data object
FirebaseData fbdo;

FirebaseJson json;

strDateTime dateTime;


byte buff[2];
unsigned long duration;
unsigned long starttime;
unsigned long endtime;
unsigned long sampletime_ms = 30000;
unsigned long lowpulseoccupancy = 0;
float value_MQ7, value_MQ135;

float PM01Value=0;
float PM2_5Value=0;
float PM10Value=0;
float ratio = 0;
int concentration = 0;
int count_co = 0;
int count_pm = 0;
int count_time = 0;
int count_135 = 0;
int count_dust = 0;
int check_update=0;
int check_time;
int check_up_data_PMSA003 = 0;
String h, m, s, d, month, y;
String path_pm1 = "/home1/pm1/"; 
String path_pm25 = "/home1/pm25/"; 
String path_pm10 = "/home1/pm10/"; 
String path_co = "/home1/co/"; 
String path_time = "/home1/time/";
String path_dust = "/home1/dust/";
int co_ng = 0;
int pm1_ng = 0;
int pm10_ng = 0;
int pm25_ng = 0;
int ppm_CO = 0;

void setup()
{
  //setup dau ra dau vao
  pinMode(23,INPUT);
  pinMode(14,OUTPUT);   //chân nối với cực âm của còi báo
  digitalWrite(14, 1);
  pinMode(34, INPUT);
  pinMode(35, INPUT);
  Serial.begin(115200);
  Serial2.begin(9600);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); 
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  initTemp();
  lcd.setContrast(40);
  lcd.clear(); // xoa lcd
  lcd.setCursor(2,0); // set vi tri hien thi
  lcd.print("Giam sat");
  lcd.setCursor(2,2); // set vi tri hien thi
  lcd.print("moi truong");
//  pinMode(26, INPUT);
  starttime = millis();   
}
// cam bien 
void PMSA003(){
//  digitalWrite(14, 0);
   if(Serial2.find(0x42)){    //start to read when detect 0x42
    Serial2.readBytes(buf,LENG);

    if(buf[0] == 0x4d){
      if(checkValue(buf,LENG)){
        PM01Value=transmitPM01(buf);  //count PM1.0 value of the air detector module
        PM2_5Value=transmitPM2_5(buf);  //count PM2_5 value of the air detector module
        PM10Value=transmitPM10(buf);  //count PM10 value of the air detector module
      }  
    }
  }

static unsigned long OledTimer=millis(); // set up thoi gian tinhs toan du lieu
  if (millis() - OledTimer >=1000)
  {
    OledTimer=millis();
    if (Firebase.getInt(fbdo, "/home1/count_pm")) {
      count_pm = fbdo.intData();
    }     
    Serial.print("PM1.0: ");
    Serial.print(PM01Value);
    Serial.println(" ug/m3");    
    Firebase.setFloat(fbdo,path_pm1+count_pm,PM01Value);
    Serial.print("PM2.5: ");
    Serial.print(PM2_5Value);
    Serial.println(" ug/m3");
    Firebase.setFloat(fbdo,path_pm25+count_pm,PM2_5Value);  
    Serial.print("PM10: ");
    Serial.print(PM10Value);
    Serial.println(" ug/m3");
    Serial.println();
    Firebase.setFloat(fbdo,path_pm10+count_pm,PM10Value); 
    int count = count_pm + 1;   
    check_up_data_PMSA003 = 0;
    Firebase.setInt(fbdo,"/home1/count_pm", count); 
//    digitalWrite(14, 1);
    lcd.setCursor(0,0);
    lcd.print("PM01: ");
    lcd.setCursor(33,0);
    lcd.clear(0, 33, 51);
    lcd.print(PM01Value);
    
    lcd.setCursor(0,1);
    lcd.print("PM2.5: ");
    lcd.setCursor(33,1);
    lcd.clear(1, 33, 51);
    lcd.print(PM2_5Value);
    
    lcd.setCursor(0,2);
    lcd.print("PM10: ");  
    lcd.setCursor(33,2);
    lcd.clear(2, 33, 51);
    lcd.print(PM10Value);
  }
}

void warning(){
    if (Firebase.getInt(fbdo, "/home1/co_ng")) {
      co_ng = fbdo.intData();
    }
    if (Firebase.getInt(fbdo, "/home1/pm1_ng")) {
      pm1_ng = fbdo.intData();
    }
    if (Firebase.getInt(fbdo, "/home1/pm10_ng")) {
      pm10_ng = fbdo.intData();
    }
    if (Firebase.getInt(fbdo, "/home1/pm25_ng")) {
      pm25_ng = fbdo.intData();
    }
}

void CO(){
  value_MQ7 = analogRead(35);
  if (Firebase.getInt(fbdo, "/home1/count_co")) {
      count_co = fbdo.intData();
  }
  int ppm_CO = (value_MQ7*980)/4095 + 20;
  Serial.println("ppm_CO");
  lcd.setCursor(0,4);
  lcd.print("CO: ");  
  lcd.setCursor(33,4);
  lcd.clear(4, 33, 51);
  lcd.print(ppm_CO);
  Firebase.setFloat(fbdo,path_co+count_co,ppm_CO); 
  int co = count_co+1;
  Firebase.setFloat(fbdo,"/home1/count_co",co); 
}

void loop()
{  
  getTemperature();
  dateTime = NTPch.getNTPtime(7.0, 0);  //Múi giờ VN 7 UTC
  if(dateTime.valid){
    NTPch.printDateTime(dateTime);
    int actualHour = dateTime.hour;
    int actualMinute = dateTime.minute;
    int actualsecond = dateTime.second;
    int actualyear = dateTime.year;
    int actualMonth = dateTime.month;
    int actualday =dateTime.day;
    check_time = actualMinute;
     h = String(actualHour);
     m = String(actualMinute);
     s = String(actualsecond);
     d = String(actualday);
     month = String(actualMonth);
     y = String(actualyear);     
 }
 String date = d + "-" + month + "-" + y + " " + h + ":" + m + ":" + s; 

 if(check_time%2 == 0 && check_update == 1){
    warning();
    if (Firebase.getInt(fbdo, "/home1/count_time")) {
          count_time = fbdo.intData();
    }
    int count_time_1 = count_time+1;
    Firebase.setString(fbdo,path_time+count_time,date);  
    Firebase.setFloat(fbdo,"/home1/count_time",count_time_1);     
    // 1 cách fix là check điều kiện cho từng hàm check_func = 0
    check_up_data_PMSA003 = 1;
    PMSA003();    
    CO();      
    if( PM01Value > pm1_ng || PM2_5Value > pm25_ng || PM10Value > pm10_ng || ppm_CO > co_ng){      
      //tạm dừng
      for(int i=0; i < 2000; i++){
        digitalWrite(14, 0);
        Serial.println("buzz");
      }
      digitalWrite(14, 1);
    }
    check_update = 0;    
 } 
 if(check_time%2 == 1){
    check_update = 1;
 }
 if(check_up_data_PMSA003 == 1){
    PMSA003();    
 } 
}

char checkValue(unsigned char *thebuf, char leng)  //ham check
{
  char receiveflag=0;
  int receiveSum=0;

  for(int i=0;i<(leng-2);i++){
    receiveSum=receiveSum+thebuf[i];
  }
  receiveSum=receiveSum + 0x42;

  if(receiveSum == ((thebuf[leng-2]<<8)+thebuf[leng-1]))
  {
    receiveSum = 0;
    receiveflag = 1;
  }
  return receiveflag;
}
int transmitPM01(unsigned char *thebuf)
{
  int PM01Val;
  PM01Val=((thebuf[3]<<8)+thebuf[4]);  //count PM1.0 value of the air detector module
  return PM01Val;
}
//transmit PM Value to PC
int transmitPM2_5(unsigned char *thebuf)
{
  int PM2_5Val;
  PM2_5Val=((thebuf[5]<<8)+thebuf[6]);  //count PM2.5 value of the air detector module
  return PM2_5Val;
}
//transmit PM Value to PC
int transmitPM10(unsigned char *thebuf)
{
  int PM10Val;
  PM10Val=((thebuf[7]<<8)+thebuf[8]);  //count PM10 value of the air detector module
  return PM10Val;
}
bool initTemp() {
  byte resultValue = 0;
  dht.setup(dhtPin, DHTesp::DHT11);
  Serial.println("DHT initiated");
  return true;
}

bool getTemperature() {

  TempAndHumidity newValues = dht.getTempAndHumidity();

  float heatIndex = dht.computeHeatIndex(newValues.temperature, newValues.humidity);
  float dewPoint = dht.computeDewPoint(newValues.temperature, newValues.humidity);
  float cr = dht.getComfortRatio(cf, newValues.temperature, newValues.humidity);
  Firebase.setInt(fbdo,"/temp",newValues.temperature);
  Firebase.setInt(fbdo,"/humi",newValues.humidity);
  return true;
}
