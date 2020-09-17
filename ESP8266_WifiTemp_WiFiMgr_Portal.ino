// -----------------------
// ALERT!!!
//   When programming, DISCONNECT the "WAKE" pin (4th from bottom when USB port facing away, right side up) from RST pin (top left, same orientation).
//
//   Ground Pin 14 to force a reset of the portal configuration.
//   Ground Pin 4 to force the Portal to be used forever, do not use the default.
// -----------------------

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <ESP8266TempSensor_WriteParams.h>

#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const int portalWait = 15;           // interval at which to run (in seconds)

//NOTE:  Paho PubSub seems to use a blocking Publish
//TODO:  determine if RF power can be adjusted to save more battery

#define ONE_WIRE_BUS 5  // Data wire is plugged into pin 2 on the Arduino
#define FORCEWIFIPORTAL_PIN 4  // Normally HIGH (powered).  When LOW (grounded), force portal to start


// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


// ----- Configuration -------
const float sleep_minutes = 10;
const int sleep_seconds = ((60 * sleep_minutes) + 15) ;  // Just a bit too short.  Wakes up early...
int iterCount;
ConfigsStruct configs ;
// ---------------------------

WiFiClient espClient;
PubSubClient client(espClient);


//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void setup_wifi() {
  bool forcePortal = false;

  // id/name placeholder/prompt default length
  char s_iterCount[5] ;
    s_iterCount[0] = '\0' ;
    sprintf( s_iterCount, "%d", iterCount ) ;
  char s_mqtt_port[6] ;
    s_mqtt_port[0] = '\0' ;
    sprintf( s_mqtt_port, "%d", configs.mqtt_port ) ;

  if( digitalRead(FORCEWIFIPORTAL_PIN) == LOW ) {
    //Force config portal
    forcePortal = true;
  }
    
  // We start by connecting to a WiFi network
  WiFiManager wifiManager;

  //set minimu quality of signal so it ignores AP's under that quality.  Defaults to 8%
  wifiManager.setMinimumSignalQuality();
  
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  WiFiManagerParameter custom_itercount("itercount", "Iteration Count", s_iterCount, 4);
  WiFiManagerParameter custom_client_name("mqtt_clientname", "mqtt client name", configs.mqtt_clientname, 40);
  WiFiManagerParameter custom_mqtt_topic("mqtt_topic", "mqtt topic", configs.mqtt_topic, 25);
  WiFiManagerParameter custom_mqtt_host("mqtt_host", "mqtt server name", configs.mqtt_server, 25);
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "mqtt port", s_mqtt_port, 5);
   
  wifiManager.addParameter(&custom_itercount);
  wifiManager.addParameter(&custom_client_name);
  wifiManager.addParameter(&custom_mqtt_topic);
  wifiManager.addParameter(&custom_mqtt_host);
  wifiManager.addParameter(&custom_mqtt_port);
 
  if( forcePortal ) {  
    wifiManager.setConfigPortalTimeout( 0 );  // Infinite wait
    Serial.println("WiFi Connection - Config Portal forced on"); 
    wifiManager.startConfigPortal() ;
    Serial.println("Finished the manually-forced portal");

    if( shouldSaveConfig ) {
      // Pull back the config values from the Portal
      String tempString ;
      tempString = custom_itercount.getValue() ;
        iterCount = tempString.toInt() ;
      strcpy(configs.mqtt_clientname, custom_client_name.getValue()) ;
      strcpy(configs.mqtt_topic, custom_mqtt_topic.getValue()) ;
      strcpy(configs.mqtt_server, custom_mqtt_host.getValue()) ;
      tempString = custom_mqtt_port.getValue() ;
        configs.mqtt_port = tempString.toInt() ;
    
      Serial.println("Values back from the config portal") ;
      Serial.print( "iter:  " ); Serial.println(iterCount);
      Serial.print( "client:" ); Serial.println(configs.mqtt_clientname);
      Serial.print( "topic: " ); Serial.println(configs.mqtt_topic);
      Serial.print( "host:  " ); Serial.println(configs.mqtt_server);
      Serial.print( "port:  " ); Serial.println(configs.mqtt_port);
  
      shouldSaveConfig = false ;  //  Saved; reset flag
      Serial.println("Config changed!  We need to save the configuration.") ;
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
        // Pull back the config values from the Portal
        String tempString ;
        tempString = custom_itercount.getValue() ;
          iterCount = tempString.toInt() ;
        strcpy(configs.mqtt_clientname, custom_client_name.getValue()) ;
        strcpy(configs.mqtt_topic, custom_mqtt_topic.getValue()) ;
        strcpy(configs.mqtt_server, custom_mqtt_host.getValue()) ;
        tempString = custom_mqtt_port.getValue() ;
          configs.mqtt_port = tempString.toInt() ;
      
        Serial.println("Values back from the config portal") ;
        Serial.print( "iter:  " ); Serial.println(iterCount);
        Serial.print( "client:" ); Serial.println(configs.mqtt_clientname);
        Serial.print( "topic: " ); Serial.println(configs.mqtt_topic);
        Serial.print( "host:  " ); Serial.println(configs.mqtt_server);
        Serial.print( "port:  " ); Serial.println(configs.mqtt_port);
      
        Serial.println("Config changed!  We need to save the configuration.") ;
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
      Serial.println(" try again in 1 seconds");
      delay(1000);   // reduced from 5 seconds to save battery
    }
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(FORCEWIFIPORTAL_PIN, INPUT_PULLUP) ;

  readConfigParams( iterCount, configs ) ;
  printConfigParams( iterCount, configs ) ;
  pinMode(ONE_WIRE_BUS, INPUT);

  setup_wifi(); 
  client.setServer(configs.mqtt_server, configs.mqtt_port);

  sensors.begin();
  sensors.setResolution(11);   // max = 12
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

  sprintf(outBuffer, "%2X,%03d,%04u,%04u,%04u",
    configs.nodeid & 0xff,
    iterCount,
    (int16_t)(temp1*10),
    (int16_t)(temp2*10),
    (int16_t)(temp3*10)
    );

  if (!client.connected()) {
    reconnectMQTT();
  }

  Serial.printf("outBuffer: %s len: %d \n",outBuffer, strlen(outBuffer));

  client.publish(configs.mqtt_topic, outBuffer, FALSE);  // Do not send as 'Retained'
  client.loop();
  client.disconnect();
  Serial.print("Message Published") ;
   
  // Write out config parameters
  writeIterationCountParam( iterCount ) ;

  // Resets completely when waking.  ie: Will re-call Setup then Loop
  ESP.deepSleep(sleep_seconds * 1000000, WAKE_RF_DEFAULT);
  //delay(sleep_seconds * 1000) ; 
  //ESP.reset() ;
}
