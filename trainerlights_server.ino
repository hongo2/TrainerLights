// author: Ricardo Lerch @RickLerch
// ricardo.lerch@gmail.com
#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library
#include <ESP8266mDNS.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <TaskScheduler.h>
#include <ArduinoJson.h>
#include <LinkedList.h>
#include <Time.h>
#include <pgmspace.h>

extern "C" {
#include "user_interface.h"
}

#define LEDA        D7  // led testigo
#define LEDE        D6  // led estimulo de salida
#define TRIGGER_PIN D5  // pulsador uso multiple
#define TRIGGER     D1  // trigger ultrasonico
#define ECHO        D2  // echo ultrasonico

const char htmlHeader[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang=en><head><title>TrainerLights</title><meta name=viewport content="width=device-width,initial-scale=1,minimum-scale=1"><meta charset=UTF-8></head><body><div class=maincontainer><div class=topmenubar><div class=logo><a href=/ ><h1>TrainerLights</h1></a></div></div><div class=topmargin></div><div class=success-message></div><div class=main-wrapper>
)rawliteral";

const char htmlFooter[] PROGMEM = R"rawliteral(
</div><div class=bottommargin></div><div class=footerbar><a href=/ >© TrainerLights</a> | <a href=/ >TrainerLights.cc</a></div><script>function listSensors(){connection.send('{"type":"list_sensors"}')}function restartESP(){var e='{"type":"restart"}';document.getElementById("restartESP").checked&&(document.getElementById("restartESP").checked=!1,console.log(e),connection.send(e))}function sendConfig(){var e='{"type":"config"';e+=',"tmode":"'+document.getElementById("tmode").value+'"',e+=',"min_delay":"'+document.getElementById("min_delay").value+'"',e+=',"max_delay":"'+document.getElementById("max_delay").value+'"',e+=',"mim_timeout":"'+document.getElementById("mim_timeout").value+'"',e+=',"max_timeout":"'+document.getElementById("max_timeout").value+'"',e+=',"accelerate_delay_percent":"'+document.getElementById("accelerate_delay_percent").value+'"',e+=',"accelerate_delay_per_seconds":"'+document.getElementById("accelerate_delay_per_seconds").value+'"',e+=',"accelerate_timeout_percent":"'+document.getElementById("accelerate_timeout_percent").value+'"',e+=',"accelerate_timeout_per_seconds":"'+document.getElementById("accelerate_timeout_per_seconds").value+'"',e+=',"min_detection_range":"'+document.getElementById("min_detection_range").value+'"',e+=',"max_detection_range":"'+document.getElementById("max_detection_range").value+'"',e+="}",console.log(e),connection.send(e)}function startTest(){connection.send('{"type":"start_test"}')}function stopTest(){connection.send('{"type":"stop_test"}')}function updateTimer(){d=new Date,n=d.getTime(),tm=n-startTime+timeOffset;var e,t=Math.floor(tm/1e3/60/60),m=Math.floor(tm/6e4)%60,o=(o=tm/1e3%60).toString().match(/^-?\d+(?:\.\d{0,-1})?/)[0];e=(e=("00"+tm).slice(-3)/10).toString().match(/^-?\d+(?:\.\d{0,-1})?/)[0],m=(m<10?"0":"")+m,o=(o<10?"0":"")+o,e=(e<10?"0":"")+e,0==(t+=t>0?":":"")&&(t=""),document.getElementById("timer").innerHTML=t+m+":"+o+"<small>."+e+"</small>"}function pauseTimer(){timeOffset=tm,clearInterval(timerInterval),document.getElementById("startTimer").href="javascript:startTimer();",document.getElementById("startTimer").innerHTML="Iniciar",stopTest()}function startTimer(){d=new Date,n=d.getTime(),startTime=n,clearInterval(timerInterval),timerInterval=setInterval(updateTimer,10),document.getElementById("startTimer").href="javascript:pauseTimer();",document.getElementById("startTimer").innerHTML="Detener",startTest()}function resetTimer(){timeOffset=0,tm=0,clearInterval(timerInterval),document.getElementById("timer").innerHTML="00:00<small>.00</small>",document.getElementById("startTimer").href="javascript:startTimer();",document.getElementById("startTimer").innerHTML="Iniciar",stopTest()}var loc;loc=location.hostname,"localhost"==location.hostname&&(loc="192.168.4.1");var connection=new WebSocket("ws://"+loc+":81/",["arduino"]);connection.onopen=function(){var e='{"type":"app_connected"';e+=',"current_time":"'+(new Date).getTime()+'"',e+="}",connection.send(e)},connection.onerror=function(e){console.log("WebSocket Error ",e)},connection.onmessage=function(e){console.log("Server: ",e.data);var t,n="";if("sensor_list"==(t=JSON.parse(e.data)).type){for(var m=0,o=t.sensors.length;m<o;++m){t.sensors[m];console.log("Sensor IP: ",t.sensors[m].ip+" | num: "+t.sensors[m].num),n+='<div class="sensor"><h1>'+(m+1)+"</h1></div>"}document.getElementById("sensors").innerHTML=n}"stats"==t.type&&(document.getElementById("test_score").innerHTML=t.test_score,document.getElementById("test_errors").innerHTML=t.test_errors,document.getElementById("max_distance").innerHTML=t.max_distance,document.getElementById("min_distance").innerHTML=t.min_distance,document.getElementById("avg_distance").innerHTML=t.avg_distance,document.getElementById("max_response_time").innerHTML=t.max_response_time,document.getElementById("min_response_time").innerHTML=t.min_response_time,document.getElementById("avg_response_time").innerHTML=t.avg_response_time)};var timerInterval,d=new Date,n=d.getTime(),startTime=n,timeOffset=0,tm=0</script></body></html>
)rawliteral";

