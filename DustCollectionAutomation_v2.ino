// EmonLib - Version: Latest 
#include <EmonLib.h>

/*
 * This code is for the project at 
 * http://www.iliketomakestuff.com/how-to-automate-a-dust-collection-system-arduino
 * All of the components are list on the url above.
 * 
This script was created by Bob Clagett for I Like To Make Stuff
For more projects, check out iliketomakestuff.com

Includes Modified version of "Measuring AC Current Using ACS712"
http://henrysbench.capnfatz.com/henrys-bench/arduino-current-measurements/acs712-arduino-ac-current-tutorial/

Parts of this sketch were taken from the keypad and servo sample sketches that comes with the keypad and servo libraries.

Uses https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
*/
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
 
// called this way, it uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
// you can also call it with a different address you want
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x41);

// Depending on your servo make, the pulse width min and max may vary, you 
// want these to be as small/large as possible without hitting the hard stop
// for max range. You'll have to tweak them as necessary to match the servos you
// have!

// our servo # counter
uint8_t servoCount = 6;
uint8_t servonum = 0;

const int OPEN_ALL = 100;
const int CLOSE_ALL = 99;

boolean buttonTriggered = 0;
boolean powerDetected = 0;
boolean collectorIsOn = 0;
int DC_spindown = 3000;

const int NUMBER_OF_TOOLS = 3;
const int NUMBER_OF_GATES = 6;

String tools[NUMBER_OF_TOOLS] = {"load0","load1","load2"}; //, "Floor Sweep"
int voltSensor[NUMBER_OF_TOOLS] = {A1,A2,A3};
long int voltBaseline[NUMBER_OF_TOOLS] = {0,0,0};

//DC right, Y, miter, bandsaw, saw Y, tablesaw, floor sweep
//Set the throw of each gate separately, if needed
int gateMinMax[NUMBER_OF_GATES][2] = {
  /*open, close*/
  {415,335},//5
  {500,363},//0
  {550,375},//1
  {285,410},//2
  {250,400},//3
  {290,165},//4
};
//keep track of gates to be toggled ON/OFF for each tool
int gates[NUMBER_OF_TOOLS][NUMBER_OF_GATES] = {
  {1,1,0,0,0,0},
  {0,0,1,1,0,0},
  {0,0,0,0,1,1},
};

const int dustCollectionRelayPin = 11;
const int manualSwitchPin = 12; //for button activated gate, currently NOT implemented

int mVperAmp = 185; // use 100 for 20A Module and 66 for 30A Module
double ampThreshold = .20;

double Voltage = 0;
double VRMS = 0;
double AmpsRMS = 0;

//button debouncing
int state = HIGH;      // the current state of the output pin
int reading;           // the current reading from the input pin
int previous = LOW;    // the previous reading from the input pin

// the follow variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long time = 0;         // the last time the output pin was toggled
long debounce = 200;   // the debounce time, increase if the output flickers

void setup(){ 
  Serial.begin(9600);
  pinMode(dustCollectionRelayPin,OUTPUT);
  pwm.begin();
  pwm.setPWMFreq(60);  // Default is 1000mS
  
 //record baseline sensor settings
 //currently unused, but could be used for voltage comparison if need be.
  delay(1000);
  for(int i=0;i<NUMBER_OF_TOOLS;i++){
    pinMode(voltSensor[i],INPUT_PULLUP);
    voltBaseline[i] = analogRead(voltSensor[i]); 
  }
  
}

void loop(){
  // use later for button debouncing
  reading = digitalRead(manualSwitchPin);

  if (reading == HIGH && previous == LOW && millis() - time > debounce) {
    if (state == HIGH){
      state = LOW;
     buttonTriggered = false;
    } else{
      state = HIGH;
     buttonTriggered = true;
    time = millis();    
    }
  }
  previous = reading;
   Serial.println("----------");
   //loop through tools and check
   int activeTool = 50;// a number that will never happen
   for(int i=0;i<NUMBER_OF_TOOLS;i++){
      if( checkForAmperageChange(i)){
        activeTool = i;
        exit;
      }
   }
  if(activeTool != 50){
    // use activeTool for gate processing
    if(collectorIsOn == false){
      //manage all gate positions
      for(int s=0;s<NUMBER_OF_GATES;s++){
        int pos = gates[activeTool][s];
        if(pos == 1){
          openGate(s);    
        } else {
          closeGate(s);
        }
      }
      turnOnDustCollection();
    }
  } else{
    if(collectorIsOn == true){
        delay(DC_spindown);
      turnOffDustCollection();  
    }
     for(int s=0;s<NUMBER_OF_GATES;s++){
       closeGate(s);
     }
  }
}
boolean checkForAmperageChange(int which){
   Voltage = getVPP(voltSensor[which]);
   VRMS = (Voltage/2.0) *0.707; 
   AmpsRMS = (VRMS * 1000)/mVperAmp;
   Serial.print(tools[which]+": ");
   Serial.print(AmpsRMS);
   Serial.println(" Amps RMS");
   if(AmpsRMS>ampThreshold){
    return true;
   }else{
    return false; 
   }
}
void turnOnDustCollection(){
  Serial.println("turnOnDustCollection");
  digitalWrite(dustCollectionRelayPin,1);
  collectorIsOn = true;
}
void turnOffDustCollection(){
  Serial.println("turnOffDustCollection");
  digitalWrite(dustCollectionRelayPin,0);
  collectorIsOn = false;
}
 
float getVPP(int sensor)
{
  float result;
  
  int readValue;             //value read from the sensor
  int maxValue = 0;          // store max value here
  int minValue = 1024;          // store min value here
  
   uint32_t start_time = millis();
   while((millis()-start_time) < 500) //sample for 1 Sec
   {
       readValue = analogRead(sensor);
       // see if you have a new maxValue
       if (readValue > maxValue) 
       {
           /*record the maximum sensor value*/
           maxValue = readValue;
       }
       if (readValue < minValue) 
       {
           /*record the maximum sensor value*/
           minValue = readValue;
       }
   }
   Serial.print("minVal: ");
   Serial.println(minValue);
   Serial.print("maxVal: ");
   Serial.println(maxValue);
   
   // Subtract min from max
   result = ((maxValue - minValue) * 5.0)/1024.0;
      
   return result;
 }

void closeGate(uint8_t num){
  int num2 = num;
  if (num==0){
    num2=6;
  }
  Serial.print("closeGate ");
  Serial.println(num);
  pwm.setPWM(num2, 0, gateMinMax[num][1]);
}
void openGate(uint8_t num){
  int num2 = num;
  if (num==0){
    num2=6;
  }
  Serial.print("openGate ");
  Serial.println(num);
    pwm.setPWM(num2, 0, gateMinMax[num][0]);
    delay(100);
    pwm.setPWM(num2, 0, gateMinMax[num][0]-5);
}
