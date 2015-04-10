/*
 * BurnCam program.
 * For burn cam project V3. Electronic afghan camera box.
 *
 * Florent Gales 2015
 * rataflo@free.fr / More info : iloveto.fail
 * License Rien à branler.
 * Do What The Fuck You Want License
 *
 * Project based on https://github.com/jocelyngirard/erixposure-arduino by Jocelin Girard based on work of Kevin Kadooka http://kadookacameraworks.com/light.html
 *
 * Lux sensor : TLS 2591.
 * LCD display.
 * Neo pixel Stick from Adafruit.
 * 
 * Max speed with servo 9g = 0.45s
 */

#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <math.h>
#include <LiquidCrystal.h>
#include <Button.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2591.h>
#include <Servo.h>
#include "BurnCam.h"

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(8, PIN_LED, NEO_GRB + NEO_KHZ800);
Adafruit_TSL2591 luxMeter = Adafruit_TSL2591(2591);
LiquidCrystal lcd(6, 5, 4, 3, 2, 1);//LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
Servo obturator;

int apertureIndex = 5;
int isoIndex = 1;
int lightMeteringType;
float exposureTimeInSeconds;
byte printTime = 0;
byte printLight = 0;
int stateObturator = 0; //1 = open; 0 = closed
byte typeOfMetering = REFLECTED_METERING;
unsigned long switchInfos = 0; //Each 2second switch infos displayed (focal -> iso -> focal).
byte luxMeterGain = 0;
byte luxMeterTiming = 0;

int triggerState = 0;
int button1State = 0;
boolean redLight = false; // sate of red light (true = on, false = off)

void setup()
{

  //Serial.begin(9600);///!\ NO DEBUG. LCD USE PIN 2 (RX).
  //Parameter values stored in EEPROM
  loadEepromValues();
  
  //Servo init
  obturator.attach(PIN_OBTURATOR);
  obturator.write(0);
  stateObturator = 0;
  delay(500);
  obturator.detach();//detach for noise.
  
  //Neo pixel leds
  pixels.begin();
  //make sure led are off
  for(int i=0;i<8;i++){
   pixels.setPixelColor(i, pixels.Color(0,0,0)); 
   pixels.show();
  }
  
  //buttons
  pinMode(PIN_TRIGGER, INPUT);
  pinMode(PIN_BUTTON1, INPUT);

  //LCD
  lcd.begin(16, 2);
  
  /* Initialise the sensor */
  if(!luxMeter.begin())
  {
    lcd.print("No TSL2591!");
  }
  configLuxMeter();
    
  // Demo for the style.
  lcd.setCursor(0, 0);
  for(int i = 1; i <= 5;i++){
    lcd.setCursor(i, 0);
    lcd.print(" BURN CAM");
    delay(100);
  }
  
  lcd.setCursor(0, 1);
  for(int i = 1; i <= 7;i++){
    lcd.setCursor(i, 1);
    lcd.print(" V3.0");
    delay(100);
  }
  delay(500);
  lcd.clear();
  
  switchInfos = millis();
}

void loop()
{
  showMenu(1);
}


/* Menu
  // 1 => open obturator
  //   11 => close obturator => 2: take shot
  // 2 => Take shot
  //   21 => Auto => 3 : red light on
  //   22 => Manual
  //     221 => Cycle through time choices. => 3 : red light on
  //   23 => Exit
  // 3 => Red light on
  //   31 => Red light off
  // 4 => Print
  //   41 => GO xtime ylight
  //   42 => Time
  //     421 => Exposure time choices
  //   43 => light
  //     431 => Light choices
  //   44 => Exit
  // 5 => Timer
  //   51 => 10' => 21
  //   52 => 30' => 21
  //   53 => Exit 
  // 6 => Setup
  //   61 => Focal
  //     611 => Cycle through focal choices
  //   62 => ISO
  //     621 => Cycle through iso choices
  //   63 => Light sensor
  //     631 => Gain
  //       6311 => Cycle through gain choices
  //     632 => Timing
  //       6321 => Cycle through timing choices
  //   64 => Exit
  //  
*/
void showMenu(int menu){
  showMenu(menu, NULL);
}

