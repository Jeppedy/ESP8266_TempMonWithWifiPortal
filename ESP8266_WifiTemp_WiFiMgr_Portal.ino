// -----------------------
// ALERT!!!
//   When programming, DISCONNECT the "WAKE" pin (4th from bottom when USB port facing away, right side up) from RST pin (top left, same orientation).
//
//   Ground Pin 4 to force the Portal to be launched
// -----------------------

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "FS.h"
#include <ArduinoJson.h>

#define ONE_WIRE_BUS 5  // Data wire is plugged into pin 2 on the Arduino
#define FORCEWIFIPORTAL_PIN 4  // Normally HIGH (powered).  When LOW (grounded), force portal to start

//Global flags
volatile bool shouldSaveConfig = false;  // flag for saving data
volatile bool forcePortal = false;  // When true, we force the portal to launch.

const char* CONFIG_FILENAME= "/parameters.json" ;
const char* ITER_FILENAME= "/lastseqnumber.json" ;
#define CONFIG_MAXSIZE 512  // Yeah, it's overly large.  So what...
#define ITER_MAXSIZE   128  // Yeah, it's overly large.  So what...

// ----- Configuration -------
int iterCount;

struct ConfigsStruct {
  int   sleeptimer_mins;
  char  nodeid[2+1];
  char  mqtt_clientname[30] ;
  char  mqtt_topic[20] ;
  char  mqtt_server[40] ;
  int   mqtt_port ;
} ;
ConfigsStruct configs ;

// Global Instantiations

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

WiFiClient espClient;
PubSubClient client(espClient);

// ---- Program Begins ----

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void setup_wifi() {
  // id/name placeholder/prompt default length
  char s_iterCount[5] ;
    sprintf( s_iterCount, "%d", iterCount ) ;
  char s_mqtt_port[6] ;
    sprintf( s_mqtt_port, "%d", configs.mqtt_port ) ;
  char s_sleeptimer[2+1] ;
    sprintf( s_sleeptimer, "%d", configs.sleeptimer_mins ) ;

  // We start by connecting to a WiFi network
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  
  // Use an interrupt to allow a button press to force into Wifi Portal mode.
  // https://thekurks.net/blog/2016/4/25/using-interrupts
  
  wifiManager.setBreakAfterConfig( true ) ; // Odd, I know, but this means "Save params even when the Wifi part of the config isn't specified".
  wifiManager.setMinimumSignalQuality();  //set minimum quality of signal so it ignores AP's under that quality.  Defaults to 8%
  wifiManager.setSaveConfigCallback(saveConfigCallback);  //set config save notify callback

  //add all custom portal parameters here
  WiFiManagerParameter custom_itercount("itercount", "Iteration Count", s_iterCount, 3);
  WiFiManagerParameter custom_sleeptimer("sleeptimer_mins", "Sleep Timer (in Mins)", s_sleeptimer, 2);
  WiFiManagerParameter custom_nodeid("nodeid", "Node ID", configs.nodeid, 2);
  WiFiManagerParameter custom_client_name("mqtt_clientname", "mqtt client name", configs.mqtt_clientname, 40);
  WiFiManagerParameter custom_mqtt_topic("mqtt_topic", "mqtt topic", configs.mqtt_topic, 25);
  WiFiManagerParameter custom_mqtt_host("mqtt_host", "mqtt server name", configs.mqtt_server, 25);
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "mqtt port", s_mqtt_port, 5);
   
  wifiManager.addParameter(&custom_itercount);
  wifiManager.addParameter(&custom_sleeptimer);
  wifiManager.addParameter(&custom_nodeid);
  wifiManager.addParameter(&custom_client_name);
  wifiManager.addParameter(&custom_mqtt_topic);
  wifiManager.addParameter(&custom_mqtt_host);
  wifiManager.addParameter(&custom_mqtt_port);
 
  if( forcePortal ) {  
    forcePortal = false ;
    
    wifiManager.setConfigPortalTimeout( 0 );  // Infinite wait
    Serial.println("WiFi Connection - Config Portal forced on"); 
    wifiManager.startConfigPortal() ;
    Serial.println("Finished the manually-forced portal");

    if( shouldSaveConfig ) {
      Serial.println("Config changed!  We need to save the configuration.") ;
      // Pull back the config values from the Portal
      String tempString ;
      tempString = custom_itercount.getValue() ;
        iterCount = tempString.toInt() ;
      tempString = custom_sleeptimer.getValue() ;
        configs.sleeptimer_mins = tempString.toInt() ;
        
      strcpy(configs.nodeid, custom_nodeid.getValue()) ;
      strcpy(configs.mqtt_clientname, custom_client_name.getValue()) ;
      strcpy(configs.mqtt_topic, custom_mqtt_topic.getValue()) ;
      strcpy(configs.mqtt_server, custom_mqtt_host.getValue()) ;
      tempString = custom_mqtt_port.getValue() ;
        configs.mqtt_port = tempString.toInt() ;
    
      Serial.println("Values back from the config portal") ;
      printConfigParams( iterCount, configs ) ;
      // Write Config Params to "disk"
      writeConfigParamsToFS(CONFIG_FILENAME, configs) ;
      writeIterationCountParamToFS( ITER_FILENAME, iterCount) ;
      shouldSaveConfig = false ;  //  Saved; reset flag
    }
    else {
      Serial.println("No change to configs...  No save required.") ;
    }
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    if (!wifiManager.autoConnect()) {
      Serial.println("Failed to auto-connect to a WiFi AP; Reached timeout.  Resetting.");
      ESP.reset();
    } else {
       if( shouldSaveConfig ) {
        Serial.println("Config changed!  We need to save the configuration.") ;

        // Pull back the config values from the Portal
        String tempString ;
        tempString = custom_itercount.getValue() ;
          iterCount = tempString.toInt() ;
        tempString = custom_sleeptimer.getValue() ;
          configs.sleeptimer_mins = tempString.toInt() ;
        strcpy(configs.nodeid, custom_nodeid.getValue()) ;
        strcpy(configs.mqtt_clientname, custom_client_name.getValue()) ;
        strcpy(configs.mqtt_topic, custom_mqtt_topic.getValue()) ;
        strcpy(configs.mqtt_server, custom_mqtt_host.getValue()) ;
        tempString = custom_mqtt_port.getValue() ;
          configs.mqtt_port = tempString.toInt() ;
      
        Serial.println("Values back from the config portal") ;
        printConfigParams( iterCount, configs ) ;
        // Write Config Params to "disk"
        writeConfigParamsToFS(CONFIG_FILENAME, configs) ;
        writeIterationCountParamToFS( ITER_FILENAME, iterCount) ;
      }
      else {
        Serial.println("No change to configs...  No save required.") ;
      } 
    }
  } else {
    Serial.println("After Portal: Already connected to WiFi.");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("After Portal: Connected to WiFi.");
    Serial.print("  IP Address is: ");
    Serial.println(WiFi.localIP()); 
  } else {
    Serial.println("After everything, still not connected to Wifi.  Restarting to attempt default Wifi connection.");
    ESP.reset() ;
  }
}

