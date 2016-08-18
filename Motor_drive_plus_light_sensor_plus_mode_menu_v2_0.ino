
/*
Remote car code.
Author Joost van der Gaag
Some code is used is parialy deriveted from other sources. These parts are:

1) The basics of the motor drivers, from: https://www.bananarobotics.com/shop/How-to-use-the-HG7881-(L9110)-Dual-Channel-Motor-Driver-Module
2) The heartbeat and timer indicatorLED code, from the course notes
3) The Light counters using the light sensors as external timer inputs, from the course notes

*/


// wired connections
#define HG7881_B_IA_L 37 // D10 --> Motor B Input A --> MOTOR B + Left
#define HG7881_B_IB_L 36 // D11 --> Motor B Input B --> MOTOR B - Left
#define HG7881_B_IA_R 39 // D10 --> Motor B Input A --> MOTOR B + Right
#define HG7881_B_IB_R 38 // D11 --> Motor B Input B --> MOTOR B - Right
 
// functional connections
#define MOTOR_B_PWM_L HG7881_B_IA_L // Motor B PWM Speed Left
#define MOTOR_B_DIR_L HG7881_B_IB_L // Motor B Direction Left
#define MOTOR_B_PWM_R HG7881_B_IA_R // Motor B PWM Speed Right
#define MOTOR_B_DIR_R HG7881_B_IB_R // Motor B Direction Right
 
// Speed and delay for motors
#define PWM_FAST 220 // arbitrary fast speed PWM duty cycle
#define DIR_DELAY 50 // brief delay for abrupt motor changes


const int OVF_LED1A = 50; //Timere thread indicator1
const int OVF_LED1B = 51; //Timere thread indicator2

//Comunication lines from Arduino Uno
const int com1 = 40;
const int com2 = 41;
const int com3 = 42;

//movement index
int moveInd = 0;

//Light sensors
volatile long overflowsLeft = 0;
volatile long overflowsRight = 0;
volatile long fractionLeft = 0;
volatile long fractionRight = 0;
volatile long cntLeft = 0;
volatile long cntRight = 0;
volatile long freqLeft = 0;
volatile long freqRight = 0;

//LightSensor indication LED pins
const int lightLEDLeft = 12;
const int lightLEDRight = 13;

//LEDState
volatile int lightLeftState = 0;
volatile int lightRightState  = 0;

//Light thresholds
volatile int lightThresholdValues[] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000};
volatile int lightThresholdValue = 0;
volatile int lightThresholdLow = 1500; //Not used right now
volatile int lightThresholdHigh = lightThresholdValues[lightThresholdValue];

///////////////////////////////
//Mode and menu variables
///////////////////////////////

//Pins
int pinModeArray[] = {28,27,26,25,24}; //Menu LEDs pin# array
int pinRotate1 = 35; //Rotary encoder pinA
int pinRotate2 = 32; //Rotary encoder pinB
int pinSelect = 33; //Rotary encoder push button pin (select action)

//Select LED specific variables
long EditBlinkTime; //Last time edit LED blinked
long EditBlinkDif = 250;//Edit LED blink interfall in ms

//Select threshold preventing double click
volatile long lastSelect; //Last time select was issued
volatile long selectThreshold = 500; //select threshold agains double click

//Mode variables also for temp mode pos (while editing)
volatile int mode = 1; //Mode
volatile int encoderPos = 0;
volatile int encoderCount = 0;
volatile int ledPos = 1; //temp mode pos

//Distance sensor related variables
int pinTrigger = 2;
int pinEcho = 3;
long duration; //Echo duration
long distance = 20;  //Distance to object in cm
long prevDistance = 20;
long prevPrevDistance = 20;

//Obstacle variables and Auto mode variables
volatile boolean obstacle = false; 
const long obstacleThreshold = 15; //Threshold for obstacle distance in cm
const long obstacleMinThreshold = 7;
volatile boolean turning = false; //If turning or not
const long turningTime = 2000; //Time of each turn (needs to be set by trail and error)
volatile boolean reversing = false; //If reversing
const long reverseTime = 1000;
volatile long timeStart; //Start time of action (both reversing and turning)


/////////////MODES///////////
//
//Different led pins [modes](array index):
//  0 = Edit;
//  1 = Idle and light intensity change mode
//  2 = Bluetooth
//  3 = Follow Light
//  4 = Autonome
//
////////////////////////////

