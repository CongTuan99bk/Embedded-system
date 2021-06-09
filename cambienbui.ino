// Connect the Pin_3 of DSM501A to Arduino 5V
// Connect the Pin_5 of DSM501A to Arduino GND
// Connect the Pin_2 of DSM501A to Arduino D8
// www.elecrow.com
#include <string.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <NTPtimeESP.h>
#include <Arduino.h>
#include <Nokia_5110.h>

#define DEBUG_ON
#define LENG 31  //0x42 + 31 bytes equal to 32 bytes
unsigned char buf[LENG];
NTPtime NTPch("ch.pool.ntp.org");   // Choose server pool as required

//nokia5110
#define RST 4
#define CE 32
#define DC 21
#define DIN 18
#define CLK 19

Nokia_5110 lcd = Nokia_5110(RST, CE, DC, DIN, CLK);

#define WIFI_SSID "FPT Telecom-6A5A"
#define WIFI_PASSWORD "dobietday"
#define FIREBASE_HOST "environment-2abbb-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "R4JGdaL0zz0mxdJ8qEBkTfRMXCJeTqiRi5BwYQiz"
//Define FirebaseESP32 data object
FirebaseData fbdo;

FirebaseJson json;

strDateTime dateTime;

byte buff[2];
int pin = 23;//DSM501A input D8
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
int check_up_data_DSM501A = 0;
String h, m, s, d, month, y;
String path_pm1 = "/home1/pm1/"; 
String path_pm25 = "/home1/pm25/"; 
String path_pm10 = "/home1/pm10/"; 
String path_co = "/home1/co/"; 
String path_time = "/home1/time/";
String path_dust = "/home1/dust/";
int dust_ng = 0;
int co_ng = 0;
int pm1_ng = 0;
int pm10_ng = 0;
int pm25_ng = 0;
int ppm_CO = 0;

void setup()
{
  pinMode(23,INPUT);
  pinMode(14,OUTPUT);
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
  lcd.setContrast(40);
  lcd.clear(); // xoa lcd
  lcd.setCursor(2,0); // set vi tri hien thi
  lcd.print("Giam sat");
  lcd.setCursor(2,2); // set vi tri hien thi
  lcd.print("moi truong");
  starttime = millis();   
}

void PMSA003(){
  digitalWrite(14, 0);
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
    digitalWrite(14, 1);
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

void DSM501A(){
  duration = pulseIn(pin, LOW);
  lowpulseoccupancy += duration;
  endtime = millis();
  if ((endtime-starttime) > sampletime_ms)
  {
    ratio = (lowpulseoccupancy-endtime+starttime + sampletime_ms)/(sampletime_ms*10.0);  // Integer percentage 0=>100
    concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62; // using spec sheet curve
    if(concentration > 2000){
      concentration = 233;
    }
    Serial.print("lowpulseoccupancy:");
    Serial.print(lowpulseoccupancy);
    Serial.print("    ratio:");
    Serial.print(ratio);
    Serial.print("DSM501A:");
    Serial.println(concentration);
    
    lcd.setCursor(0,3);
    lcd.print("DSM501A: ");  
    lcd.setCursor(40,3);
    lcd.clear(3, 45, 51);
    lcd.print(concentration);

    if (Firebase.getInt(fbdo, "/home1/count_dust")) {
          count_dust = fbdo.intData();
    }
    int count_dust_1 = count_dust+1;
    Firebase.setFloat(fbdo,path_dust+count_dust,concentration);  
    Firebase.setFloat(fbdo,"/home1/count_dust",count_dust_1);  
    check_up_data_DSM501A = 0;   
//    Firebase.setFloat(fbdo,"/DSM501A",concentration);
    lowpulseoccupancy = 0;
    starttime = millis();
  } 
}

void warning(){
   if (Firebase.getInt(fbdo, "/home1/dust_ng")) {
      dust_ng = fbdo.intData();
    }
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
    Serial.println(dust_ng);
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
  dateTime = NTPch.getNTPtime(7.0, 0);
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
    check_up_data_DSM501A = 1;
    PMSA003();   
    DSM501A();
    CO();      
    if( concentration > dust_ng || PM01Value > pm1_ng || PM2_5Value > pm25_ng || PM10Value > pm10_ng || ppm_CO > co_ng){      
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
 if(check_up_data_PMSA003 == 1){
    DSM501A();    
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
