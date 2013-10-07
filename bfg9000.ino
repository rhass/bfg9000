// i2c library.
#include <Wire.h>
// Honeywell SSC/HSC i2c pressure sensory library.
#include <hsc_ssc_i2c.h>
// LCD Libraries -- Not yet implemented
#include <ST7036.h>
#include <LCD_C0220BiZ.h>

// Set the LCD type, configuration, and i2c address.
ST7036 lcd = ST7036 ( 2, 20, 0x78 );

// see hsc_ssc_i2c.h for a description of these values
// these defaults are valid for the SSCDANN150PG2A3 chip
#define SLAVE_ADDR 0x28
#define OUTPUT_MIN 0
#define OUTPUT_MAX 0x3fff  // 2^14 - 1
#define PRESSURE_MIN 0     // min is 0 for sensors that give absolute values
#define PRESSURE_MAX 150   // 150psi

// Application/implementation specific pin configuration.
#define ONBOARD_LED   13
#define TRIGGER_INPUT 10
#define SAFETY_INPUT  2
#define TRIGGER_RELAY 8
#define PUMP_RELAY    12
// Tank pressure settings -- the pump cannot output more than 20 PSI.
#define MIN_TANK_PSI  10.0
#define MAX_TANK_PSI  20.0

void setup() {
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(TRIGGER_INPUT, INPUT);
  pinMode(SAFETY_INPUT, INPUT);
  pinMode(PUMP_RELAY, OUTPUT);
  pinMode(TRIGGER_RELAY, OUTPUT);
  
  digitalWrite(PUMP_RELAY, LOW);
  digitalWrite(TRIGGER_RELAY, LOW);
  
  lcd.init();
  lcd.setContrast(10);
  
  Serial.begin(115200);
}

void fire() {
  if ( safety_engaged() == false ) {
    Serial.println("FIRE!!!");
    digitalWrite(TRIGGER_RELAY, HIGH);
    delay(25);
    digitalWrite(TRIGGER_RELAY, LOW);
  } else {
    Serial.println("Safety Engaged....");
  }
}

float currentPsi() {
  struct cs_raw ps;
  uint8_t error_level = ps_get_raw(SLAVE_ADDR, &ps);
  float psi, therm;
  if ( error_level == 4 ) {
    Serial.println(F("err pressure sensor missing"));
  } 
  else {
    if ( error_level == 3 ) {
      Serial.print(F("err pressure sensor diagnostic fault "));
      Serial.println(ps.status, BIN);
    }
    if ( error_level == 2 ) {
      // if data has already been feched since the last
      // measurement cycle
      Serial.print(F("warn stale pressure data "));
      Serial.println(ps.status, BIN);
    }
    if ( error_level == 1 ) {
      // chip in command mode
      // no clue how to end up here
      Serial.print(F("warn command mode "));
      Serial.println(ps.status, BIN);
    }
    ps_convert(ps, &psi, &therm, OUTPUT_MIN, OUTPUT_MAX, PRESSURE_MIN, PRESSURE_MAX);
    // The sensor seems to always provide absolute pressure readings despite being a gage sensor. 
    // Hack a compensation by subtracting one atmosphere of pressure (14.695PSI)
    float gpsi = psi - 14.7;
    char p_str[10];
    dtostrf(gpsi, 2, 2, p_str);
    Serial.print("Current PSI: ");
    Serial.println(p_str);
    return gpsi;
  }
}


boolean safety_engaged() {
  // The SPST switched used in my project has some odd characteristics which
  // prevent me from being able to use this as an INPUT_PULLUP pin which returns
  // the state of the pin. As such I need to invert the status.
  boolean not_safe = digitalRead(SAFETY_INPUT);
  //Serial.print("Safety Pin Status: ");
  //Serial.println(not_safe, DEC);
  if (not_safe) {
    digitalWrite(ONBOARD_LED, LOW);
    return false;
  } else {
    digitalWrite(ONBOARD_LED, HIGH);
    return true;
  }
  boolean safety = digitalRead(SAFETY_INPUT);
  Serial.print("Safety Pin Status: ");
  Serial.println(safety, DEC);
  return safety;
}

void energize_pump(boolean power) {
  Serial.print("Pump Relay Status: ");
  Serial.println(power, DEC);
  digitalWrite(PUMP_RELAY, power);
}

void loop() {
  boolean t = digitalRead(TRIGGER_INPUT);
  if ( t == false ) {
    fire(); // Fire button pressed.
  }
  lcd.clear();
  
  // Update the LCD with the current safety switch status.
  if (safety_engaged()) {
    lcd.setCursor(0,1);
    lcd.print("====== SAFE ======");
  } else {
    lcd.setCursor(0,0);
    lcd.print("******* ARMED *******");
  }
 
  // Store the current PSI for use in the loop.
  float cpsi = currentPsi();
  
  lcd.setCursor(1,0);
  lcd.print("PSI: ");
  lcd.setCursor(1,6); 
  lcd.print(cpsi);
   
  lcd.setCursor(1,12);
  lcd.print("Pump:");
  // Turn the pump on when the storage tank pressure drops below the defined PSI.
  if ( cpsi < MIN_TANK_PSI ) {
    energize_pump(true);    
    lcd.print("ON");
    // Turn the pump off when the pump meets or exceeds the defined PSI.
  } 
  else if ( cpsi >= MAX_TANK_PSI ) {
    energize_pump(false);
    lcd.print("OFF");
  }
 
  delay(500);
}  