void showMenu(int menu, int previousMenu){
  lcd.clear();
  calcExpTimeAndShowInfos(NULL, NULL);
  
  lcd.setCursor(0, 1);
  
  //1 : Open obturator
  if(menu == 1){
      //show under menu
      lcd.print(">Open obturator");
      //button management
      triggerState = digitalRead(PIN_TRIGGER);
      button1State = digitalRead(PIN_BUTTON1);
      while(triggerState != HIGH && button1State != HIGH){ //non blocking loop.
        
        calcExpTimeAndShowInfos(NULL, NULL);
        delay(debounceTime);
        triggerState = digitalRead(PIN_TRIGGER);
        button1State = digitalRead(PIN_BUTTON1);
      }
      //trigger button.
      if (triggerState == HIGH) {     
        openObturator();
        showMenu(11);
      }
      //button 1 trigger
      if (button1State == HIGH) {     
        showMenu(2);
      } 
  }
  //11 Close obturator
  else if(menu == 11){
      //show under menu
      lcd.print(">Close ");
      //show time running (non blocking).
      float elapsedSecond = 0;
      unsigned long currentMillis = millis();
      unsigned long previousMillis = millis();
      delay(debounceTime); //debounce time.
      triggerState = digitalRead(PIN_TRIGGER);
      while(triggerState != HIGH) {
        if(currentMillis - previousMillis > 1000 * elapsedSecond){
          lcd.setCursor(0, 1);
          lcd.print(">Close ");
          if(elapsedSecond < 10){
            lcd.print(elapsedSecond);
            lcd.print("s       ");
          }else if(elapsedSecond < 60){
            lcd.print(elapsedSecond);
            lcd.print("s      ");
          }else{ //mn management
            int mn = (int)elapsedSecond / 60;
            int sec = (int)elapsedSecond % 60;
            lcd.print(mn);lcd.print(":");lcd.print(sec);
          }
          elapsedSecond += 1;
        }
        
        delay(debounceTime); //debounce time.
        triggerState = digitalRead(PIN_TRIGGER);
        currentMillis = millis();
      }
      
      closeObturator();
      showMenu(2); //go to menu take shot
  }
  //2 : Take shot
  else if(menu == 2){
      //show under menu
      lcd.print(">Take shot      ");
      //button management
      triggerState = digitalRead(PIN_TRIGGER);
      button1State = digitalRead(PIN_BUTTON1);
      while(triggerState != HIGH && button1State != HIGH){ //non blocking loop.
        
        calcExpTimeAndShowInfos(NULL, NULL);
        delay(debounceTime);
        triggerState = digitalRead(PIN_TRIGGER);
        button1State = digitalRead(PIN_BUTTON1);
      }
      //trigger button.
      if (triggerState == HIGH) {     
         showMenu(21);
      }
      //button 1 trigger
      if (button1State == HIGH) {     
         showMenu(3);
      } 
  }
  //21 : auto
  else if(menu == 21){
      //show under menu
      lcd.print(">Auto           ");
      if(waitForButton() == PIN_TRIGGER){
        if(previousMenu != NULL && previousMenu == 51){//if menu before is timer 10'
          showCountdown(10);
        }else if(previousMenu != NULL && previousMenu == 52){//if menu before is timer 30'
          showCountdown(30);
        }
        takeShot(NULL);
        showMenu(3); //go to red light after shot
      }else{
        showMenu(22, previousMenu);
      }
  }
  //22 : Manual
  else if(menu == 22){
      //show under menu
      lcd.print(">Manual          ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(221, previousMenu);
      }else{
        showMenu(23, previousMenu);
      }
  }
  //221 : Manual choice
  else if(menu == 221){
      //show under menu
        int menuChoice = 0;
        lcd.print(">");
        lcd.print(shutterSpeedTexts[menuChoice]);
        lcd.print("    ");
        while(waitForButton() == PIN_BUTTON1){
          if(menuChoice == length(shutterSpeeds)){
            menuChoice = 0;
          }else{
            menuChoice++;
          }
          lcd.setCursor(0, 1);
          if(menuChoice < length(shutterSpeeds)){
            lcd.print(">");
            lcd.print(shutterSpeedTexts[menuChoice]);
            lcd.print("    ");
          }else{
            lcd.print(">Exit");
          }
        }
        //parameter change
        if(menuChoice < length(shutterSpeeds)){
          if(previousMenu != NULL && previousMenu == 51){//if menu before is timer 10'
            showCountdown(10);
          }else if(previousMenu != NULL && previousMenu == 52){//if menu before is timer 30'
            showCountdown(30);
          }
          takeShot(shutterSpeeds[menuChoice]);
          showMenu(3); //go to red light after shot
        }else{
          showMenu(22, previousMenu);
        }
        
  }
  else if(menu == 23){
      lcd.print(">Exit           ");
      if(waitForButton() == PIN_TRIGGER){
        if(previousMenu != NULL && previousMenu == 51){//if menu before is timer 10'
          showCountdown(10);
        }else if(previousMenu != NULL && previousMenu == 52){//if menu before is timer 30'
          showCountdown(30);
        }else{
          showMenu(2);
        }
      }else {
        showMenu(21, previousMenu);
      }
  }
  //3 : Red light on
  if(menu == 3){
      //show under menu
      if(!redLight){
        lcd.print(">Red light on");
      }else{
        lcd.print(">Red light off");
      }
      if(waitForButton() == PIN_TRIGGER){
        if(!redLight){ //Switch red light on
          for(int i=0;i<8;i++){
            pixels.setPixelColor(i, pixels.Color(100, 0, 0));
          }
          redLight = true;
        }else{  //Switch red light on
          for(int i=0;i<8;i++){
            pixels.setPixelColor(i, pixels.Color(0, 0, 0));
          }
          redLight = false;
        }
        pixels.show();
        showMenu(3);
      }else{
        showMenu(4);
      }
 }
 //4 : Print
 else if(menu == 4){
      lcd.print(">Print          ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(41);
      }else {
        showMenu(5);
      }
 }
 //41 : Expose
 else if(menu == 41){
      lcd.print(">Go ");
      lcd.print(timesPrint[printTime]);
      lcd.print("s ");
      lcd.print(lightTypes[printLight]);
      if(waitForButton() == PIN_TRIGGER){
        
        //Sitch rgb led on.
        for(int i=0;i<8;i++){
          if(printLight == 0){ //Neutral
            pixels.setPixelColor(i, pixels.Color(light_neutral[0],light_neutral[1],light_neutral[2]));
          }else if(printLight == 1){ //warm
            pixels.setPixelColor(i, pixels.Color(light_warm[0],light_warm[1],light_warm[2]));
          }else if(printLight == 2){ //cold
            pixels.setPixelColor(i, pixels.Color(light_cold[0],light_cold[1],light_cold[2]));
          }
        }
        pixels.show();
        redLight = false;
        showCountdown(timesPrint[printTime]);
        
        //switch off
        for(int i=0;i<8;i++){
          pixels.setPixelColor(i, pixels.Color(0, 0, 0));
        }
        pixels.show();
        
        //say it's done.
        lcd.setCursor(0, 1);
        lcd.print("Done!           ");
        delay(1000);
        showMenu(4);
        
      }else{
        showMenu(42);
      }
 }
 //42 : Print time
 else if(menu == 42){
      lcd.print(">Time");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(421);
      }else {
        showMenu(43);
      }
 }
 //421 : Exposure time choice
 else if(menu == 421){
      int menuChoice = 0;
      lcd.print(">");
      lcd.print(timesPrint[menuChoice]);
      lcd.print("s   ");
      while(waitForButton() == PIN_BUTTON1){
        if(menuChoice == length(timesPrint)){
          menuChoice = 0;
        }else{
          menuChoice++;
        }
        lcd.setCursor(0, 1);
        if(menuChoice < length(timesPrint)){
          lcd.print(">");
          lcd.print(timesPrint[menuChoice]);
          lcd.print("s   ");
        }else{
          lcd.print(">Exit");
        }
      }
      //parameter change
      if(menuChoice < length(timesPrint)){
        printTime = menuChoice;
        EEPROM.write(PRINT_TIME_ADDR, printTime);
      }
      showMenu(42);
 }
 //43 : Exposure time
 else if(menu == 43){
      //show under menu
      lcd.print(">Light");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(431);
      }else {
        showMenu(44);
      }
 }
 //431 : Choices type of light
 else if(menu == 431){
      //show under menu
      int menuChoice = 0;
      lcd.print(">");
      lcd.print(lightTypes[menuChoice]);
      while(waitForButton() == PIN_BUTTON1){
        if(menuChoice == length(lightTypes)){
          menuChoice = 0;
        }else{
          menuChoice++;
        }
        lcd.setCursor(0, 1);
        if(menuChoice < length(lightTypes)){
          lcd.print(">");
          lcd.print(lightTypes[menuChoice]);
        }else{
          lcd.print(">Exit");
        }
      }
      //parameter change
      if(menuChoice < length(lightTypes)){
        printLight = menuChoice;
        EEPROM.write(PRINT_LIGHT_ADDR, printLight);
      }
      showMenu(43);
 }
 //44 : Exit
 else if(menu == 44){
      //show under menu
      lcd.print(">Exit           ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(4);
      }else {
        showMenu(41);
      }
 }
 //5 :Timer
 else if(menu == 5){
      //show under menu
      lcd.print(">Timer          ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(51);
      }else {
        showMenu(6);
      }
 }
 //51 : Timer length
 else if(menu == 51){
      //show under menu
      lcd.print(">10s            ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(3, 51);
    }else {
        showMenu(52);
    }
 }
 else if(menu == 52){
      //show under menu
      lcd.print(">30s            ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(3, 52);
      }else {
        showMenu(53);
      }
 }
 else if(menu == 53){
      lcd.print(">Exit           ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(5);
      }else {
        showMenu(51);
      }
 }
 //6 : Setup
 else if(menu == 6){
      //show under menu
      lcd.print(">Setup          ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(61);
      }else {
        return;
      }
 }
 //61 : Focal
 else if(menu == 61){
      //show under menu
      lcd.print(">Focal          ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(611);
      }else {
        showMenu(62);
      }
 }
 //611 : Apertures choices
 else if(menu == 611){
      //show under menu
      int menuChoice = 0;
      lcd.print(">");
      lcd.print(apertures[menuChoice]);
      lcd.print("  ");
      while(waitForButton() == PIN_BUTTON1){
        if(menuChoice == length(apertures)){
          menuChoice = 0;
        }else{
          menuChoice++;
        }
        
        if(menuChoice < length(apertures)){
          calcExpTimeAndShowInfos(menuChoice, NULL);
          lcd.setCursor(0, 1);
          lcd.print(">");
          lcd.print(apertures[menuChoice]);
          lcd.print("  ");
        }else{
          lcd.setCursor(0, 1);
          lcd.print(">Exit           ");
        }
      }
      //parameter change
      if(menuChoice < length(apertures)){
        apertureIndex = menuChoice;
        EEPROM.write(APERTURE_MEMORY_ADDR, apertureIndex);
      }
      showMenu(61);
 }
 //62 : ISO
 else if(menu == 62){
      //show under menu
      lcd.print(">ISO            ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(621);
      }else {
        showMenu(63);
      }
 }
 //621 : Iso choice
 else if(menu == 621){
      //show under menu
      int menuChoice = 0;
      lcd.print(">");
      lcd.print(isos[menuChoice]);
      lcd.print("iso  ");
      while(waitForButton() == PIN_BUTTON1){
        if(menuChoice == length(isos)){
          menuChoice = 0;
        }else{
          menuChoice++;
        }
        
        if(menuChoice < length(isos)){
          calcExpTimeAndShowInfos(NULL, menuChoice);
          lcd.setCursor(0, 1);
          lcd.print(">");
          lcd.print(isos[menuChoice]);
          lcd.print("iso  ");
        }else{
          lcd.setCursor(0, 1);
          lcd.print(">Exit           ");
        }
      }
      //parameter change
      if(menuChoice < length(isos)){
        isoIndex = menuChoice;
        EEPROM.write(ISO_MEMORY_ADDR, isoIndex);
      }
      showMenu(62);
 }
 //63 : Light sensor
 else if(menu == 63){
      //show under menu
      lcd.print(">Light sensor   ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(631);
      }else {
        showMenu(64);
      }
 }
 //631 : Light sensor : gain
 else if(menu == 631){
      //show under menu
      lcd.print(">Gain           ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(6311);
      }else {
        showMenu(632);
      }
 }
 //6311 : Gain choice
 else if(menu == 6311){
      //show under menu
      int menuChoice = 0;
      lcd.print(">");
      lcd.print(gainTexts[menuChoice]);
      while(waitForButton() == PIN_BUTTON1){
        if(menuChoice == length(gains)){
          menuChoice = 0;
        }else{
          menuChoice++;
        }
        
        if(menuChoice < length(gains)){
          lcd.setCursor(0, 1);
          lcd.print(">");
          lcd.print(gainTexts[menuChoice]);
        }else{
          lcd.setCursor(0, 1);
          lcd.print(">Exit           ");
        }
      }
      //parameter change
      if(menuChoice < length(gains)){
        luxMeterGain = menuChoice;
        EEPROM.write(LUX_METER_GAIN_ADDR, luxMeterGain);
        configLuxMeter();
      }
      showMenu(631);
 }
 //632 : Light sensor : timing
 else if(menu == 632){
      //show under menu
      lcd.print(">Timing         ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(6321);
      }else {
        showMenu(633);
      }
 }
 //6321 : Timing choice
 else if(menu == 6321){
      //show under menu
      int menuChoice = 0;
      lcd.print(">");
      lcd.print(timingTexts[menuChoice]);
      while(waitForButton() == PIN_BUTTON1){
        if(menuChoice == length(timings)){
          menuChoice = 0;
        }else{
          menuChoice++;
        }
        
        if(menuChoice < length(timings)){
          lcd.setCursor(0, 1);
          lcd.print(">");
          lcd.print(timingTexts[menuChoice]);
        }else{
          lcd.setCursor(0, 1);
          lcd.print(">Exit           ");
        }
      }
      //parameter change
      if(menuChoice < length(timings)){
        luxMeterTiming = menuChoice;
        EEPROM.write(LUX_METER_TIMING_ADDR, luxMeterTiming);
        configLuxMeter();
      }
      showMenu(631);
 }
 //633 : Exit
 else if(menu == 633){
      //show under menu
      lcd.print(">Exit          ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(63);
      }else {
        showMenu(631);
      }
 }
 //64 : Exit
 else if(menu == 64){
      //show under menu
      lcd.print(">Exit          ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(6);
      }else {
        showMenu(61);
      }
 }
}