const char htmlCss[] PROGMEM = R"rawliteral(
<style>/* CSS Bootstrap Customizations Ricardo Lerch @RickLerch */body{width:100%;padding:0;margin:0;font-size:14px;font-family:"Helvetica Neue",Helvetica,Arial,sans-serif;color:#333;background-color:#fff;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}table{width:100%;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}hr{margin-top:2px;margin-bottom:2px;border:0;border-top:1px solid #CCC}h1,h2,h3{margin-top:5px;margin-bottom:5px;font-weight:700}h1{font-size:20px}h2{font-size:18px}h3{font-size:16px}a,a:visited{color:#428bca;text-decoration:none}a:active,a:hover{outline:0;text-decoration:none;color:#516496}a.btn,a.btn:visited,a.btn:hover,a.btn:active{color:#FFF;text-decoration:none}ul{margin:0}.big{font-size:60px;margin:auto}.mid{font-size:40px;margin:auto}.red{color:#982713}.green{color:#408114}.blue{color:#144881}input{width:100%;padding:12px 4px;font-size:18px;border:2px solid #ccc;-webkit-border-radius:4px;-moz-border-radius:4px;border-radius:4px;outline:none;margin:0;margin-bottom:10px;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}textarea{resize:none;width:100%;padding:12px 4px;font-size:18px;line-height:30px;border:2px solid #ccc;-webkit-border-radius:4px;-moz-border-radius:4px;border-radius:4px;outline:none;margin:0;margin-bottom:10px;font-family:"Helvetica Neue",Helvetica,Arial,sans-serif;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}input[type=range]{-webkit-appearance:none;margin:18px 0;width:100%;border:none}input[type=range]::-webkit-slider-runnable-track{height:30%;cursor:pointer;animate:.2s;background:-webkit-linear-gradient(top,#555,#444,#222,#444,#555);border-radius:3px;border:2px solid #010101}input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;border:1px solid #000;height:40px;width:60px;border-radius:5px;background:-webkit-linear-gradient(left,#AAA,#BBB,#BBB,#BBB,#CCC,#AAA,#CCC,#AAA,#CCC,#AAA,#BBB,#BBB,#BBB,#AAA);cursor:pointer;border:2px solid #010101;margin-top:-20px}input[type=radio],input[type=checkbox]{display:none}label:before{content:"";display:inline-block;width:50px;height:50px;margin-right:10px;position:relative;left:0;box-sizing:border-box}.slabelo:before{border-radius:15px}.slabel:before{border-radius:4px;background:#FFF;border:2px solid #ccc}.checkbox label{margin-bottom:10px}.checkbox label:before{border-radius:3px}input[type=radio] + label:before{border-radius:25px}input[type=radio]:checked + label:before{content:"\2022";color:green;font-size:100px;text-align:center;font-weight:700;line-height:48px}input[type=checkbox]:checked + label:before{content:"\2713";text-shadow:1px 1px 1px rgba(0,0,0,.2);font-size:40px;font-weight:700;color:green;text-align:center;line-height:50px}.sensor{width:30%;float:left;text-align:center;margin:1%;border:#000 1px solid;background:#eaffe4}.sensors{width:100%;display:inline-block;text-align:center;border:#000 1px solid}.entire{width:100%;padding:1%;box-sizing:border-box}.half{width:47%;float:left;padding:1%}.third{width:31%;float:left;padding:1%}.twothird{width:64%;float:left;padding:1%}.cont{width:100%;display:inline-block;text-align:center}.contone{width:98%;display:inline-block;text-align:center;padding:1%}.main-wrapper{display:block;padding:10px}.left-wrapper{background-color:#FAFAFA;display:none}.edit-controls{padding:15px}.form-container{width:90%;margin:auto}.detail-title{float:left}.topmenubar,.footerbar{color:#FFF;width:100%;background:#16191B;color:#1f3853;font-size:12px;font-weight:400;z-index:2000;position:relative;display:inline-block;box-sizing:border-box}.topmargin{display:inline-block;height:45px;width:100%}.bottommargin{display:inline-block;height:100px;width:100%}.topmenubar{top:0;left:0;height:45px;position:fixed}.footerbar{bottom:0;vertical-align:middle;text-align:center;z-index:auto}.topmenubar a,.footerbar a{color:#FFF}.topmenubar a:hover,.footerbar a:hover{color:#CCC;text-decoration:none}.logo{display:inline-block;float:left;margin-right:15px;color:#fff;font-size:13px;text-align:center;text-shadow:0 1px 2px rgba(0,0,0,0.5);margin-left:15px;margin-top:10px}.searchbar{display:inline-block;width:100%;float:left;margin-top:15px}.feed{position:relative;display:block;width:100%;background:#fff;border-bottom:1px solid #CCC;min-height:136px;overflow:hidden}.view-detail{position:absolute;right:5px;bottom:5px}.maincontainer{width:100%;box-sizing:border-box;display:inline-block}.option small{font-weight:400}.option{position:relative;font-size:18px;font-weight:700;padding:15px 5px;margin:0;background-color:#FFF;border-style:solid;border-width:1px;border-color:#BBB transparent transparent;cursor:pointer;vertical-align:middle}.opt,.opt:hover,.opt:visited{color:#000}.option:hover,.feed:hover{background-color:#F7F8FF}.option:hover .circ{background:#3a4951}.options-top-bar{width:1102px;padding-left:5px;background:#FFF;box-sizing:border-box;display:inline-block;transition:.5s}.options-top-bar-title{float:left;padding-top:5px}.option-price-title{float:left;text-align:right;margin-left:10px;width:40%}.circ{float:right;background-color:#FFF;border-style:solid;border-width:2px;border-color:#BBB;border-radius:12px;width:20px;height:20px;position:absolute;right:5px;top:14px}.optdesc{float:left}.optsubtitle{margin-top:5px}h1 small,h2 small,h3 small{color:#929292}.selector{width:100%;font-size:18px;padding:10px 0;background:#FFF;border:2px solid #ccc;-webkit-border-radius:4px;-moz-border-radius:4px;border-radius:4px;outline:none;margin:0;margin-bottom:10px;height:50px}.btn{outline:0;display:inline-block;margin-bottom:0;font-weight:700;text-align:center;vertical-align:middle;cursor:pointer;background-image:none;border:1px solid transparent;white-space:nowrap;padding:12px;font-size:14px;line-height:1.428571429;border-radius:4px;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}.btn-block{display:block;width:100%;padding-left:0;padding-right:0}.btn-success{background:green;color:#FFF}.btn-danger{background:#970101;color:#FFF}.btn-danger:hover,.btn-danger:active,.btn-danger.active{background:#B32626;color:#FFF}.btn-success:hover,.btn-success:active,.btn-success.active{background:#409E40;color:#FFF}.btn-blue{background:#0007A7;color:#FFF}.btn-blue:hover,.btn-blue:active,.btn-blue.active{background:#516496;color:#FFF}.btn-lblue{background:#2FC2EF;color:#FFF}.btn-lblue:hover,.btn-lblue:active,.btn-lblue.active{background:#68D5F7;color:#FFF}.btn-red{background:#970101;color:#FFF}.btn-red:hover,.btn-red:active,.btn-red.active{background:#B32626;color:#FFF}</style>
)rawliteral";

