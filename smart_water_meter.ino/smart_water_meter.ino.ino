void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}

#include <WiFi.h>
#include <HTTPClient.h>
#include <LCD_I2C.h>
ter me#include <ArduinoJson.h>

const int valvePin = 23; 
int tdspin=18;
int tdsdata=100;
int ThermistorPin = 34;
int Vo;
float R1 = 10000;
float logR2, R2, T;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;


const int sensorPin = 2; 
int turbidityPin = 35; 
volatile int pulseCount = 0; 
float flowRate = 0.0;
unsigned long previousMillis = 0;
const unsigned long interval = 1000;
float totalLiters = 0.0;
float literm3=0.0;


const char*ssid = "Manzi";
const char*password ="dillox12";
String serverName ="http://192.168.1.80/DashBoard_for_IOT/Backend.php";
const size_t JSON_DOC_SIZE = 512; 


LCD_I2C lcd(0x27, 20, 4);


void pulseCounter() {
  pulseCount++;
}


void setup() {
  lcd.begin();
  lcd.backlight();
  
  pinMode(18, INPUT); 
  pinMode(valvePin, OUTPUT); 
  digitalWrite(valvePin, LOW); 
  pinMode(sensorPin, INPUT_PULLUP); 
  attachInterrupt(digitalPinToInterrupt(sensorPin), pulseCounter, RISING);


  Serial.begin(115200);
  WiFi.begin(ssid,password);


  Serial.print("connecting to wifi...");
while(WiFi.status() != WL_CONNECTED)
{
  delay(500);
  Serial.print(".");
}
Serial.println("\n connected to wifi");
}


void loop() {
  // --- Sensor Readings & Flow Calculation (Every 1 Second) ---
  unsigned long currentMillis = millis();


  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    float calibrationFactor = 4.5;
    flowRate = (pulseCount / calibrationFactor) / (interval / 1000.0);
    float volumeAdded = flowRate / 60.0;
    totalLiters += volumeAdded;
    literm3= totalLiters / 1000;
    pulseCount = 0;


    Serial.print("Flow Rate: ");
    Serial.print(flowRate);
    Serial.println(" L/min");
    Serial.print("Total Volume: ");
    Serial.print(totalLiters, 3); 
    Serial.println(" L");
    Serial.print("Volume: ");
    Serial.print(literm3, 3); 
    Serial.println(" m3");


    lcd.setCursor(0, 0);
    lcd.print("smart water meter");
    lcd.setCursor(0, 1);
    lcd.print("F.rate: ");
    lcd.print(flowRate);
    lcd.print(" L/min");
    lcd.setCursor(0, 2);
    lcd.print("T.Volume: ");
    lcd.print(totalLiters, 3);
    lcd.print(" L");
    lcd.setCursor(0, 3);
    lcd.print("Cubic.M: ");
    lcd.print(literm3, 3);
    lcd.print(" m3");
  }


  Vo = analogRead(ThermistorPin);
  R2 = R1 * (4095.0 / (float)Vo - 1.0); 
  logR2 = log(R2);
  T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  T = T - 273.15; 
  
  int turbidityValue = analogRead(turbidityPin);
  int Turbidity=map(turbidityValue,0,4096,0,10);
  
  Serial.print("Temperature: "); Serial.print(T); Serial.println(" *c");
  Serial.print("Turbidity: "); Serial.print(Turbidity); Serial.println(" NTU");
  Serial.print("TDS: "); Serial.println(tdsdata);
  
  if(isnan(flowRate) || isnan(totalLiters) || isnan(Turbidity) || isnan(T) || isnan(tdsdata))
  {
    Serial.println("Failed to read from sensor!"); 
  }
  else
  {
    sendToServer(flowRate, totalLiters, Turbidity, T, tdsdata);
  }
  
  delay(10000); 
}


void sendToServer(float flowRate,float totalLiters,float Turbidity,float T,float tdsdata)
{
  if(WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("content-Type","application/json");


    // Construct the JSON payload to send SENSOR data to the server
    String jsonData = "{"
                   "\"flow_rate\": " + String(flowRate, 2) + "," +
                   "\"total_volume\": " + String(totalLiters, 2) + "," +
                   "\"turbidity_value\": " + String(Turbidity, 2) + "," +
                   "\"temperature\": " + String(T, 2) + "," +
                   "\"tds_value\": " + String(tdsdata, 2) +
                   "}";


    int httpResponseCode = http.POST(jsonData);


    if(httpResponseCode > 0)
    {
      String response = http.getString();
      Serial.print("Server Response Code: ");
      Serial.println(httpResponseCode);
      Serial.print("Raw Server Response Body: ");
      Serial.println(response); 


      StaticJsonDocument<JSON_DOC_SIZE> doc;
      DeserializationError error = deserializeJson(doc, response);


      if (error) {
          Serial.print("JSON Response Parsing failed: ");
          Serial.println(error.c_str());
      } else {
          const char* valveStatus = doc["valve_status"]; 
          
          if (valveStatus) {
              if (strcmp(valveStatus, "ON") == 0) {
                  digitalWrite(valvePin, HIGH);
                  Serial.println("Server set valve to ON.");
              } else if (strcmp(valveStatus, "OFF") == 0) {
                  digitalWrite(valvePin, LOW);
                  Serial.println("Server set valve to OFF.");
              } else {
                  Serial.print(" Unknown status received: ");
                  Serial.println(valveStatus);
              }
          } else {
              Serial.println("'valve_status' key missing in server response.");
          }
      }
    }
    else
    {
        Serial.printf("HTTP POST failed, code: %d\n", httpResponseCode);
    }
    http.end(); 
  }
  else
  {
    Serial.println("Wifi Disconnected");
  }
}