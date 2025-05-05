/*
 MQTT connected stepper motor firmware developed for Simmoe by Oliver "Sproxxy" Herss March 2025 based on the PD stepper project.

 Make sure to have the ESP-32 boards in your IDE to build for the hardware.
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TMC2209.h>

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





//TMC2209 setup
TMC2209 stepper_driver;
HardwareSerial & serial_stream = Serial2;
const long SERIAL_BAUD_RATE = 115200;

const uint8_t RUN_CURRENT_PERCENT = 100; //how much current to run at (0-100%)

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


//MQTT topic
String MQTTTopic = "ESPStepper";
String MQTTTopicAnnounce = "Pairing";

//MQTT subtopics for specific motor ID
String sendTopic;
String recieveTopic;
String clientTopic;
String serverTopic;

//combined MQTT message
String MQTTmsg = "";
String DeviceMessage;

// Extracted JSON parameters from message
int Steps = 0;
int Speed = 0;
int Dir = 0;
int microSteps = 256; //Standard value

//WiFi
WiFiClient espClient;

//MQTT function variables
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];

void setup_wifi() {// Wifi setup
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  //WiFi connection established
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) { //Whie connecting blink LED to show status
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

void callback(char* topic, byte* payload, unsigned int length) { //Runs whenever we receive a message on the recive a message on subsribed topics
  //recieve message and print topic to serial monitor
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  //combines characters into full string
  MQTTmsg = ""; //clear the string
  for (int i = 0; i < length; i++) {
    MQTTmsg += (char)payload[i];
  }

  //prints received message to console
  Serial.println(MQTTmsg);

  if(String(topic) == recieveTopic){ //Checks if message is in the command topic
    StaticJsonDocument<200> doc;  // Define a JSON document with a suitable size
    DeserializationError error = deserializeJson(doc, MQTTmsg); //interpret JSON from received message

    if (error) { //Prints JSON error to console and via MQTT to user
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.f_str());
        String messageToSend = "Json parse failed: " + String(error.f_str());
        client.publish(sendTopic.c_str(),  messageToSend.c_str());
        return;
      }

    String StepString = doc["Steps"]; //Store the data in "Steps" as a string for comparison 

    if(StepString == "Run"){ //check if constant motion is requested
      Speed = doc["Speed"];
      Dir = doc["Dir"];
      microSteps = doc["MicroSteps"];

      if (Dir == 1){ //set direction
        digitalWrite(DIR, HIGH);
      } else {
        digitalWrite(DIR, LOW);
        Speed = -Speed;
      }

      String stringToSend = "Running motor at speed: " + String(Speed) + " at " + String(microSteps) + " microsteps";
      Speed *= 70; //Scale speed to value stepper driver can understand
      digitalWrite(TMC_EN, LOW);
      stepper_driver.setMicrostepsPerStep(microSteps);
      stepper_driver.enable();
      stepper_driver.moveAtVelocity(Speed*microSteps); //Starts motion
      Serial.println(stringToSend);
      client.publish(sendTopic.c_str(), stringToSend.c_str()); //Sends info back to user via MQTT
    }

    else if(StepString == "Stop"){ //Check if command is to stop constant motion
      //digitalWrite(TMC_EN, HIGH);
      stepper_driver.moveAtVelocity(0);
      stepper_driver.disable();
      Serial.println("Motor stopped");
      client.publish(sendTopic.c_str(),  "Motor stopped");
    }

    else{ //If not constant motion, intepret amount of steps requested, and run the doSteps() function
      Steps = doc["Steps"];  // Assuming the JSON contains a parameter named "Steps"
      Speed = doc["Speed"];  // Assuming the JSON contains a parameter named "Speed"
      Dir = doc["Dir"];  // Assuming the JSON contains a parameter named "Dir"
      microSteps = doc["MicroSteps"]; // Assuming the JSON contains a parameter named "MicroSteps"
      stepper_driver.setMicrostepsPerStep(microSteps); //Sets micro step resolution
      doSteps(Dir, Steps, Speed); //Runs function for moving the motor
    }

  }

  else{ //If recieved message is not a command, assume it is a pairing command, and send out device info
    Serial.println("Sending device info on topic: " + clientTopic);
    client.publish(clientTopic.c_str(), DeviceMessage.c_str());
  }
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
      Serial.print("Response topic: ");
      Serial.println(sendTopic);
      // ... and resubscribe
      client.subscribe(recieveTopic.c_str());
      client.subscribe(serverTopic.c_str());
      Serial.print("Subscribed to topic: ");
      Serial.println(recieveTopic);
      Serial.print("Subscribed to topic: ");
      Serial.println(serverTopic);
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
  pinMode(LED1, OUTPUT); //WiFi status LED
  pinMode(LED2, OUTPUT); //MQTT status LED
  
    //PD Trigger Setup  (Selects the voltage needed over the USB port, tín this case just 5 volts)
  pinMode(CFG1, OUTPUT);
  pinMode(CFG2, OUTPUT);
  pinMode(CFG3, OUTPUT);
                            //  5V   9V   12V   15V   20V  (Can also be changed on the fly)
  digitalWrite(CFG1, HIGH); //  1    0     0     0     0
  digitalWrite(CFG2, LOW);  //  -    0     0     1     1
  digitalWrite(CFG3, LOW);  //  -    0     1     1     0
  
   //Setup serial comms with TMC2209
  pinMode(MS1, OUTPUT); 
  pinMode(MS2, OUTPUT); 
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(TMC_EN, OUTPUT);
  pinMode(DIAG, INPUT); //Response data pin from stepper driver

  //Motor setup
  digitalWrite(TMC_EN, LOW); //Enabled here and later enabled/disabled over UART

  digitalWrite(MS1, LOW); //used to set serial address in UART mode
  digitalWrite(MS2, LOW);


  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);


  stepper_driver.setup(serial_stream, SERIAL_BAUD_RATE, TMC2209::SERIAL_ADDRESS_0, TMC_RX, TMC_TX); //Sets up serial communication with stepper driver from defnied values
  stepper_driver.setRunCurrent(RUN_CURRENT_PERCENT); // sets motor current to defined value at the top of the code
  stepper_driver.enableAutomaticCurrentScaling(); //current control mode
  stepper_driver.enableStealthChop(); //stealth chop needs to be enabled for stall detect
  stepper_driver.setStandstillMode(stepper_driver.NORMAL);
  stepper_driver.disable(); //Disable the motor 


  //MQTT topics defines
  MQTTTopic += String(motor_id); //combines the name of the motor with its ID to make a unique device on MQTT
  sendTopic = MQTTTopic + "/Response"; //A subtopic for sending feedback on the motor's status
  recieveTopic = MQTTTopic + "/Motor"; //A subtopic the motor will listen to 
  clientTopic = MQTTTopicAnnounce + "/Client"; //A subtopic for sending information about the device upon bootup and upon request
  serverTopic = MQTTTopicAnnounce + "/Server"; //A subtopic the motor will listen to to send info about itself to the client topic
  DeviceMessage = "{ResponseTopic: " + sendTopic + ", CommandTopic: " + recieveTopic + ", ID: " + motor_id + ", DeviceType: Stepper_Motor, MinSpeed: 1, MaxSpeed: 10, MaxMicroSteps: 256}"; //The message the device sends during pairing
  
  //Start preparation code
  delay(500); //delay needed before "Serial.begin" to ensure bootloader mode entered correctly.
  Serial.begin(115200); //Start serial connection for debugging at set baud rate
  Serial.println("Code Starting");
  setup_wifi(); //Connect to WiFi
  client.setServer(mqtt_server, mqtt_port); //Define MQTT server attributes
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
}

void doSteps (int DIRECTION, int STEPS, int STEPSPEED){
  STEPS *= microSteps;
  if(DIRECTION == 1 || DIRECTION == 0){ //Checks for valid direction
    if(STEPSPEED > 0 && STEPSPEED <= 10){ //checks for valid speed
      if(STEPS > 0){ //checks for valid amount of steps
        stepper_driver.enableCoolStep();
        stepper_driver.enable(); //enables motor
        int moveTime = ((STEPS * delays[STEPSPEED]) + (STEPS * delays[STEPSPEED])) / 1000000; //calculate the time it will take to complete any given motion from the parameters given
        String messageToSend = "{Steps: " + String(STEPS) + ", Speed: " + String(STEPSPEED) + ", TimeToFinish: " + String(moveTime) + ", Status: Running}"; //combined string with all the infor about requested motion and its status
        client.publish(sendTopic.c_str(),  messageToSend.c_str());

        if (DIRECTION == 1){ //set direction
            digitalWrite(DIR, HIGH);
          }
        else {
            digitalWrite(DIR, LOW);
          }

        for (int i = 0; i < STEPS; i++){ //For-loop for amount of steps to do
          digitalWrite(STEP, HIGH);
          delayMicroseconds(delays[STEPSPEED]);
          digitalWrite(STEP, LOW);
          delayMicroseconds(delays[STEPSPEED]);
        }

        messageToSend = "{Steps: " + String(STEPS) + ", Speed: " + String(STEPSPEED) + ", TimeToFinish: " + String(moveTime) + ", Status: Done}"; //combined string with all the infor about requested motion and its status
        client.publish(sendTopic.c_str(),  messageToSend.c_str());
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
  stepper_driver.disable();//disables motor
}
