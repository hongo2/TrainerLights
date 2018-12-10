
#include <ESP8266WiFi.h>
#include <TaskScheduler.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
//#include <ESP8266HTTPClient.h>
extern "C" {
#include "user_interface.h"
}


#define LEDA        D8  // D7  // led testigo BUILTIN_LED = 16;
#define LEDE        D6  // led estimulo de salida
#define LEDERR        D7  // led estimulo de salida
#define LEDOK        D8  // led estimulo de salida
#define TRIGGER_PIN D5  // pulsador uso multiple
#define TRIGGER     D1  // trigger ultrasonico
#define ECHO        D2  // echo ultrasonico

bool webSocketConnected = false;
WebSocketsClient webSocket;  //Client

//HTTPClient http;
String a;
int timeout;
int tdelay;
int min_detection_range = 0;
int max_detection_range = 50;

unsigned long time_start = millis();
unsigned long time_now = millis();

Scheduler ts;
void MeasureDistance();
void StimulusTimeout();
void StimulusStart();
void LedsOff();

// Tasks
Task tMeasureDistance(30, TASK_FOREVER, &MeasureDistance, &ts, true);
Task tStimulusStart(0, TASK_ONCE, &StimulusStart, &ts, false);
Task tStimulusTimeout(10000, TASK_ONCE, &StimulusTimeout, &ts, false);
Task tLedsOff(100, TASK_ONCE, &LedsOff, &ts, false);


//char ssid[] = "Speedy-16DD33"; // "NeuroTrainer";
//char password[] = "4314110680";

char ssid[] = "TrainerLights"; // "TrainerLights";
char password[] = "1234567890";

bool stimulating = false;


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      webSocket.begin("192.168.4.1", 81, "/");
      webSocketConnected = false;
      break;
    case WStype_CONNECTED: 
      Serial.printf("[WSc] Connected to url: %s\n", payload);

      webSocketConnected = true;

      
        // arma el json con la ip
        a = "{\"type\":\"sensor\",\"ip\":\"";
        a += String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]);
        a += "\"}";
        // send message to server
        webSocket.sendTXT(a);
    
      break;
      
    case WStype_TEXT:{
      Serial.printf("[WSc] get text: %s\n", payload);

      // recibe json desde el js en la pagina de control
      StaticJsonBuffer<512> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(payload);
      
      if (!root.success()) {
        Serial.println("JSON parsing failed!");
      }
     
      const char* jtype = root["type"];
      
      
      Serial.print("recibe: ");
      Serial.println(jtype);

      
      //************************************
      // estimulo    
      if (String(jtype) == String("stimulus")){
        Serial.println("ESTIMULO");
        timeout = root["timeout"];
        tdelay = root["delay"];
        min_detection_range = root["min_detection_range"];
        max_detection_range = root["max_detection_range"];
        tStimulusStart.setInterval(tdelay);
        tStimulusStart.restartDelayed();
      }
      
      // ************************************
      // restart
      if (String(jtype) == String("restart")){
        ESP.restart();
      }

      
      // send message to server
      // webSocket.sendTXT("message here");
    }
    break;
      
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      hexdump(payload, length);

      // send data to server
      // webSocket.sendBIN(payload, length);
      break;
  }

}


