#include <SPI.h>
#include <Ethernet.h>

#include <ArduinoJson.h>

#define sensorMoisture A11
#define sensorHumidity A9
#define sensorTemperature A10
#define sensorSonicTrig 31
#define sensorSonicEcho 30
#define motor A8
#define LEDgalben 40
#define LEDverde 46
#define LEDrosu 47

static int airHumidity = 0;
static int soilHumidity = 0;
static int temperature = 0;
static float tankLevel = 0;
static int tankLevelPercent = 0;
static boolean pumpOn = false;
static boolean processOn = false;
static boolean autoIrrigation = true;
boolean reading = false;


static int minTankLevel = 0.5;

static int idealSoilMoisture = 50;
static int minSoilMoisture = 20;

static int idealTemperature = 24;
static int maxTemperature = 35;

static int minAirHumidity = 20;
static int maxAirHumidity = 70;

static int oldV = 0;

String myStr;


// ==================================================================== ETHERNET SERVER =========================================================
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

IPAddress ip(192, 168, 1, 177);
//IPAddress gateway(192, 168, 1, 1);
//IPAddress subnet(255, 255, 255, 0);

EthernetServer server(80);



// ==================================================================== IRIGATOR SETUP =========================================================
void setup(){
  Serial.begin(9600);
  pinSetup();
  setupEthernetShield();
  initFirst();
}

void pinSetup(){
  pinMode(sensorHumidity,INPUT);
  pinMode(sensorMoisture,INPUT);
  pinMode(sensorTemperature,INPUT);
  pinMode(sensorSonicTrig,OUTPUT);
  pinMode(sensorSonicEcho,INPUT);
  pinMode(motor,OUTPUT);
  pinMode(LEDverde, OUTPUT);
  pinMode(LEDrosu, OUTPUT);
  pinMode(LEDgalben, OUTPUT);
  Serial.println("pinurile au fost setate");
}

void setupEthernetShield() {
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }


  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
}


// ==================================================================== IRIGATOR LOOP =========================================================
void loop()
{
  checkForClient();
  gatherSensorInformation();
  if (processOn) {
     powerOn();
     computeLogic();
  } else {
    shutDownProcess();
  }

  delay(2);
}

void powerOn() {
  digitalWrite(LEDrosu, LOW);
  digitalWrite(LEDverde, HIGH);
}

void gatherSensorInformation() {
  airHumidity = getAirHumidity();
  soilHumidity = getSoilMoisture();
  temperature = getTemperature();
  tankLevel = getTankLevel();
  tankLevelPercent = getTankLevelPercent();
}

void shutDownProcess() {
  motorOFF();
  // powerOff();
  digitalWrite(LEDrosu, HIGH);
  digitalWrite(LEDverde, LOW);
}

void computeLogic() {
  if (autoIrrigation == false && pumpOn) {
    motorON();
    return;
  }

  if(autoIrrigation == false && !pumpOn) {
    motorOFF();
    return;
  }

  if(!processOn) {
    motorOFF();
    digitalWrite(LEDrosu, HIGH);
    digitalWrite(LEDverde, LOW);
  } else {
    digitalWrite(LEDrosu, LOW);
    digitalWrite(LEDverde, HIGH);
  }

  if( soilHumidity < minSoilMoisture && !soilHumidity > (minSoilMoisture + 5) ) {
    motorON();
    Serial.println("Sol foarte uscat -> Pornire motor");
    delay(1000);
    return;
  }

  if (autoIrrigation) {
    Serial.print("The value of processOn is ");
    Serial.println(processOn);
   if( soilHumidity < idealSoilMoisture && temperature < maxTemperature ) {
    //normal
    Serial.println(" Irigare normala - > Pornire motor ");
    motorON();
    delay(1000);
    return;
   } 

   if(airHumidity < minAirHumidity && temperature < maxTemperature && soilHumidity < minSoilMoisture) {
    //seceta
    motorON();
    delay(1000);
    Serial.println(" Seceta - > Pornire motor ");
    return;
   } 
   
    if (soilHumidity < idealSoilMoisture && airHumidity > maxAirHumidity) {
    //ploua
     motorOFF();
    }

    if(temperature > maxTemperature) {
      // sa nu se arda planta
      motorOFF();
    }

    if(airHumidity > maxAirHumidity) {
      // vreme umeda (gen London)
      motorOFF();
    }

    if(soilHumidity > idealSoilMoisture) {
      motorOFF();
    }
  }
}

void initFirst() {
    digitalWrite(LEDrosu, HIGH);
    digitalWrite(LEDverde, HIGH);
    digitalWrite(LEDgalben, HIGH);
    Serial.println("Initializint leds ...");
    delay(2500);
}