void reconnectMQTT() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    if (client.connect(configs.mqtt_clientname)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in .5 seconds");
      delay(500);   
    }
  }
}

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
  }
  return result;
}

// Loads the configuration from a file
bool readConfigParamsfromFS(const char *filename, ConfigsStruct& cfg) {
  // Open file for reading
  File file = SPIFFS.open(filename, "r");

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<CONFIG_MAXSIZE> configdoc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(configdoc, file);
  if (error)
    Serial.println("Failed to read config file, defaults will be used");

  uint8_t mac[6];
  String clientName ;
  WiFi.macAddress(mac);
  clientName = "tempsensor-" ;
  clientName += macToStr(mac) ;

  // Copy values from the JsonDocument to the Config
  strlcpy(cfg.nodeid,                  // <- destination
          configdoc["nodeid"] | "XX",  // <- source
          sizeof(cfg.nodeid));         // <- destination's capacity
  strlcpy(cfg.mqtt_clientname,                  // <- destination
          configdoc["mqtt_clientname"] | clientName.c_str(),  // <- source
          sizeof(cfg.mqtt_clientname));         // <- destination's capacity
  strlcpy(cfg.mqtt_topic,                  // <- destination
          configdoc["mqtt_topic"] | "temperatures",  // <- source
          sizeof(cfg.mqtt_topic));         // <- destination's capacity
  strlcpy(cfg.mqtt_server,                  // <- destination
          configdoc["mqtt_server"] | "192.168.2.128",  // <- source
          sizeof(cfg.mqtt_server));         // <- destination's capacity
  cfg.mqtt_port = configdoc["mqtt_port"] | 1883;
  cfg.sleeptimer_mins = configdoc["sleeptimer"] | 10;
  
  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
}

// Loads the configuration from a file
bool readIterCountfromFS(const char *filename, int& count) {
  // Open file for reading
  File file = SPIFFS.open(filename, "r");

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<ITER_MAXSIZE> configdoc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(configdoc, file);
  if (error) {
    Serial.println("Failed to read config file, defaults will be used");
  }
  // Copy values from the JsonDocument to the Config
  count = configdoc["lastseq"] | 0;
  
  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
}

void printConfigParams( int& iterCount, ConfigsStruct& cfg) {
  printf("\n") ;
  Serial.println( "Configuration: " );
  printf("Sleep Timer [%d]\n", cfg.sleeptimer_mins);
  printf("NodeID [%s]\n", cfg.nodeid);
  printf("Client name [%s]\n", cfg.mqtt_clientname);
  printf("MQ Topic [%s]\n", cfg.mqtt_topic);
  printf("MQ Host [%s]\n", cfg.mqtt_server);
  printf("MQ Port [%d]\n", cfg.mqtt_port);

  printf("iterCount [%d]\n", iterCount);
}


