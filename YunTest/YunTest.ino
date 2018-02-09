/*

 Arduino GroveStreams Stream Feed via Ethernet

 The GroveStreams client sketch is designed for the Arduino YUN.
 A full "how to" guide for this sketh can be found at https://www.grovestreams.com/developers/getting_started_arduino_yun_control.html
 This sketch updates several stream feeds with an analog input reading,
 from a temperature probe, via the GroveStreams API: https://www.grovestreams.com/developers/api.html
 It also checks for LED on/off commands, a sampleFrequency setting, and takes appropriate action(s).
 Use the Serial Monitor on the Arduino IDE to see verbose network feedback
 and the GroveStreams connectivity status.

 License:
  Copyright 2015 GroveStreams LLC.
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at: http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

 GroveStreams Setup:

 * Sign Up for Free User Account - https://www.grovestreams.com
 * Create a GroveStreams organization while selecting the Arduino blueprint
 * Enter a unique MAC Address for this network in this sketch under "Local Network Settings"
 *    Navigate to http://arduino.local/ to find your YUN's MAC address
 * Enter your GroveStreams secret api key under "GroveStreams Settings" in this sketch
 *    (The api key can be retrieved from within a GroveStreams organization: click the Api Keys toolbar button,
 *     select your Api Key, and click View Secret Key. Paste the Secret Key below)

 Arduino Requirements:

 * Arduino YUN
 * Arduino 1.5.8 IDE
 * ArduinoJson.h: https://github.com/bblanchon/ArduinoJson/wiki
*/

#include <Bridge.h>
#include <Process.h>
#include <ArduinoJson.h>


// Local Network Settings
char mac[] = "90:A2:DA:F5:01:C7"; // Change this!!! Must be unique on local network.
// Browse to your YUN to find it: http://arduino.local/

// GroveStreams Settings
char gsApiKey[] = "YOUR_SECRET_API_KEY_HERE";   //Change This!!!
char gsComponentName[] = "Temperature";        //Optionally change. Set this to give your component a name when it initially registers.

char gsComponentTemplateId[] = "temp";  //Don't change. Tells GS what template to use when the feed initially arrives and a new component needs to be created.
// The blueprint is expecting "temp".

//GroveStreams Stream IDs. Stream IDs tell GroveStreams which component streams the values will be assigned to.
//Don't change these unless you edit your GroveStreams component definition and change the stream IDs to match these.
char gsStreamId1[] = "s1";   //Don't change. Temp C.
char gsStreamId2[] = "s2";   //Don't change. Temp F.

// Other Settings
unsigned long updateFrequency = 10000UL;    // Update frequency in milliseconds (20000 = 20 seconds). Change this to change your sample frequency.

const int temperaturePin = 0;  // You might need to change depending on the Pin you are using. The Temperature pin number.
const int ledPin = 13;                 // You might need to change depending on the Pin you are using. The LED pin number.

char samples[40];                   // Change this buffer size only if you increase or decrease the size of samples being uploaded.

unsigned long lastUploadTime = 0; //Don't change. Used to determine if samples need to be uploaded.


void setup() {

  Bridge.begin();

  Serial.begin(9600);
  while (!Serial);

  //Wait for the connection to finish stopping
  delay(2500);

  //Set the mac and ip variables so that they can be used during sensor uploads later
  Serial.print("MAC: ");
  Serial.println(mac);
  Serial.println();

  pinMode(ledPin, OUTPUT);
}

void loop() {

  // Update sensor data to GroveStreams
  if (millis() - lastUploadTime > updateFrequency)
  {
    updateGroveStreams();
  }

}