const int MODE_EDIT       = 0;
const int MODE_IDLE       = 1;
const int MODE_BLUETOOTH  = 2;
const int MODE_LIGHT      = 3;
const int MODE_AUTO       = 4;
const int MODE_LIGHT_SET  = 1; // Same as idle to add extra mode

//Menu state variables
volatile boolean editing = false;
volatile boolean selected = false;

//cuurentTime in ms
long currentTime;

void setup(){
  noInterrupts(); // Stop interupts
  Serial.begin(9600);
  motorSetup();
  bluetoothComSetup();
  interuptSetup();
  lightTimerSetup();
  menuSetup();
  changeLightThreshold(lightThresholdValue);
  distanceSetup();
  delay(100);
  interrupts(); // Start interupts
}

void loop(){
  delay(110);
  
  
  //Get distance
  prevPrevDistance = prevDistance;
  prevDistance = distance;
  
  //noInterrupts();
  duration = pingDistanceTime();
  distance = microToCm(duration);
  
  if(distance < obstacleMinThreshold){
    distance = prevDistance;
  }
  //interrupts();
  
  Serial.println(distance);
  
  //Detect obstacle
  if(distance < 7000){//Interupts can be triggered during measurement leading to false information (very large numbers) this is basaclly a low-pass filter against false positives
    if(max(distance, max(prevDistance,prevPrevDistance)) < obstacleThreshold){//Filter out single low values
      obstacle = true;
    }else{
      obstacle = false;
    }  
  }
  
  currentTime = millis();
  //Let Edit LED blink
  if(editing){
    if((currentTime-EditBlinkTime)>EditBlinkDif){
      EditBlinkTime = currentTime;
      digitalWrite(pinModeArray[0],digitalRead(pinModeArray[0])^1);  
    }
  }else{
    if(digitalRead(pinModeArray[0])==HIGH){
      digitalWrite(pinModeArray[0], LOW);  
    }
  }  
  
  //If mode is selected activate that mode
  if(selected){
    selectedLedAction(ledPos);
    mode = ledPos;
    selected = false; 
  }
  
 
}

//////////////////////////////////////////////////////////////////
//
//SETUP FUNCTIONS, this will set the pins to ouput mode and will set them all to low (stop the motors)
//
//////////////////////////////////////////////////////////////////
void menuSetup(){
  //Set all pins for menu
  for(int i = 0; i < sizeof(pinModeArray); i++){
    pinMode(pinModeArray[i], OUTPUT);
  }  
  pinMode(pinSelect, INPUT);
  pinMode(pinRotate1, INPUT);
  pinMode(pinRotate2, INPUT);
  
  digitalWrite(pinSelect, HIGH);
  digitalWrite(pinRotate1, HIGH);
  digitalWrite(pinRotate2, HIGH);

  
  //Attach interups for rotary encoder (slect and rotate)
  attachInterrupt(2, rotarySelect, RISING);
  attachInterrupt(3, rotateRight, RISING);
  attachInterrupt(5, rotateLeft, RISING);
  
  //Set menu LEDS to inital state
  ledMenu(ledPos);
  
  //Set compare times to current time
  EditBlinkTime, lastSelect = millis();  
}

void distanceSetup(){
  pinMode(pinTrigger, OUTPUT);
  pinMode(pinEcho, INPUT);  
}

void bluetoothComSetup(){
  //set pins to read movement
  pinMode(com1, INPUT);
  pinMode(com2, INPUT);
  pinMode(com3, INPUT); 
 
  //Light intensity pins on tMinus arduino
  DDRA = 255; 
  
  
  //Pin setup for indication leds
  pinMode(OVF_LED1A, OUTPUT);
  pinMode(OVF_LED1B, OUTPUT);
  digitalWrite(OVF_LED1A, LOW);
  digitalWrite(OVF_LED1B, HIGH);
}

void interuptSetup(){
  TCCR1A = 0; // initialize the register for timer 5
  TCCR1B = 0;
  TCNT1 = 0; //set timer to 0
  OCR1A = 2700;       // 625 = 20ms                                                                                                                                                                                                                                                                                                           ; //625 = 20ms
  
  /*
  CS52 CS51 CS50 Description
  0 0 0 Timer turned off
  0 0 1 System clock
  0 1 0 System clock /8
  0 1 1 System clock /64
  1 0 0 System clock /256
  1 0 1 System clock /1024
  */
  //TCCR5B |= (1 << CS50);
  //TCCR5B |= (1 << CS51);
  TCCR1B |= (1 << CS12);

  TCCR1B |= (1 << WGM12); // set CTC mode
  TIMSK1 |= (1 << OCIE1A); // enable OCR5A interrupt for timer 5  
}