const char htmlContent[] PROGMEM = R"rawliteral(
<div class=entire><h1>Tiempo de la prueba</h1><div class=cont><h1 class=big id=timer style=font-family:monospace>00:00.<small>00</small></h1></div><br><br><div class=cont><div class=twothird><a class="btn btn-block btn-lg btn-success"href=javascript:startTimer(); id=startTimer>Iniciar</a></div><div class=third><a class="btn btn-block btn-lg btn-danger"href=javascript:resetTimer();>Reset</a></div></div><hr><div class=cont><div class=half><h1>Puntos</h1><h1 class="big green"id=test_score>0</h1></div><div class=half><h1>Errores</h1><h1 class="big red"id=test_errors>0</h1></div></div><hr><h1>Tiempos de reacción (ms)</h1><div class=cont><div class=third><h1>Promedio</h1><h1 class="mid blue"id=avg_response_time>0</h1></div><div class=third><h1>Mínimo</h1><h1 class="mid green"id=min_response_time>0</h1></div><div class=third><h1>Máximo</h1><h1 class="mid red"id=max_response_time>0</h1></div></div><hr><h1>Distancias de reacción (cm)</h1><div class=cont><div class=third><h1>Promedio</h1><h1 class="mid blue"id=avg_distance>0</h1></div><div class=third><h1>Mínimo</h1><h1 class="mid green"id=min_distance>0</h1></div><div class=third><h1>Máximo</h1><h1 class="mid red"id=max_distance>0</h1></div></div></div><hr><br><br><div class=entire><h2>Modos de entrenamiento</h2><select class=selector id=tmode><option value=random>Al Azar<option value=sequence>En Secuencia</select></div><div class=entire style=display:none><h2>Modos de estímulo</h2><select class=selector id=stimulus_mode><option value=on>Luz Encendida<option value=blink>Parpadea<option value=blink_once>Parpadea una vez</select></div><div class=entire style=display:none><h2>Tiempo de la prueba</h2>Tiempo de la prueba en segundos <input id=time_test type=number value=0></div><div class=entire><h2>Delay</h2>Elije un valor de delay (ms) al azar entre:</div><div class=cont><div class=half><input id=min_delay type=number value=0></div><div class=half><input id=max_delay type=number value=0></div></div><div class=entire><h2>Timeout</h2>Elije un valor de timeout (ms) al azar entre:</div><div class=cont><div class=half>Mínimo <input id=mim_timeout type=number value=1000></div><div class=half>Máximo <input id=max_timeout type=number value=1000></div></div><div class=entire style=display:none><h2>Acelerar delay</h2>Acelera el delay x% por cada x seg</div><div class=cont style=display:none><div class=half>% <input id=accelerate_delay_percent type=number value=0></div><div class=half>Segundos <input id=accelerate_delay_per_seconds type=number value=0></div></div><div class=entire style=display:none><h2>Acelerar Timeout</h2>Acelera el timeout x% por cada x seg</div><div class=cont style=display:none><div class=half>% <input id=accelerate_timeout_percent type=number value=0></div><div class=half>Segundos <input id=accelerate_timeout_per_seconds type=number value=0></div></div><div class=entire><h2>Rango de detección</h2>Detecta un objeto entre mínimo y máximo (cm)</div><div class=cont><div class=half>Mínimo <input id=min_detection_range type=number value=0></div><div class=half>Máximo <input id=max_detection_range type=number value=50></div></div><div class=entire><a class="btn btn-block btn-lg btn-success"href=javascript:sendConfig();>Configurar</a></div><br><br><hr><br><br><div class=entire><a class="btn btn-block btn-lg btn-success"href=javascript:listSensors();>Listar Sensores</a><h1>Sensores Conectados:</h1><div class=sensors id=sensors></div></div><br><br><hr><br><br><div class=cont style=display:none><div class=third><center><input id=restartESP type=checkbox value=0><label class=slabel for=restartESP></label></center></div><div class=twothird><a class="btn btn-block btn-lg btn-danger"href=javascript:restartESP();>Reiniciar sistema</a></div></div>
)rawliteral";