/* block wait for button and return when button pressed
* return PIN_TRIGGER => trigger, PIN_BUTTON1 => button 1
*/
byte waitForButton(){
  delay(debounceTime); //debounce time
  // read the state of the buttons
  triggerState = digitalRead(PIN_TRIGGER);
  button1State = digitalRead(PIN_BUTTON1);

  while(triggerState != HIGH && button1State != HIGH) {     
    triggerState = digitalRead(PIN_TRIGGER);
    button1State = digitalRead(PIN_BUTTON1);
    delay(debounceTime);  //debounce time
  }
  return triggerState == HIGH ? PIN_TRIGGER : PIN_BUTTON1;
}

void openObturator(){
  //Obturator init
  obturator.attach(PIN_OBTURATOR);
  //Verify the obturator is closed.
  if(stateObturator == 0){
    obturator.write(80);
    stateObturator = 1;
    delay(400);
  }
  obturator.detach();
}

void closeObturator(){
    //Obturator init
    obturator.attach(PIN_OBTURATOR);
    
    //close servo.
    if(stateObturator == 1){
      obturator.write(0);
      stateObturator = 0;
      delay(500);
    }
    obturator.detach();
}

/*
* Take shot.
*/
void takeShot(float nbSecond){
  
  int elapsedSecond = 0;
  
  exposureTimeInSeconds = nbSecond != NULL ? nbSecond : NULL;
  
  //Calculate exposition time with best quality for lux
  if(exposureTimeInSeconds == NULL){
    //luxMeter.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);
    calcExpTimeAndShowInfos(NULL, NULL);
    //luxMeter.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
  }
  //Inclusion du temps d'ouverture/fermeture.
  exposureTimeInSeconds -=  0.45;
  
  //Ouverture servo.
  obturator.attach(PIN_OBTURATOR);
  obturator.write(80);
  delay(250);
  stateObturator = 1;

  showCountdown(exposureTimeInSeconds);
  
  obturator.write(0);
  stateObturator = 0;
  delay(500);
  obturator.detach();  
}

