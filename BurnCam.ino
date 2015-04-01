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
 * Lux sensor : TLS 2561.
 * LCD display.
 * Neo pixel Stick from Adafruit.
 * 
 * Max speed with servo 9g = 1/4s
 */

#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <math.h>
#include <LiquidCrystal.h>
#include <Button.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <Servo.h>
#include "BurnCam.h"

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(8, PIN_LED, NEO_GRB + NEO_KHZ800);
Adafruit_TSL2561_Unified luxMeter = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
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

int triggerState = 0;
int button1State = 0;
boolean redLight = false; // sate of red light (true = on, false = off)

void setup()
{

  //Serial.begin(9600);///!\ NO DEBUG. LCD USE PIN 2 (RX).
  
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
  
  
  //Servo
  //Obturator init
  obturator.attach(PIN_OBTURATOR);
  obturator.write(0);
  stateObturator = 0;
  delay(200);
  obturator.detach();//detach for noise.
  
  /* Initialise the sensor */
  if(!luxMeter.begin())
  {
    lcd.print("No TSL2561!");
  }
  
  /* You can also manually set the gain or enable auto-gain support */
  // luxMeter.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  //luxMeter.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  luxMeter.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  luxMeter.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  //luxMeter.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  //luxMeter.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */
  
  //Parameter values stored in EEPROM
  loadEepromValues();
  
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
  calcExpTimeAndShowInfos(NULL, NULL);
  lcd.setCursor(0, 1);
  lcd.print(">Take shot      ");
  
  //button management
  triggerState = digitalRead(PIN_TRIGGER);
  button1State = digitalRead(PIN_BUTTON1);
  
  //trigger button.
  if (triggerState == HIGH) {     
     takeShot();
  }
  //button 1 trigger
  if (button1State == HIGH) {     
     showMenu(1);
  } 
}


/* Menu
  // 0 => Take shot
  // 1 => open obturator
  //   10 => close obturator
  // 2 => Red light on
  //   21 => Red light off
  // 3 => Timer
  //   31 => 10'
  //   32 => 30'
  //   33 => Exit 
  // 4 => Print
  //   41 => GO xtime ylight
  //   42 => Time
  //     421 => Exposure time choices
  //   43 => light
  //     431 => Light choices
  //   44 => Exit
  // 5 => Setup
  //   51 => Focal
  //     511 => Cycle through focal choices
  //   52 => ISO
  //     521 => Cycle through iso choices
  //   53 => Exit
  //  
*/
void showMenu(int menu){
  lcd.clear();
  calcExpTimeAndShowInfos(NULL, NULL);
  
  lcd.setCursor(0, 1);
  
  //1 :Close obturator
  if(menu == 1){
      //show under menu
      lcd.print(">Open obturator");
      if(waitForButton() == PIN_TRIGGER){
        openObturator();
        showMenu(11);
      }else{
        showMenu(2);
      }
  }
  //10 Close obturator
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
      showMenu(1);
 }
 //2 : Red light on
  if(menu == 2){
      //show under menu
      if(!redLight){
        lcd.print(">Red light on");
      }else{
        lcd.print(">Red light off");
      }
      if(waitForButton() == PIN_TRIGGER){
        if(!redLight){ //Switch red light on
          for(int i=0;i<8;i++){
            pixels.setPixelColor(i, pixels.Color(200, 0, 0));
          }
          redLight = true;
        }else{  //Switch red light on
          for(int i=0;i<8;i++){
            pixels.setPixelColor(i, pixels.Color(0, 0, 0));
          }
          redLight = false;
        }
        pixels.show();
        showMenu(2);
      }else{
        showMenu(3);
      }
 }
 //3 :Timer
 else if(menu == 3){
      //show under menu
      lcd.print(">Timer          ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(31);
      }else {
        showMenu(4);
      }
 }
 //31 : Timer length
 else if(menu == 31){
      //show under menu
      lcd.print(">10s            ");
      if(waitForButton() == PIN_TRIGGER){
        showCountdown(10);
        takeShot();
        showMenu(31);
    }else {
        showMenu(32);
    }
 }
 else if(menu == 32){
      //show under menu
      lcd.print(">30s            ");
      if(waitForButton() == PIN_TRIGGER){
        showCountdown(30);
        takeShot();
        showMenu(32);
      }else {
        showMenu(33);
      }
 }
 else if(menu == 33){
      lcd.print(">Exit           ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(3);
      }else {
        showMenu(31);
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
        showMenu(41);
        
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
 //5 : Setup
 else if(menu == 5){
      //show under menu
      lcd.print(">Setup          ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(51);
      }else {
        return;
      }
 }
 //51 : Focal
 else if(menu == 51){
      //show under menu
      lcd.print(">Focal          ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(511);
      }else {
        showMenu(52);
      }
 }
 //511 : Apertures choices
 else if(menu == 511){
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
      showMenu(51);
 }
 //52 : ISO
 else if(menu == 52){
      //show under menu
      lcd.print(">ISO            ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(521);
      }else {
        showMenu(53);
      }
 }
 //521 : Iso choice
 else if(menu == 521){
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
      showMenu(52);
 }
 //53 : Exit
 else if(menu == 53){
      //show under menu
      lcd.print(">Exit          ");
      if(waitForButton() == PIN_TRIGGER){
        showMenu(5);
      }else {
        showMenu(51);
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
    delay(200);
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
      delay(250);
    }
    obturator.detach();
}

/*
* Take shot.
*/
void takeShot(){
  
  int elapsedSecond = 0;
  
  //Calculate exposition time with best quality for lux
  luxMeter.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);
  calcExpTimeAndShowInfos(NULL, NULL);
  luxMeter.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
  
  //Inclusion du temps d'ouverture/fermeture.
  exposureTimeInSeconds -=  0.45;
  
  //Ouverture servo.
  obturator.attach(PIN_OBTURATOR);
  obturator.write(80);
  delay(200);
  stateObturator = 1;

  //if(exposureTimeInSeconds > 1){
  showCountdown(exposureTimeInSeconds);
  //}
  
  obturator.write(0);
  stateObturator = 0;
  delay(250);
  obturator.detach();  
}

/*
* Calc exposure time, display info on lcd.
* param focal & iso : if not NULL use it for calculation. 
*/
void calcExpTimeAndShowInfos(int focal, int iso)
{
  lcd.setCursor(0, 0);
  
  sensors_event_t event;
  luxMeter.getEvent(&event);
  float aperture = focal != NULL ? apertures[focal] : apertures[apertureIndex]; 
  float luxValue = event.light;
  int isoValue = iso != NULL ? isos[iso] : isos[isoIndex];
  int shutterSpeedIndex = 0;
  boolean isPositive;
  float exposureValue = log(luxValue / 2.5) / log(2);
  
  
  // Problem : no light or too much light
  if (luxValue <= 0.0f) 
  {
    lcd.print("Err light:");
    lcd.print((int)event.light);
    lcd.print("lux");
    exposureTimeInSeconds = 0;
    return;
  }
  
  //Calcul for Incident (captor is on the camera)
  exposureTimeInSeconds = pow(aperture, 2) / (luxValue / (REFLECTED_CALIBRATION_CONSTANT   / isoValue));
  
  //Limit of current shutter speed.
  if(exposureTimeInSeconds < 0.4f){
    exposureTimeInSeconds = 0.4;
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
    float shortestSpeedGap = 1.0f;
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


