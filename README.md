# ESP8266_TempMonWithWifiPortal
Monitor temperatures from an Adafruit ESP8266 using persistent values, managed through a WiFiManager portal

Uses an Adafruit Feather Huzzah
Reads DS18B20 OneWire temperature sensors
Publishes temperature to an MQTT queue
Goes to a long sleep; wakes by resetting the module completely (Re-runs Setup, then Loop)

Configured using a WiFiManager portal solution.
My params are added to the SSID/Password and all managed through their portal
Portal launches when WiFi cannot connect, or...
Force the portal to launch by pulling a hard-coded pin low.

Benefits:
Extremely power-efficient
Fast to come up, connect, do its work, go back to sleep
Portal allows easy configuration (and reading) of all values used by the sketch

TODO:
* Finish code for persisting the updated values (Just call the "Write" method?)
* Figure out where WiFiManager writes its parameters such that I'm not overwriting then with mine. (why does it work?)
* Externalize more of the hard-codes into parameters.
* Confirm flexibility such that I can fire up a new node from scratch, using the Portal to configure everything, providing "defaults" for missing the values, not hard-coding them.