void writeIterationCountParamToFS( const char *filename, const int& count) {
  // Open file for writing
  File file = SPIFFS.open(filename, "w");
  if (!file) {
    Serial.println("Failed to create Iteration Count file");
    return;
  }

  StaticJsonDocument<ITER_MAXSIZE> doc;  // Allocate a temporary JsonDocument

  // Set the values in the document
  doc["lastseq"] = count;

  // Serialize JSON to file
  if (serializeJsonPretty(doc, file) == 0) {
    Serial.println("Failed to write to file");
  }

  file.close();
}

void writeConfigParamsToFS( const char *filename, ConfigsStruct& cfg) {
  // Open file for writing
  File file = SPIFFS.open(filename, "w");
  if (!file) {
    Serial.println("Failed to update Params Config file");
    return;
  }

  StaticJsonDocument<CONFIG_MAXSIZE> doc;  // Allocate a temporary JsonDocument

  // Set the values in the document
  doc["nodeid"] = cfg.nodeid;
  doc["sleeptimer"] = cfg.sleeptimer_mins;
  doc["mqtt_clientname"] = cfg.mqtt_clientname;
  doc["mqtt_topic"] = cfg.mqtt_topic ;
  doc["mqtt_server"] = cfg.mqtt_server;
  doc["mqtt_port"] = cfg.mqtt_port ;

  serializeJsonPretty(doc, Serial ) ;  // Show what's being written by sending to Serial

  // Serialize JSON to file
  if (serializeJsonPretty(doc, file) == 0) {
    Serial.println("Failed to write to file");
  }

  file.close();
}

// Prints the content of a file to the Serial
void listDirectory() {
  Serial.println("--Directory Listing --");
  Dir dir = SPIFFS.openDir ("");
  while (dir.next ()) {
    Serial.print (dir.fileName ());
    Serial.print("\t") ;
    Serial.println (dir.fileSize ());
  }
  Serial.println("------");
}
    
// Prints the content of a file to the Serial
void printFile(const char *filename) {
  Serial.print("--Listing file contents for: ");
  Serial.print(filename) ;
  Serial.println("----");

  // Open file for reading
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.print("Failed to read file: ");
    Serial.println(filename) ;
    return;
  }

  // Extract each character one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();

  file.close();
  Serial.println("------");
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  
  pinMode(FORCEWIFIPORTAL_PIN, INPUT_PULLUP) ;
  pinMode(ONE_WIRE_BUS, INPUT);

//  attachInterrupt(digitalPinToInterrupt(FORCEWIFIPORTAL_PIN), onDemandPortal, CHANGE);
  if( digitalRead(FORCEWIFIPORTAL_PIN) == LOW )
    forcePortal = true;  

  if (SPIFFS.begin()) {
      Serial.println("SPIFFS Active");
  } else {
      Serial.println("Unable to activate SPIFFS...  Going Comatose!!");
      ESP.deepSleep((60 * 60) * 1000000, WAKE_RF_DEFAULT);
      return ;
  }

//  listDirectory() ;
//  printFile(CONFIG_FILENAME) ;
//  printFile(ITER_FILENAME) ;
  readConfigParamsfromFS( CONFIG_FILENAME, configs ) ;
  readIterCountfromFS( ITER_FILENAME, iterCount ) ;
  printConfigParams( iterCount, configs ) ;

  // Initialize Temperature Sensors
  sensors.begin();
  sensors.setResolution(11);   // max = 12

  // Initialize WiFi
  setup_wifi(); 

  // Initialize MQTT Client
  client.setServer(configs.mqtt_server, configs.mqtt_port);
}

void loop()
{
  float temp1=999;
  float temp2=999;
  float temp3=999;
  char outBuffer[32] ; 

  sensors.requestTemperatures(); // Send the command to get temperatures

  temp1 = sensors.getTempFByIndex(0);  // Garage
  Serial.print("Temp1: "); Serial.print(temp1); Serial.println("F");
//  temp2 = sensors.getTempFByIndex(0);  // Freezer
//  temp3 = battery_level(); //Voltage

  if ( ++iterCount > 999 ) iterCount = 0;

  sprintf(outBuffer, "%2.2s,%03d,%04u,%04u,%04u",
    configs.nodeid,
    iterCount,
    (int16_t)(temp1*10),
    (int16_t)(temp2*10),
    (int16_t)(temp3*10)
    );

  Serial.printf("outBuffer: %s len: %d \n",outBuffer, strlen(outBuffer));

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.publish(configs.mqtt_topic, outBuffer, FALSE);  // Do not send as 'Retained'
  client.loop();
  client.disconnect();
  Serial.print("Message Published") ;
   
  // Write out latest iterator
  writeIterationCountParamToFS( ITER_FILENAME, iterCount ) ;

  // Resets completely when waking.  ie: Will re-call Setup then Loop
  ESP.deepSleep(((60 * configs.sleeptimer_mins) + 15) * 1000000, WAKE_RF_DEFAULT);  // Adding an extra 15 seconds, as it wakes up just a bit early...
}