// configuration
String tmode = "random";
int min_delay = 0;
int max_delay = 0;
int mim_timeout = 1000;
int max_timeout = 1000;
int accelerate_delay_percent = 0;
int accelerate_delay_per_seconds = 0;
int accelerate_timeout_percent = 0;
int accelerate_timeout_per_seconds = 0;
int min_detection_range = 0;
int max_detection_range = 50;

int timeout = 1000;
int tdelay = 0;


bool isTesting; // si esta dentro del tiempo de prueba mada los datos a la app

int currentSensor;

// test variables
int test_score = 0;
int test_errors = 0;
int max_distance = 0;
int min_distance = 9999;
int avg_distance = 0;
int max_response_time = 0;
int min_response_time = 9999;
int avg_response_time = 0;
int test_count = 0;

time_t app_time = 0;

Scheduler ts;
void MeasureDistance();
void StimulusTimeout();


// Tasks
// Task tMeasureDistance(30, TASK_FOREVER, &MeasureDistance, &ts, true);
Task tStimulusTimeout(2000, TASK_ONCE, &StimulusTimeout, &ts, false);

ESP8266WebServer webServer = ESP8266WebServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

const char* apName = "TrainerLights";
const char* apPassword = "1234567890";
const char* host = "";

// class para la lista de sensores conectados
class Sensor {
  public:
    IPAddress ip;   // ip del sensor
    bool isEnabled; // esta enabled
    uint8_t num;
};


