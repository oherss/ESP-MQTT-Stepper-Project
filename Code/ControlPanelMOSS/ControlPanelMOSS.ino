#include <ESP32RotaryEncoder.h>
#include "avdweb_Switch.h"
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
const int motor_id = 2;

/////////////////////////
//     Pin Defines     //
/////////////////////////

const byte toggleSwitchpin = 21;
const byte buttonPin = 16;
const byte LEDPin = 4;
const uint8_t DI_ENCODER_A   = 23;
const uint8_t DI_ENCODER_B   = 22;
const int8_t  DI_ENCODER_SW  = 12;
const int8_t  DO_ENCODER_VCC = 13;

/////////////////////////////
// Global Variable Defines //
/////////////////////////////

//MQTT topic
String MQTTTopic = "ControlPanel";
String MQTTTopicAnnounce = "Pairing";

//MQTT subtopics for specific motor ID
String sendTopic;
String recieveTopic;
String clientTopic;
String serverTopic;

//combined MQTT message
String MQTTmsg = "";
String DeviceMessage;

//LED state
bool LEDSTATE = false;

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

Switch toggleSwitch = Switch(toggleSwitchpin);
Switch Button = Switch(buttonPin);
RotaryEncoder rotaryEncoder( DI_ENCODER_A, DI_ENCODER_B, DI_ENCODER_SW, DO_ENCODER_VCC );


void buttonCallbackFunction(void* s)
{
    client.publish(sendTopic.c_str(),  "{ComponentType: Button, ID: 1, Action: Clicked}");
    Serial.print("Button: ");
    Serial.println((char*)s);
  client.publish("Star3/Command", "Shoot star :3");
}

void toggleCallbackFunction(void* s)
{
  client.publish(sendTopic.c_str(),  "{ComponentType: Toggle, ID: 1, Action: StateChange}");
  Serial.print("Toggle: ");
  Serial.println((char*)s);
}

void knobCallback( long value )
{
  client.publish(sendTopic.c_str(),  ("{ComponentType: Knob, ID: 1, Action: Turn, Direction: " + String(value) + "}").c_str());
	Serial.printf( "Direction: %ld\n", value );
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  //WiFi connection established
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) { //show connection status for WiFi in console
    delay(250);
    Serial.print(".");
  }

  //Show that WiFi connection was sucessful via serial, and show the current IP adress
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) { //Runs whenever we receive a message on the recive topic
  //recieve message and print topic to serial monitor
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

  if(String(topic) == recieveTopic){ //If recieved a command, simply toggle LED status
    LEDSTATE = !LEDSTATE;
    digitalWrite(LEDPin, LEDSTATE);
  }
  else{ //If recieved mesage in pairing topic, send device info
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
      client.publish(sendTopic.c_str(), "Panel Online");
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

void setup()
{
  //Defines all inputs, as inputs
    pinMode(toggleSwitchpin, INPUT);
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(DI_ENCODER_A, INPUT);
    pinMode(DI_ENCODER_B, INPUT);
    pinMode(DI_ENCODER_SW, INPUT);
    pinMode(DO_ENCODER_VCC, INPUT);
    pinMode(LEDPin, OUTPUT);


  //MQTT topics defines
  MQTTTopic += String(motor_id); //combines the name of the motor with its ID to make a unique device on MQTT
  sendTopic = MQTTTopic + "/Response"; //A subtopic for sending feedback on the motor's status
  recieveTopic = MQTTTopic + "/Command"; //A subtopic the motor will listen to 
  clientTopic = MQTTTopicAnnounce + "/Client"; //A subtopic for sending information about the device upon bootup and upon request
  serverTopic = MQTTTopicAnnounce + "/Server"; //A subtopic the motor will listen to to send info about itself
  DeviceMessage = "{ResponseTopic: " + sendTopic + ", CommandTopic: " + recieveTopic + ", ID: " + motor_id + ", DeviceType: Control_Panel, Num_Knobs: 1, Num_Toggle: 1, Num_PushBtn: 1, Num_LED: 1}"; 
      

  //Start preparation code
  delay(500); //delay needed before "Serial.begin" to ensure bootloader mode entered correctly.
  Serial.begin(115200);
  Serial.println("Code Starting");
  setup_wifi(); //Connect to WiFi
  delay(10);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback); //Sets callback function when something arrives on the topic we have subscribed to
  reconnect(); //Connect to the MQTT server


  //Set up callback function for toggle switches
  toggleSwitch.setPushedCallback(&toggleCallbackFunction, (void*)"turned on");
  toggleSwitch.setReleasedCallback(&toggleCallbackFunction, (void*)"turned off");

  //Set up callback for push button
  Button.setSingleClickCallback(&buttonCallbackFunction, (void*)"single click");
	
  //Rotary encoder setup
  rotaryEncoder.setEncoderType( EncoderType::HAS_PULLUP ); 	// This tells the library that the encoder has its own pull-up resistors
	rotaryEncoder.setBoundaries( 0, 1, false ); //Set up rotary encoder to only send direction
	rotaryEncoder.onTurned( &knobCallback ); //define callback for rotary encoder

	rotaryEncoder.begin();// This is where the inputs are configured and the interrupts get attached
}

void loop()
{
   if (!client.connected()) { //check if MQTT connection is still good
    reconnect(); //recoonect if connection is bad
    delay(1000);
  }
  if(WiFi.status() != WL_CONNECTED){ //check if WiFi connection is still good
     setup_wifi(); 
  }

  client.loop(); //MQTT loop
  toggleSwitch.poll();
  Button.poll();
	// Your stuff here
}