void monitorWiFi()
{

  // We start by connecting to a WiFi network
  if(WiFi.status() != WL_CONNECTED){
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    
    WiFi.mode(WIFI_STA);

    WiFi.begin(ssid, password);
    
    
    while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(LEDA, LEDA!=16); //titla el led hasta que se conecta 
      delay(300);
      digitalWrite(LEDA, LEDA==16); // si es el 16 BUILTIN_LED lo deja HIGH para apagarlo
      delay(200);
      Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    
    webSocket.begin("192.168.4.1", 81, "/");
    
    //webSocket.beginSocketIO("192.168.4.1", 81, "/");
    
    // event handler
    webSocket.onEvent(webSocketEvent); 
    // try ever 5000 again if connection has failed
    webSocket.setReconnectInterval(1000);

  }
}



 
void setup() {
  
    delay(500); 
  
    pinMode(TRIGGER_PIN, INPUT);
    pinMode(LEDE , OUTPUT);
    pinMode(LEDA , OUTPUT);
    pinMode(LEDERR , OUTPUT);
    pinMode(LEDOK , OUTPUT);

    
    pinMode(TRIGGER, OUTPUT);
    pinMode(ECHO, INPUT);
  
    Serial.begin (115200);
//    Serial.setDebugOutput(true);

    Serial.println(" ");
    Serial.println(" ");
    Serial.println(" ");
    Serial.println("*****************************");
    Serial.println("*                           *");
    Serial.println("*       TrainerLights       *");
    Serial.println("*    By: Ricardo Lerch      *");
    Serial.println("*  ricardo.lerch@gmail.com  *");
    Serial.println("*                           *");
    Serial.println("*****************************");
    Serial.println(" ");
    Serial.println(" ");
    Serial.println(" ");
//    Serial.end();

    // no duerme el wifi
    wifi_set_sleep_type(NONE_SLEEP_T);
    
}

void loop() {

    if (stimulating && !tMeasureDistance.isEnabled()){
      tMeasureDistance.enable();
    }
    if (!stimulating && tMeasureDistance.isEnabled()){
      tMeasureDistance.disable();
    }



//isEnabled()

  
//  if (stimulating){ // si esta estimulando mide, si no, no
//    tMeasureDistance.enable();
//  }else{
//    tMeasureDistance.disable();
//  }
    
  monitorWiFi();
  webSocket.loop();
  ts.execute();
  
}


long duration, distance, ldistance;
void MeasureDistance(){
  
  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  
  digitalWrite(TRIGGER, LOW);
  duration = pulseIn(ECHO, HIGH);
  distance = (duration/2) / 29.1;

              
  if (distance <= max_detection_range && distance >= min_detection_range) // aca detecto un objeto entre 3 y 50 cm
    {
      // detecto algo y...
      tStimulusTimeout.disable();
      time_now = millis();
      unsigned long response_time = time_now - time_start ;

        if (response_time > 50) { // si la respuesta es menor a 50 ms lo toma como error
        
          // apaga el led de estimulo
          digitalWrite(LEDE, LOW);
          digitalWrite(LEDOK, HIGH);
          tLedsOff.setInterval(100);
          tLedsOff.restartDelayed();
          // se pone en modo no stimulating
          stimulating = false;
          
          // manda info al server
          
          a = "{\"type\":\"response\",\"time\":\"" + String(response_time) + "\"";
          a += ",\"ip\":\"";
          a += String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]) + "\"" ;
          a += ",\"distance\":\"" + String(distance) + "\"";
          a += ",\"error\":\"0\"";
          a += "}";
          
    
          webSocket.sendTXT(a);
          
          if (distance != ldistance){    
            Serial.println(distance);
            ldistance = distance;
          }
        
      }else{
        
        StimulusTimeout();
        
      }
      
    }
    
}

void StimulusTimeout(){
      
      // apaga el led de estimulo
      digitalWrite(LEDE, LOW);
      // se pone en modo no stimulating
      stimulating = false;
      
      a = "{\"type\":\"response\",\"time\":\"" + String(tdelay + timeout) + "\"";
      a += ",\"ip\":\"";
      a += String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]) + "\"" ;
      a += ",\"error\":\"1\"";
    
      a += "}";
        
      webSocket.sendTXT(a);
      stimulating = false;
      
      digitalWrite(LEDERR, HIGH);
      tLedsOff.setInterval(200);
      tLedsOff.restartDelayed();
  
}

void LedsOff(){
  digitalWrite(LEDERR, LOW);
  digitalWrite(LEDOK, LOW);
}

void StimulusStart(){
  // empieza el estimulo con el delay
  // prende el led de estimulo y se pone en modo stimulating
  digitalWrite(LEDE, HIGH);
  time_start = millis();
  stimulating = true;
  tStimulusTimeout.setInterval(timeout);
  tStimulusTimeout.restartDelayed();
  
}