LinkedList<Sensor*> sensorList = LinkedList<Sensor*>();

uint8_t appConnected = NULL;


// class para guardar el sensor que se esta ejecutando el estimulo
class sensorStimulating {
  public:
    IPAddress ip;   // ip del sensor
    
};

// esto se ejecuta cuando una estacion se desconecta
WiFiEventHandler stationDisconnectedHandler;
void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt) {
  Serial.println("** Station disconnected: **");
  
}

bool stimulating = false;
int lastSensor = 1000; // last sensor stimulating in sensorList

void setup() {
  delay(100);

  
    pinMode(TRIGGER_PIN, INPUT);
    pinMode(LEDE , OUTPUT);
    pinMode(LEDA , OUTPUT);
    pinMode(TRIGGER, OUTPUT);
    pinMode(ECHO, INPUT);

  
    Serial.begin (115200);
    Serial.setDebugOutput(true);
    
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


// https://github.com/esp8266/Arduino/issues/570
// aumenta el maximo de conexiones de 4(default) a 32
struct softap_config config;
wifi_softap_get_config(&config); // Get config first.
config.max_connection = 32; // how many stations can connect to ESP8266 softAP at most.

wifi_softap_set_config(&config);// Set ESP8266 softap config

   // no duerme el wifi
    wifi_set_sleep_type(NONE_SLEEP_T);
//
//
    WiFi.disconnect();


  
    WiFi.mode(WIFI_AP_STA);
    // definition
    // bool softAP(const char* ssid, const char* passphrase = NULL, int channel = 1, int ssid_hidden = 0, int max_connection = 4);
    
//    WiFi.softAP(apName, apPassword, 1, 0, 32);
    WiFi.softAP(apName, apPassword);
    
    Serial.print("AP IP address "); 
    Serial.println(WiFi.softAPIP());
        

    // start webSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    if(MDNS.begin("esp8266")) {
        Serial.println("MDNS responder started");
    }

    // handle index
    webServer.on("/", []() {
        // send index.html

        webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        webServer.sendHeader("Pragma", "no-cache");
        webServer.sendHeader("Expires", "-1");
        webServer.setContentLength(strlen_P(htmlHeader) + strlen_P(htmlCss) + strlen_P(htmlContent) + strlen_P(htmlFooter));
        webServer.send(200, "text/html", ""); 
        webServer.sendContent_P(htmlHeader);
        webServer.sendContent_P(htmlCss);
        webServer.sendContent_P(htmlContent);
        webServer.sendContent_P(htmlFooter);
        webServer.client().stop();
        
    });
    
    // start webServer server
    webServer.begin();

    // Add service to MDNS
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);

    // avisa cuando un cliente se desconecta del ap
    stationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(&onStationDisconnected);
    
}