float getLuxValue()
{
//  sensors_event_t event;
//  luxMeter.getEvent(&event);
//  float lux = event.light;
  
  uint32_t lum = luxMeter.getFullLuminosity();
  uint16_t ir, full;
  ir = lum >> 16;
  full = lum & 0xFFFF;
  
  return luxMeter.calculateLux(full, ir);
}

/*
* Calc exposure time, display info on lcd.
* param focal & iso : if not NULL use it for calculation. 
*/
void calcExpTimeAndShowInfos(int focal, int iso)
{
  lcd.setCursor(0, 0);
  
  float aperture = focal != NULL ? apertures[focal] : apertures[apertureIndex]; 
  float luxValue = getLuxValue();
  int isoValue = iso != NULL ? isos[iso] : isos[isoIndex];
  int shutterSpeedIndex = 0;
  boolean isPositive;
  float exposureValue = log(luxValue / 2.5) / log(2);
  
  
  // Problem : no light or too much light
  if (luxValue <= 0.0f) 
  {
    lcd.print("Err lux sensor! ");
    exposureTimeInSeconds = 0;
    return;
  }
  
  //Calcul for Incident (captor is on the camera)
  exposureTimeInSeconds = pow(aperture, 2) / (luxValue / (REFLECTED_CALIBRATION_CONSTANT   / isoValue));
  
  //Limit of current shutter speed.
  if(exposureTimeInSeconds < 0.45f){
    exposureTimeInSeconds = 0.45;
  }
  
  // Minutes management
  if (exposureTimeInSeconds >= 60.0f)
  {
    lcd.print(exposureTimeInSeconds / 60.0f);
    lcd.print("m");
  }
  // Seconds management 
  else if ((exposureTimeInSeconds > 1.0f) && (exposureTimeInSeconds < 60.0f))
  {
    lcd.print((int)exposureTimeInSeconds);
    lcd.print("s");
    
  }
  // Fractional time management, we find the nearest standard shutter speed linked to exposure time value
  else if (exposureTimeInSeconds <= 1.0f)
  {
    float shortestSpeedGap = 0.5f;//previous 1.0f
    for (int index = 0; index < length(shutterSpeeds); index++)
    {
      float shutterSpeed = shutterSpeeds[index];
      float speedGap = exposureTimeInSeconds - shutterSpeed;
      float absSpeedGap = fabs(speedGap);
      if (absSpeedGap < shortestSpeedGap)
      {
        shortestSpeedGap = absSpeedGap;
        shutterSpeedIndex = index;
        isPositive = speedGap < 0.0f;
      }
    }
    
    lcd.print(shutterSpeedTexts[shutterSpeedIndex]);
  }

  // We display the exposure value
  if(exposureValue > 3){
    //lcd.print(" Ev+3");
  }else if(exposureValue < -3){
    //lcd.print(" Ev-3");
  }else {
    //lcd.print(" Ev");
    //lcd.print((int)exposureValue);
  }
  lcd.print(" Ev");
  lcd.print((int)exposureValue);
  
  //aperture or iso value (switch each 2 sec)
  if(millis() > switchInfos + 4000) switchInfos = millis();
  if(millis() < switchInfos + 2000){
    lcd.print(" f/");
    lcd.print(aperture);
  }else if(millis() < switchInfos + 4000){
    lcd.print(" ");
    lcd.print(isoValue);
    lcd.print("iso   ");
  }
}


