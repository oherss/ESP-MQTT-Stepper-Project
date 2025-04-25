/*
 MQTT connected stepper motor firmware developed for Simmoe by Oliver "Sproxxy" Herss March 2025 based on the PD stepper project.

 Make sure to have the ESP-32 boards in your IDE to build for the hardware.
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/////////////////////////
//    WiFi Details     //
/////////////////////////

const char* ssid = "Next-Iot";
const char* password = "shin-pygmy-renal-Hahk";

/////////////////////////
//     MQTT Server     //
/////////////////////////

const char* mqtt_server = "mqtt-plain.nextservices.dk";
uint16_t mqtt_port = 1883;
const int motor_id = 1;

/////////////////////////
//     Pin Defines     //
/////////////////////////

//TMC2209 Stepper Driver
#define TMC_EN  21 //Low to enable motor, high to disable)
#define STEP    5
#define DIR     6
#define MS1     1
#define MS2     2
#define SPREAD  7
#define TMC_TX  17
#define TMC_RX  18
#define DIAG    16
#define INDEX   11

//PD Trigger (CH224K)
#define PG      15  //power good singnal (dont enable stepper untill this is good)
#define CFG1    38
#define CFG2    48
#define CFG3    47

//Other
#define VBUS    4
#define NTC     7
#define LED1    10
#define LED2    12

/////////////////////////////
// Global Variable Defines //
/////////////////////////////

//Motor speeds
int delays[] = {15, 2000, 1000, 700, 400, 200, 100, 75, 50, 40, 30}; //list of speeds (higher = slower, 0 = stop)
int numSpeeds = 10;
bool runMotor = 0;

//Led flashing 
long lastFlash = 0;
int flashInt = 100;
bool flashState = 0;

//MQTT topic
String MQTTTopic = "ESPStepper";
String MQTTTopicAnnounce = "ESPSteppers";

//MQTT subtopics for specific motor ID
String sendTopic;
String recieveTopic;

//combined MQTT message
String MQTTmsg = "";

// Extracted JSON parameters from message
int Steps = 0;
int Speed = 0;
int Dir = 0;

//WiFi
WiFiClient espClient;

//MQTT function variables
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  //WiFi connection established
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    digitalWrite(LED1, HIGH);
    delay(250);
    digitalWrite(LED1,LOW);
  }

  //Show that WiFi connection was sucessful via serial, and show the current IP adress
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) { //Runs whenever we receive a message on the recive topic
  //recieve message
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  //combines characters into full string
  MQTTmsg = "";
  for (int i = 0; i < length; i++) {
    MQTTmsg += (char)payload[i];
  }
  //prints received message to console
  Serial.println(MQTTmsg);

  StaticJsonDocument<200> doc;  // Define a JSON document with a suitable size
  DeserializationError error = deserializeJson(doc, MQTTmsg); //interpret JSON from received message

if (error) { //Prints JSON error to console and via MQTT to user
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    String messageToSend = "Json parse failed: " + String(error.f_str());
    client.publish(sendTopic.c_str(),  messageToSend.c_str());
    return;
  }

  String StepString = doc["Steps"];
  if(StepString == "Run"){
    runMotor = 1;
    Speed = doc["Speed"];
    Dir = doc["Dir"];
    if (Dir == 1){ //set direction
      digitalWrite(DIR, HIGH);
    } else {
      digitalWrite(DIR, LOW);
    }
    digitalWrite(TMC_EN, LOW);
    String stringToSend = "Running motor at speed: " + String(Speed);
    Serial.println(stringToSend);
    client.publish(sendTopic.c_str(), stringToSend.c_str());

  }
  else if(StepString == "Stop"){
    runMotor = 0;
    digitalWrite(TMC_EN, HIGH);
    Serial.println("Motor stopped");
    client.publish(sendTopic.c_str(),  "Motor stopped");
  }
  else{
    Steps = doc["Steps"];  // Assuming the JSON contains a parameter named "Steps"
    Speed = doc["Speed"];  // Assuming the JSON contains a parameter named "Speed"
    Dir = doc["Dir"];  // Assuming the JSON contains a parameter named "Dir"
    doSteps(Dir, Steps, Speed); //Runs function for moving the motor
  }

  

 // Print the extracted values (for debugging)
  Serial.print("Steps: ");
  Serial.println(Steps);
  Serial.print("Speed: ");
  Serial.println(Speed);
  Serial.print("Direction: ");
  Serial.println(Dir);

  
}

void reconnect() { //MQTT connection to server
  // Loop until we're connected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTTTopic.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(sendTopic.c_str(), "Motor Online");
      client.publish(MQTTTopicAnnounce.c_str(), sendTopic.c_str());
      Serial.print("Response topic: ");
      Serial.println(sendTopic);
      // ... and resubscribe
      client.subscribe(recieveTopic.c_str());
      Serial.print("Subscribed to topic: ");
      Serial.println(recieveTopic);
    } else { // If we fail to connect to MQTT server
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  
    //PD Trigger Setup
  pinMode(PG, INPUT);
  pinMode(CFG1, OUTPUT);
  pinMode(CFG2, OUTPUT);
  pinMode(CFG3, OUTPUT);
                            //  5V   9V   12V   15V   20V  (Can also be changed on the fly)
  digitalWrite(CFG1, HIGH); //  1    0     0     0     0
  digitalWrite(CFG2, LOW);  //  -    0     0     1     1
  digitalWrite(CFG3, LOW);  //  -    0     1     1     0
  
  //Stepper simple setup (no serial comms)
  pinMode(TMC_EN, OUTPUT);
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(MS1, OUTPUT);
  pinMode(MS1, OUTPUT);

  digitalWrite(TMC_EN, HIGH); //High to disable motor driver on startup
  digitalWrite(MS1, HIGH); //Microstep resolution configuration (internal pull-down resistors: MS2, MS1: 00: 1/8, 01: 1/32, 10: 1/64 11: 1/16
  digitalWrite(MS2, LOW);

  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);


  //MQTT topics defines
  MQTTTopic += String(motor_id); //combines the name of the motor with its ID to make a unique device on MQTT
  sendTopic = MQTTTopic + "/Response"; //A subtopic for sending feedback on the motor's status
  recieveTopic = MQTTTopic + "/Motor"; //A subtopic the motor will listen to 

  //Start preparation code
  delay(500); //delay needed before "Serial.begin" to ensure bootloader mode entered correctly.
  Serial.begin(115200);
  Serial.println("Code Starting");
  setup_wifi(); //Connect to WiFi
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback); //Sets callback function when something arrives on the topic we have subscribed to
  reconnect(); //Connect to the MQTT server
  
}

void loop() {

  if (!client.connected()) { //check if MQTT connection is still good
    reconnect(); //recoonect if connection is bad
    digitalWrite(LED2, LOW); //Show MQTT bad status by turning off the corrosponding LED
    delay(1000);
  }
  else{
    digitalWrite(LED2, HIGH); //Show MQTT good status by turning on the corrosponding LED
  }

  if(WiFi.status() != WL_CONNECTED){ //check if WiFi connection is still good
    digitalWrite(LED1, LOW); //Show WiFi bad status by turning off the corrosponding LED
  }
  else{
    digitalWrite(LED1, HIGH); //Show WiFi good status by turning on the corrosponding LED
  }

  client.loop(); //MQTT loop


  if(runMotor){
    digitalWrite(STEP, HIGH);
    delayMicroseconds(delays[Speed]);
    digitalWrite(STEP, LOW);
    delayMicroseconds(delays[Speed]);
  }
}

void doSteps (int DIRECTION, int STEPS, int STEPSPEED){
  if(DIRECTION == 1 || DIRECTION == 0){
    if(STEPSPEED > 0 && STEPSPEED <= 10){
      if(STEPS > 0){
        String messageToSend = "Doing movement: " + String(STEPS) + " steps at speed " + String(STEPSPEED);
        client.publish(sendTopic.c_str(),  messageToSend.c_str());
        digitalWrite(TMC_EN, LOW); //Enable stepper motor driver

        if (DIRECTION == 1){ //set direction
            digitalWrite(DIR, HIGH);
          } else {
            digitalWrite(DIR, LOW);
          }
        for (int i = 0; i < STEPS; i++){
          digitalWrite(STEP, HIGH);
          delayMicroseconds(delays[STEPSPEED]);
          digitalWrite(STEP, LOW);
          delayMicroseconds(delays[STEPSPEED]);
          }
        client.publish(sendTopic.c_str(),  "Movement complete");
        digitalWrite(TMC_EN, HIGH); //Disable stepper motor driver
        }
      else{
        client.publish(sendTopic.c_str(),  "Error in input: invalid steps, should be greater than 0");
        Serial.println("Error in input: invalid steps, should be greater than 0");
      }
    }
    else{
      client.publish(sendTopic.c_str(),  "Error in input: invalid speed (Range 1-10)");
      Serial.println("Error in input: invalid speed (Range 1-10)");
    }
  }
  else{
    client.publish(sendTopic.c_str(),  "Error in input: invalid direction, expected 1 (forward) or 0 (reverse)");
    Serial.println("Error in input: invalid direction, expected 1 (forward) or 0 (reverse)");
  }

}