void loop() {

  webSocket.loop();
  webServer.handleClient();
  ts.execute();

  
  // ejecutar programa de entrenamiento
  // si no esta estimulando elige un sensor al azar y lo pone on
  Sensor *s;
  String a;
  if (!stimulating){

    // stimulustTimeout = stimulustTimeout -  stimulustTimeout * 0.009;
    
    
    // elige un sensor al azar
    if (tmode == "random") {
      currentSensor = random(0,sensorList.size());
    }else{
      currentSensor++;
      if (currentSensor >= sensorList.size()){
        currentSensor = 0;
      }
   }

                   
    timeout = random(mim_timeout, max_timeout+1);
    tdelay = random(min_delay, max_delay+1);

    if (timeout < 100){
      timeout = 100;  
    }
    if (tdelay < 0){
      tdelay = 0; 
    }
    
    if (currentSensor != lastSensor && sensorList.size() > 0){ // solo elije uno diferente al ultimo
      
      lastSensor = currentSensor;
      s = sensorList.get(currentSensor);
      if (s){
        
        a = "{\"type\":\"stimulus\"";
        a += ",\"timeout\":\"" + String(timeout) + "\"";
        a += ",\"delay\":\"" + String(tdelay) + "\"";
        a += ",\"min_detection_range\":\"" + String(min_detection_range) + "\"";
        a += ",\"max_detection_range\":\"" + String(max_detection_range) + "\"";
        a += ",\"light\":{\"mode\":\"on\",\"intensity\":\"100\",\"color\":{\"R\":\"255\",\"G\":\"255\",\"B\":\"255\"}}}";
        
        webSocket.sendTXT(s->num, a);
        
        // pone el timeout del sensor mas 1000ms
        tStimulusTimeout.setInterval(tdelay+timeout+1000);
        tStimulusTimeout.restartDelayed();
        
        stimulating = true;

        Serial.println(a);

      }
    }
    
  }
  
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    IPAddress ip = webSocket.remoteIP(num);
    
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: 
            
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

            // send message to client
            webSocket.sendTXT(num, "{\"status\":\"connected\"}");
            
        break;
        
        case WStype_TEXT:
            Serial.printf("[%u] get Text: %s\n", num, payload);

            // recibe json desde el js en la pagina de control
            StaticJsonBuffer<512> jsonBuffer;
            JsonObject& root = jsonBuffer.parseObject(payload);
            
            if (!root.success()) {
              Serial.println("JSON parsing failed!");
            }
            
            const char* jtype = root["type"];

            Serial.print("recibe: ");
            Serial.println(jtype);
            
            
            Sensor *s;
            int i;
            String a="";
            
            // ************************************
            // List Sensors
            if (String(jtype) == String("list_sensors")){ // lista sensores conectados
              Serial.println("List Sensors");
              // devuelve un json con la lista de sensores conectados  
              a="{\"type\":\"sensor_list\",\"sensors\":[";
              for (i = 0; i < sensorList.size(); i++){
                  // Get sensors from list
                  s = sensorList.get(i);
                  Serial.println(s->ip);
                     // arma el json con la ip
                    if (i){
                        a +=",";
                    }
                    a += "{\"id\":\"" +String(s->ip)+ "\"";
                    a += ",\"ip\":\"" + String(s->ip[0]) + "." + String(s->ip[1]) + "." + String(s->ip[2]) + "." + String(s->ip[3]) + "\"";
                    a += ",\"num\":\"" + String(s->num) + "\"";
                    a += ",\"state\":\"on\"}";
                    
//                    String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3])
//                    a.concat("\"}");        
              }
              a +="]}";
              char jstr[a.length()+1];
              a.toCharArray(jstr, a.length()+1);
              // send websocket packet

              webSocket.sendTXT(num, a);  
            }
            
            // ************************************
            // recibe un nuevo sensor via websocket
            if (String(jtype) == String("sensor")){
              Serial.println("SENSOR");
              
              Sensor *newSensor = new Sensor();
              newSensor->ip = ip;
              newSensor->isEnabled = true;
              newSensor->num = num;
              
              bool sensorExists = false;
              Sensor *s;
              int i;
              for(i = 0; i < sensorList.size(); i++){
                  // Get sensors from list
                  s = sensorList.get(i);
                  
                  if (s->ip == newSensor->ip){  // el sensor ya existe en la lista lo deja ahi
                    sensorExists = true;
                  }
              }
              
              if (!sensorExists){
                  sensorList.add(newSensor);
              }
        
              Serial.println("Sensores conectados: ");
              // manda la lista de sensores conectados al serial
              for (i = 0; i < sensorList.size(); i++){
                  // Get sensors from list
                  s = sensorList.get(i);
                  Serial.println(s->ip);
              }   
              
            }

            // ************************************
            // response   
            if (String(jtype) == String("response")){
              Serial.println("SENSOR response");
              tStimulusTimeout.disable();
              stimulating = false;
              // si esta en modo test y la app_connected manda info a la app conectada
              if (isTesting){
                int test_error = root["error"];
                if (test_error){
                  test_errors++;
                }else{
                  test_score++;
                  
                  if (test_count){ // solo la primer cuenta
                    avg_response_time = (avg_response_time + int(root["time"])) / 2;
                    avg_distance = (avg_distance + int(root["distance"])) / 2;
                  }else{ 
                    test_score = 0;
                    test_errors = 0;
                    max_distance = root["distance"];
                    min_distance = root["distance"];
                    avg_distance = root["distance"];
                    max_response_time = root["time"];
                    min_response_time = root["time"];
                    avg_response_time = root["time"];
                  }
                    if (max_response_time < root["time"]){max_response_time = root["time"];}
                    if (min_response_time > root["time"]){min_response_time = root["time"];}
                    if (max_distance < root["distance"]){max_distance = root["distance"];}
                    if (min_distance > root["distance"]){min_distance = root["distance"];}
                }
  
                // si esta conectada la app manda la info via websocket
                if (appConnected != NULL){
                  String a;
                  a = "{\"type\":\"stats\"";
                  a += ",\"test_score\":\"" + String(test_score) + "\"";
                  a += ",\"test_errors\":\"" + String(test_errors) + "\"";
                  a += ",\"max_distance\":\"" + String(max_distance) + "\"";
                  a += ",\"min_distance\":\"" + String(min_distance) + "\"";
                  a += ",\"avg_distance\":\"" + String(avg_distance) + "\"";
                  a += ",\"max_response_time\":\"" + String(max_response_time) + "\"";
                  a += ",\"min_response_time\":\"" + String(min_response_time) + "\"";
                  a += ",\"avg_response_time\":\"" + String(avg_response_time) + "\"";

                  a += "}";
                  webSocket.sendTXT(appConnected, a);
                  Serial.println(a);
                }

                test_count++;
              }
            } 

            // ************************************
            // start_test
            if (String(jtype) == String("start_test")){
              Serial.println("start_test");
              isTesting = true;
              // test variables
              test_score = 0;
              test_errors = 0;
              max_distance = 0;
              min_distance = 9999;
              avg_distance = 0;
              max_response_time = 0;
              min_response_time = 9999;
              avg_response_time = 0;
              test_count = 0;
            }
            // ************************************
            // stop_test
            if (String(jtype) == String("stop_test")){
              Serial.println("stop_test");
              isTesting = false;              
            }
                        
            // ************************************
            // app_connected
            if (String(jtype) == String("app_connected")){
              appConnected = num;
              app_time = root["current_time"];
              Serial.print("current_time: ");
              Serial.println(app_time);
              Serial.print("app_connected: ");
              Serial.println(appConnected);
            }

            
            // ************************************
            // restart
            if (String(jtype) == String("restart")){
              for (i = 0; i < sensorList.size(); i++){
                s = sensorList.get(i);
                a="{\"type\":\"restart\"}";
                webSocket.sendTXT(s->num, a);
              }
              Serial.println("RESET");
              ESP.reset();
            }

            
            // ************************************
            // config
            if (String(jtype) == String("config")){ // lista sensores conectados
              Serial.println("Get Configuration"); 
              const char* ctmode = root["tmode"];
              tmode = String(ctmode);
              min_delay = root["min_delay"];
              max_delay = root["max_delay"];
              mim_timeout = root["mim_timeout"];
              max_timeout = root["max_timeout"];
              accelerate_delay_percent = root["accelerate_delay_percent"];
              accelerate_delay_per_seconds = root["accelerate_delay_per_seconds"];
              accelerate_timeout_percent = root["accelerate_timeout_percent"];
              accelerate_timeout_per_seconds = root["accelerate_timeout_per_seconds"];
              min_detection_range = root["min_detection_range"];
              max_detection_range = root["max_detection_range"];
              Serial.println(max_detection_range); 
            }


            
            break;
            
      }
            
            
    }


void StimulusTimeout(){
  // el ultimo sensor estimulado no respondio lo saca de la lista
  sensorList.remove(lastSensor);
  lastSensor = 1000;
  stimulating = false;
  Serial.println("StimulusTimeout");
}








