#include <Arduino.h>
#include "../include/eloquent_esp32cam.h"
#include <WiFi.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <ESP32Servo.h>
#include <string.h>
#include <iostream>

#include <cmath>

using namespace std;


#define WIFI_SSID "Backbone"
#define WIFI_PASSWORD "GMUBackbone@123"
#define HOSTNAME  "esp32cam"


//#define HOST "192.168.1.127"
#define HOST "x-d-e3241-n05238.mesa.gmu.edu"
#define PORT 4000
#define MOTOR 14 //switch to 14
#define SERVO 13


#define LED_BUILTIN 4


WiFiClient client;

Servo brushlessMotor;
Servo servoMotor;
int x_1, y_1, x_2, y_2;
float closePWM;
float distanceFromCam;
long int timer,timerHolder,timerCount;
int servoCount = 0;
int servoCounter = 0;
long boxArea;


int frameCount = 0;
enum braitenbergStates{
  Start,
  slowDown,
  closeGate,
  idle,
};
void setup() {
    Serial.begin(115200);
    Serial.println("___Camera Streamer___");
    brushlessMotor.attach(MOTOR);
    servoMotor.attach(SERVO);
    servoMotor.write(0);
    distanceFromCam = 260.92;
    delay(2000); //Just added 
    init(); //Just added
    // camera settings
    // replace with your own model!
    eloq::camera.pinout.aithinker();
    eloq::camera.brownout.disable();
    // Edge Impulse models work on square images
    // face resolution is 240x240
    eloq::camera.resolution.face();
    eloq::camera.quality.high();
    Serial.println("hi");
    // init camera
    while (!eloq::camera.begin().isOk())
        Serial.println(eloq::camera.exception.toString());

    // connect to WiFi
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
  if(!client.connect(HOST, PORT)){
    Serial.println("Client not connected!");
    return;
  }
}

String objDetect(){
    
    if (!eloq::camera.capture().isOk()) {
        Serial.println(eloq::camera.exception.toString());
        return "";
    }
    client.print(eloq::camera.frame->len);
    client.write((const char *) eloq::camera.frame->buf, eloq::camera.frame->len);
    client.flush();
    String str = client.readStringUntil('#');
    return str;
}

int parseRecStr (const String inString, long *center, long* area){
  String inStr = inString ;
  long bounds[4];
  for (int i = 0; i < 4; i++) {
    int commaIndex = inStr.indexOf(',');
    bounds[i] = inStr.substring(0, commaIndex).toInt();
    inStr = inStr.substring(commaIndex + 2);
  }

  *area = abs(bounds[2] * bounds[3]);
  center[0] = bounds[0];
  center[1] = bounds[1];

  return 1;
}
braitenbergStates currState = Start; //Set initial state to start
void loop() {
    switch(currState){
      case Start:
        if(boxArea != 0){ //If some object is detected, switch to slowDown state
          currState = slowDown;
          timerHolder = millis(); //Make a record of the time in millis this program is in at this point
        }
      break;
      case slowDown:
        if(millis() - timerHolder > 10000){ //Keep comparing the difference in time until the current moment of time minus the timeHolder is greater than 10000 ms or 10 seconds
          currState = closeGate; //if this condition is true, proceed to close the gate
        }
      break;
      case closeGate:
        if(servoCount < 180){ //Once the angle we are writing the servo to is 180 
        // (Should take 0.02 * (180/5) seconds or 0.72 seconds but because of ObjDetection() call varies in returning from function, 
        // our desired closing gate time is only limited to a MINIMUM of 0.72 seconds, actual Closing Gate speed is much slower)
        // switch to the Idle state or the else state
          currState = closeGate;
        }
        else{
          currState = idle;
        }
      break;
      case idle: //Remain in idle and keep the gate closed and brushless motors off until tail agent finishes doing its thing
      // You could help the speed of the Braitenburh by allowing both Brushless motors to be on, I will prepare a code for you to uncomment if 
      // Brushless motors are to be on in this state if that is what is desired. Comment line 179 and uncomment line 180 to apply this change if needed
        currState = idle;
      break;
    }
    if(!client.connected()){
      if(!client.connect(HOST, PORT)){
        Serial.printf(".");
        return;
      }    
      Serial.println("Client not connected!");
      delay(500);
    }
    //timer = millis();
    String detection = objDetect();
    //timer = millis() - timer; //Total duration of call to objdetect
    if(detection.length() > 0){
      long center[2];
      parseRecStr(detection, center, &boxArea);
      distanceFromCam = (-20.36)*log1p(boxArea);
      distanceFromCam = distanceFromCam+260.92;
      if(currState == Start){
        brushlessMotor.write(113); //Keep the brushless motors going at a constant speed, but just don't go too fast, with the current build,
        //A fast enough speed will bend the carbon fiber rod too much to the point where Braitenburg desired path line isn't met
        //Consider changing this if and when a better gate structure is implemented. 
      }
      else if(currState == slowDown){
        closePWM = map(distanceFromCam, 27, 207, 0, 30); //Map the equation return from lines 159-160 (or what the distance is)
        //Which should be from 27 to 207 inches to some number from 0 to 30 and add that to 90 in the below write() call
        brushlessMotor.write(closePWM+90);
      }
      else if(currState == closeGate){
        brushlessMotor.write(105); //Keep at a constant forward speed to help servo a bit
        if(servoCount != 180){ //This really doesn't need to be here as timing aspect was removed from this state from intial code
          servoMotor.write(servoCount);
          servoCount += 5; //Increse the next angle write by 5 degrees
        }
      }
      else if(currState == idle){
        brushlessMotor.write(90); //You may want to change this here to something like 115 to help the tail agent lead the braitenburg to the goal
        //brushlessMotor.write(115);
      }
      else{
      }
    }
    else{
    }
  delay(20);
}


void init(){
  brushlessMotor.write(90);
  delay(0.1);
  brushlessMotor.write(90);
  delay(2000);
}