void lightTimerSetup(){
  //Set LED pins as output
  pinMode(lightLEDLeft, OUTPUT);
  pinMode(lightLEDRight, OUTPUT);
  
  //Timer 4 is Right
  TCCR4A = 0;
  TCCR4B = 0;
  TCCR4B |= (1 << CS40);
  TCCR4B |= (1 << CS41);
  TCCR4B |= (1 << CS42);
  TIMSK4 |= (1 << TOIE4);
  
  //Timer 5 is Left
  TCCR5A = 0;
  TCCR5B = 0;
  TCCR5B |= (1 << CS50);
  TCCR5B |= (1 << CS51);
  TCCR5B |= (1 << CS52);
  TIMSK5 |= (1 << TOIE5);  
}

void motorSetup(){
  //Left side
  pinMode( MOTOR_B_DIR_L, OUTPUT );
  pinMode( MOTOR_B_PWM_L, OUTPUT );
  digitalWrite( MOTOR_B_DIR_L, LOW );
  digitalWrite( MOTOR_B_PWM_L, LOW );
  
  //Right side
  pinMode( MOTOR_B_DIR_R, OUTPUT );
  pinMode( MOTOR_B_PWM_R, OUTPUT );
  digitalWrite( MOTOR_B_DIR_R, LOW );
  digitalWrite( MOTOR_B_PWM_R, LOW );
}

//////////////////////////////////////////////////////////////////
//
//Interupt functions
//
//////////////////////////////////////////////////////////////////
ISR(TIMER1_COMPA_vect)
{
  digitalWrite(OVF_LED1A, digitalRead(OVF_LED1A) ^ 1);
  digitalWrite(OVF_LED1B, digitalRead(OVF_LED1B) ^ 1);
  
  //Only read movement when in bluetooth mode
  if(mode == MODE_BLUETOOTH){
    readMovement(digitalRead(com1), digitalRead(com2), digitalRead(com3));
  }
  
  //Light sensor part
  //Left
  overflowsLeft=cntLeft;
  cntLeft=0;
  fractionLeft=TCNT5;
  TCNT5=0;
  freqLeft = (overflowsLeft << 16) + fractionLeft;
  
  //Right
  overflowsRight=cntRight;
  cntRight=0;
  fractionRight=TCNT4;
  TCNT4=0;
  freqRight = (overflowsRight << 16) + fractionRight;
  
//  Serial.print("Left: ");
//  Serial.print(freqLeft);
//  Serial.print(" Hz ; ");
//  Serial.print("Right: ");
//  Serial.print(freqRight);
//  Serial.println(" Hz");
  
  //Toggle Left LED
  if(freqLeft > lightThresholdHigh){
    if(lightLeftState != 1){
      digitalWrite(lightLEDLeft, HIGH);
      lightLeftState = 1;
    }  
  }else{
    if(lightLeftState != 0){
      digitalWrite(lightLEDLeft, LOW);
      lightLeftState = 0;
    } 
  }
    //Toggle Right LED
  if(freqRight > lightThresholdHigh){
    if(lightRightState != 1){
      digitalWrite(lightLEDRight, HIGH);
      lightRightState = 1;
    }  
  }else{
    if(lightRightState != 0){
      digitalWrite(lightLEDRight, LOW);
      lightRightState = 0;
    } 
  }
  
  //Move towards light when mode equals light mode
  if(mode == MODE_LIGHT){
    if(lightLeftState == 1 && lightRightState == 1){//Forwards
      if(moveInd != 5){
        moveInd = 5;  
        carForward();
      }  
    }else if(lightLeftState == 1 && lightRightState == 0){//Left
      if(moveInd != 4){
        moveInd = 4;
        carLeft();  
      }  
    }else if(lightLeftState == 0 && lightRightState == 1){//Right
      if(moveInd != 1){
        moveInd = 1;  
        carRight();
      } 
    }else if(lightLeftState == 0 && lightRightState == 0){//Stop
      if(moveInd != 0){
        moveInd = 0; 
        carStop(); 
      }  
    }
  }
  
  //Auto mode
  if(mode == MODE_AUTO){
    if(!turning && !reversing){//Only start evading when not already doing so
      if(obstacle){
        reversing = true;
        timeStart = millis();
        moveInd = 2;
        carReverse(); //Start reversing 
      }else{
        //If no obstacle and not moving forwards, start moving forwards
        if(moveInd != 5){
          moveInd = 5;  
          carForward();
        }  
      }
    }else if(reversing){
      if((millis()-timeStart)>reverseTime){//When reverse time is over
        reversing = false;
        turning = true;
        timeStart = millis();
        moveInd = 4;
        carLeft();  
      }
    }else if(turning){
      if((millis()-timeStart)>turningTime){//When reverse time is over
        turning = false;  
        obstacle = false;
      }
    }  
  }
  
}