void writeJsonResponse(EthernetClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Content-Type: application/json;charset=utf-8");
  client.println("Server: Arduino");
  client.println("Connection: close");
  client.println();
  String response = "{\"soilHumidity\":" + String(soilHumidity) 
    + ", \"airHumidity\":" + String(airHumidity) 
    +  ", \"tankLevel\":" + String(tankLevel) 
    +  ", \"tankLevelPercent\":" + String(tankLevelPercent) 
    + ", \"tankUnit\":\"litre\""
    +  ", \"pumpOn\":" + String(pumpOn) 
    +  ", \"processOn\":" + String(processOn)
    +  ", \"autoIrrigation\":" + String(autoIrrigation) 
    + ", \"temperature\":" + String(temperature) 
    + ", \"tempUnit\":\"Celcius\" }";
  client.println(response);//"[{\"tempasdasddsaIn\":23.2, \"tempOut\":16.8, \"unit\":\"Celcius\" }]");
  client.println();
}


void processRequestData(EthernetClient client) {
  char json[] = {};
  char c;
  int i=0;

  while(client.available()) { 
    c = client.read();
    json[i] = c;
    i++;
  }
  
  json[i] = '\0';
  
  Serial.println();
  Serial.print("PostData = ");
  Serial.println(json);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if(!root.success()) {
    Serial.println("Parsing of json failed.\n");
    return;
  }

  if (root["pumpOn"] != "") {
    pumpOn = root["pumpOn"];
    Serial.print("Change pump state to ");
    Serial.println(pumpOn); 
  }

  if (root["processOn"] != "") {
    processOn = root["processOn"];
    Serial.print("Change process state to ");
    Serial.println(processOn); 
  }

  
  if (root["autoIrrigation"] != "") {
    autoIrrigation = root["autoIrrigation"];
    Serial.print("Change autoIrrigation state to ");
    Serial.println(autoIrrigation); 
  }
}


void checkForClient(){

  EthernetClient client = server.available();

  if (client) {
    // an http request ends with a blank line
    Serial.println("Client has connected!");
    boolean currentLineIsBlank = true;
    myStr = "";
    while (client.connected()) {
     while(client.available()) {
       char c = client.read();
       // if you've gotten to the end of the line (received a newline
       // character) and the line is blank, the http request has ended,
       // so you can send a reply
       if (c == '\n' && currentLineIsBlank) {  
         processRequestData(client);
         writeJsonResponse(client);
         client.stop();
       }
       else if (c == '\n') {
         // you're starting a new line
         currentLineIsBlank = true;
       } 
       else if (c != '\r') {
         // you've gotten a character on the current line
         currentLineIsBlank = false;
       }
     }
   }
    delay(100); // give the web browser time to receive the data
    client.stop();
    Serial.println("client disconnected");
  } 
}

// ==================================================================== SENSOR READINGS =========================================================
int getAirHumidity(){
  int sensorReadHumidity = analogRead(sensorHumidity);
  int humidity = map(sensorReadHumidity, 0, 1023, 0, 100);
  return humidity;
}

int getSoilMoisture(){
  int sensorReadMoisture = analogRead(sensorMoisture);
  int moisture = map(sensorReadMoisture, 400, 1023, 100, 0);
  if (moisture > 100){moisture = 100;}
  if (moisture < 0){moisture = 0;}
  return moisture; 
}

int getTemperature(){
  int sensorReadTemperature = analogRead(sensorTemperature);
  int temperature = map(sensorReadTemperature,0,1023,20,80);
  return temperature;
}

float getTankLevel(){
  float duration,distance,volume;
  digitalWrite(sensorSonicTrig,LOW);
  delayMicroseconds(2);
  digitalWrite(sensorSonicTrig,HIGH);
  delayMicroseconds(10);
  digitalWrite(sensorSonicTrig,LOW);
  duration = pulseIn(sensorSonicEcho,HIGH);
  distance = (duration/2)*0.0344;
  volume = 5.2 - ((distance * (30*30/PI))*0.001);
  if ( volume <=0 ) { return 0; }
  if (volume >=100) {return 100; }
  return volume;
}

int getTankLevelPercent(){
  return (tankLevel*100)/5.2;
}

// ==================================================================== PUMP ON AND OFF =========================================================
void motorON(){
  delay(1500);
  analogWrite(motor,128); 
  pumpOn = true;
  digitalWrite(LEDgalben, HIGH);
}

void motorOFF(){
  analogWrite(motor,0);
  pumpOn = false;
  digitalWrite(LEDgalben, LOW);
}