void updateGroveStreams() {
  Serial.println("updateGroveStreams...");

  //Assemble the url that is used to pass the temperature readings to GroveStreams; Include request stream IDs
  lastUploadTime = millis();

  int urlBufSize = 210;
  char urlBuf[urlBufSize];

  //Include the rsid (request stream ID) parameter for each stream's last_value to be returned. Below we are requesting the
  // sample frequency and the led indicator stream last_values
  sprintf(urlBuf, "http://grovestreams.com/api/feed?compTmplId=%s&compId=%s&compName=%s&rsid=freq&rsid=led&api_key=%s%s",
          gsComponentTemplateId, mac, gsComponentName, gsApiKey, getSamples());

  char xHeadBuf[40];
  sprintf(xHeadBuf, "X-Forwarded-For:%s", mac);

  //Uncomment the following lines to debug your strings and to ensure their buffers are large enough
  //Serial.println(strlen(xHeadBuf));
  //Serial.println(xHeadBuf);
  //Serial.println(urlBuf);
  //Serial.println(strlen(urlBuf));
  //Serial.println(getSamples());
  //Serial.println(strlen(getSamples()));

  if (strlen(urlBuf) > urlBufSize) {
    Serial.println("urlBuf string buffer too small! Increase urlBufSize.");
  }

  Process process;
  process.begin("curl");
  process.addParameter("-d");
  process.addParameter("");
  process.addParameter("-k");
  process.addParameter("-X");
  process.addParameter("PUT");

  //Headers
  process.addParameter("-H");
  process.addParameter(xHeadBuf);
  process.addParameter("-H");
  process.addParameter("Connection:close");
  process.addParameter("-H");
  process.addParameter("Content-Type:application/json");

  //URL
  process.addParameter(urlBuf);

  //Make the request
  unsigned int result = process.run();

  //Display whatever is returned (usually an error message or requested return streams)
  char response[50] = {0}; //Initialize buffer to nulls;
  int i = 0;
  while (process.available() > 0) {
    char c = process.read();
    if (i < 50) {
      response[i++] = c;
    } else if (i == 50) {
      Serial.println("response string buffer too small!");
    }
    Serial.print(c);
  }
  Serial.flush();

  Serial.print("\nFeed PUT Response Code: ");
  Serial.println(result);
  Serial.print("Feed PUT Response Body: ");
  Serial.println(response);

  if (strlen(response) == 0) {
    return;
  }

  //Extract Return Stream last_values from the JSON returned
  StaticJsonBuffer<200> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(response);
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }

  if (root.containsKey("freq") && root["freq"] >= 10000) {
    //Set the update Frequency
    updateFrequency = root["freq"];
  }

  if (root.containsKey("led")) {
    bool ledOn = (bool)root["led"];

    //Handle led command
    if (ledOn && digitalRead(ledPin) == LOW) {
      digitalWrite(ledPin, HIGH);
    } else if (!ledOn && digitalRead(ledPin) == HIGH) {
      digitalWrite(ledPin, LOW);
    }

  }

  Serial.println("");
}

char* getSamples() {
  //Get the temperature analog reading and convert it to a string

  float voltage, degreesC, degreesF;

  voltage = (analogRead(temperaturePin) * 0.004882814);
  degreesC = (voltage - 0.5) * 100.0;
  degreesF = degreesC * (9.0 / 5.0) + 32.0;

  char tempC[15] = {0}; //Initialize buffer to nulls
  dtostrf(degreesC, 12, 3, tempC); //Convert float to string

  char tempF[15] = {0}; //Initialize buffer to nulls
  dtostrf(degreesF, 12, 3, tempF); //Convert float to string

  // Example: &s1=25.684&s2=78.231
  sprintf(samples, "&%s=%s&%s=%s", gsStreamId1, trim(tempC), gsStreamId2, trim(tempF));

  return samples;
}

char* trim(char* input) {
  //Trim leading and ending spaces

  int i, j;
  char *output = input;
  for (i = 0, j = 0; i < strlen(input); i++, j++)
  {
    if (input[i] != ' ')
      output[j] = input[i];
    else
      j--;
  }
  output[j] = 0;

  return output;
}