//Right light sensor timer overflow interupt
ISR(TIMER4_OVF_vect) //Timer4  = Right
{
  cntRight++;
}

//Left light sensor timer overflow interupt
ISR(TIMER5_OVF_vect) //Timer5 = Left
{
  cntLeft++;
}

//interupt when rotary button is pressed (select action)
void rotarySelect(){
  //Filter presses withing threshold against accidental double click
  if((millis()-lastSelect)>selectThreshold){  
    if(editing){//When pressed in editing mode exit editing state and activate selected state
      editing = false;
      selected = true;
    }else{// If not in editing state go to editing state
      editing = true;
      mode = MODE_EDIT;
      carStop(); //Stop the car
    }
    lastSelect = millis(); //Update last pressed
  }
}

//interupt when rotary encoder is turnes (rotation)(Needs improvement to ga forwards and backwards)
void rotateRight(){
  if((millis()-lastSelect)>selectThreshold){
    if(editing){//Only do something when in editing state
  
      changeLedPos(true);
  
    }
    
    //Light intensity
    if(mode == MODE_LIGHT_SET){
      lightThresholdValue ++;
      if(lightThresholdValue>7){lightThresholdValue = 0;}
      changeLightThreshold(lightThresholdValue);
      lightThresholdHigh = lightThresholdValues[lightThresholdValue];    
    }
    lastSelect = millis(); //Update last pressed
  }
}

//interupt when rotary encoder is turnes (rotation)(Needs improvement to ga forwards and backwards)
void rotateLeft(){
  if(editing){//Only do something when in editing state

    changeLedPos(false);

  }
  
  //Light intensity
  if(mode == MODE_LIGHT_SET){
    lightThresholdValue --;
    if(lightThresholdValue<0){lightThresholdValue = 7;}
    changeLightThreshold(lightThresholdValue);
    lightThresholdHigh = lightThresholdValues[lightThresholdValue];    
  }
}

////////////////////
//
//Distance functions
//
///////////////////
long pingDistanceTime(){
  digitalWrite(pinTrigger,LOW);//Low signal for a short time for clean signal
  delayMicroseconds(2);
  digitalWrite(pinTrigger,HIGH);//trigger measurement
  delayMicroseconds(10); 
  digitalWrite(pinTrigger,LOW); 
  return pulseIn(pinEcho, HIGH);
}


long microToCm(long microSec){
//v = 340m/s => 3400000 cm / s => 1cm = 29.4 ms
// devide by 2 for bouncing
return microSec / 29.4 / 2;
}


////////////////////
//
//Other Menu functions
//
///////////////////
void ledMenu(int mode){
  //Reset all mode leds
  for(int i = 1; i < sizeof(pinModeArray); i++){
    digitalWrite(pinModeArray[i], LOW);
  }
  if(mode>=1 && mode < sizeof(pinModeArray)){
    digitalWrite(pinModeArray[mode], HIGH);    
  }
  
}

//Change lightIntensityThreshold
void changeLightThreshold(int treshValue){
  byte number = 0;
  for (int i = 0; i <=treshValue; i++){
    number = number | (1 << i);  
  }
  PORTA = number ^255;
}

//Mode selected LED idication function
void selectedLedAction(int mode){
  for(int j = 0; j < 3; j++){
    for(int i = 1; i < sizeof(pinModeArray); i++){
      digitalWrite(pinModeArray[i], LOW);
    }
    delay(250);  
    for(int i = 1; i < sizeof(pinModeArray); i++){
      digitalWrite(pinModeArray[i], HIGH);
    }
    delay(250);
  }
  ledMenu(mode);
}

