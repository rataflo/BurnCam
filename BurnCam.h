/*
 * Constants for Burn Cam
 */

#ifndef BURNCAM_H
#define BURNCAM_H

#define length(array)  (sizeof(array) / sizeof(array[0]))

#define PIN_OBTURATOR                7
#define PIN_LED                      8

#define PIN_TRIGGER                  13
#define PIN_BUTTON1                  12

// EEPROM Memory addresses
#define APERTURE_MEMORY_ADDR         0
#define ISO_MEMORY_ADDR              1
#define LIGHT_TYPE_MEMORY_ADDR       2
#define PRINT_TIME_ADDR              3
#define PRINT_LIGHT_ADDR             4
#define LUX_METER_GAIN_ADDR          5
#define LUX_METER_TIMING_ADDR        6

// Light metering modes
#define REFLECTED_METERING           0
#define INCIDENT_METERING            1

// Light metering calibration constants
#define REFLECTED_CALIBRATION_CONSTANT    12.5 //12.5 = Canon/Nikon, 14 = Minolta/Pentax //ISO 2720:1974 recommends a range for K (calibration constant) with luminance in cd/m² => wtf?
#define INCIDENT_CALIBRATION_CONSTANT     250 //ISO 2720:1974 recommends a range for C of 240 to 400 with illuminance in lux;

// Available apertures
const float apertures[] = { 1.2, 1.4, 1.8, 2, 2.8, 4.5, 5.6, 8, 11, 16, 22, 32 };

// Available ISOs
const int isos[] = { 3, 5, 10, 25, 100, 200, 400, 800};

// Available shutter speeds
const float shutterSpeeds[] = { 2, 1, 1/2.0, 1/4.0};
const char* shutterSpeedTexts[] = { "2s", "1s", "1/2s", "1/4s" };

//Light for print (RGB)
const byte light_neutral[3] = { 0, 50, 50};
const byte light_warm[3] = { 0, 50, 0 };
const byte light_cold[3] = { 0, 0, 50 };
const char* lightTypes[ ] = { "Green&Blue      ", "Green           ", "Blue            " };

// Constants for lux sensor
const tsl2591Gain_t gains[4] = { TSL2591_GAIN_LOW, TSL2591_GAIN_MED, TSL2591_GAIN_HIGH, TSL2591_GAIN_MAX};
const char* gainTexts[] = { "Low-bright light", "Med             ", "High            ", "Max-dim light   " };
const tsl2591IntegrationTime_t timings[6] = { TSL2591_INTEGRATIONTIME_100MS, TSL2591_INTEGRATIONTIME_200MS, TSL2591_INTEGRATIONTIME_300MS, TSL2591_INTEGRATIONTIME_400MS, TSL2591_INTEGRATIONTIME_500MS, TSL2591_INTEGRATIONTIME_600MS};
const char* timingTexts[] = { "100ms bright lig", "200ms", "300ms", "400ms", "500ms", "600ms dim light" };

//Times for print
const byte timesPrint[] = {1, 2, 3, 4, 5, 7, 9, 11, 13, 15};

const byte debounceTime = 100; //debounce time for push button.

#endif