void loadEepromValues()
{
  apertureIndex = EEPROM.read(APERTURE_MEMORY_ADDR);
  if (apertureIndex >= length(apertures))
  {
    apertureIndex = 5;
  }
  isoIndex = EEPROM.read(ISO_MEMORY_ADDR);
  if (isoIndex >= length(isos))
  {
    isoIndex = 0;
  }
  lightMeteringType = EEPROM.read(LIGHT_TYPE_MEMORY_ADDR);
  if (lightMeteringType == 255)
  {
    lightMeteringType = REFLECTED_METERING;
  } 
  printTime = EEPROM.read(PRINT_TIME_ADDR);
  if (printTime == 255)
  {
    printTime = 0;
  }
  printLight = EEPROM.read(PRINT_LIGHT_ADDR);
  if (printLight == 255)
  {
    printLight = 0;
  }
  luxMeterGain = EEPROM.read(LUX_METER_GAIN_ADDR);
  if (luxMeterGain == 255)
  {
    luxMeterGain = 0;
  }
  luxMeterTiming = EEPROM.read(LUX_METER_TIMING_ADDR);
  if (luxMeterTiming == 255)
  {
    luxMeterTiming = 0;
  }
}

/*
* Final countdown tududududuuuu.
*/
void showCountdown(float nbSeconds)
{
  //Start counting
  float elapsedSecond = nbSeconds;
  unsigned long currentMillis = millis();
  unsigned long previousMillis = millis();
  
  lcd.setCursor(0, 1);
  lcd.print("                ");
  
  lcd.setCursor(0, 1);
  int mn = (int)elapsedSecond / 60;
  int sec = (int)elapsedSecond % 60;
  if(mn<10) lcd.print("0");
  lcd.print(mn);
  lcd.print(":");
  if(sec<10) lcd.print("0");
  lcd.print(sec);
    
  while((currentMillis - previousMillis) < (1000 * nbSeconds)) {
    //display minuts:second remaining.
    if((currentMillis - previousMillis) > (1000 * (nbSeconds - elapsedSecond))){
      lcd.setCursor(0, 1);
      int mn = (int)elapsedSecond / 60;
      int sec = (int)elapsedSecond % 60;
      if(mn<10) lcd.print("0");
      lcd.print(mn);
      lcd.print(":");
      if(sec<10) lcd.print("0");
      lcd.print(sec);
      elapsedSecond -= 1;
    }
    currentMillis = millis();
  }
}

void configLuxMeter(){
  //Gain config
  luxMeter.setGain(gains[luxMeterGain]);

  //Timing config
  luxMeter.setTiming(timings[luxMeterTiming]);
     
}