//change mode LEDs
void changeLedPos(boolean forwards){
 if(forwards){
   ledPos++;
  if(ledPos>4){
    ledPos = 1; 
  } 
 }else{
   ledPos--;
   if(ledPos<1){
     ledPos = 4;  
   }
 } 
 ledMenu(ledPos);
}

//////////////////////////////////////////////////////////////////
//
//Read movement function
//
//////////////////////////////////////////////////////////////////
void readMovement(int com1, int com2, int com3){
  int number = 0;
  if(com1 == HIGH){number += 4;}
  if(com2 == HIGH){number += 2;}
  if(com3 == HIGH){number += 1;}
  switch(number){
    case 0: //STOP
      //Serial.println("STOP");
      if(moveInd != 0){
        moveInd = 0; 
        carStop(); 
      }      
    break;
    case 1: //Right
      //Serial.println("RIGHT");
      if(moveInd != 1){
        moveInd = 1;  
        carRight();
      } 
    break;
    case 2: //Back(reverse)
      //Serial.println("BACK");
      if(moveInd != 2){
        moveInd = 2; 
        carReverse(); 
      } 
    break;
    case 4: //Left
      //Serial.println("LEFT");
      if(moveInd != 4){
        moveInd = 4;
        carLeft();  
      } 
    break;
    case 5: //FORWARD
      //Serial.println("FORWARD");
      if(moveInd != 5){
        moveInd = 5;  
        carForward();
      } 
    break;
    default:
      Serial.print("UNKNOWN :");
      Serial.println(number);
      if(moveInd != number){
        moveInd = 0; 
        carStop(); 
      }
    break;
  }    
}

//////////////////////////////////////////////////////////////////
//
//Stop, forward and reverse action for each side
//
//////////////////////////////////////////////////////////////////
void leftForward(){ 
  // set the motor speed and direction
  digitalWrite( MOTOR_B_DIR_L, HIGH ); // direction = forward
  analogWrite( MOTOR_B_PWM_L, 255-PWM_FAST ); // PWM speed = fast
}

void leftReverse(){
  // set the motor speed and direction
  digitalWrite( MOTOR_B_DIR_L, LOW ); // direction = reverse
  analogWrite( MOTOR_B_PWM_L, PWM_FAST ); // PWM speed = fast
}

void rightForward(){
  // set the motor speed and direction
  digitalWrite( MOTOR_B_DIR_R, HIGH ); // direction = forward
  analogWrite( MOTOR_B_PWM_R, 255-PWM_FAST ); // PWM speed = fast  
}

void rightReverse(){
  // set the motor speed and direction
  digitalWrite( MOTOR_B_DIR_R, LOW ); // direction = reverse
  analogWrite( MOTOR_B_PWM_R, PWM_FAST ); // PWM speed = fast  
}

void leftStop(){
  digitalWrite( MOTOR_B_DIR_L, LOW );
  digitalWrite( MOTOR_B_PWM_L, LOW );
}

void rightStop(){
  digitalWrite( MOTOR_B_DIR_R, LOW );
  digitalWrite( MOTOR_B_PWM_R, LOW ); 
}

//////////////////////////////////////////////////////////////////
//
//Stop both motors used in all functions for safety
//
//////////////////////////////////////////////////////////////////
void motorStop(){
  leftStop();
  rightStop();  
  delay( DIR_DELAY );
}

//////////////////////////////////////////////////////////////////
//
//Stop, foward, reverse, left and right action for car
//
//////////////////////////////////////////////////////////////////
void carStop(){
  Serial.println( "Stop..." );
  motorStop();
}

void carForward(){
  Serial.println( "Forward..." );
  motorStop();
  //Both forward is car forward
  leftForward();
  rightForward();
}

void carReverse(){
  Serial.println( "Reverse..." );
  motorStop();
  // both reverse is car reverse
  leftReverse();
  rightReverse();
}

void carLeft(){
  Serial.println( "Left..." );
  motorStop();
  //left reverse right forward is left
  leftReverse();
  rightForward();  
}

void carRight(){
  Serial.println( "Right..." );
  motorStop();
  //left forward right reverse is right
  leftForward();
  rightReverse();  
}
  
