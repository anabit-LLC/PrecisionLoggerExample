/********************************************************************************************************
This Arduino sketch is meant to work with Anabit's Precision Logger product which features a 
32 bit ADC with a MUX that allows you to route the ADC to 10 single ended channels or 5 differential 
channels. It also includes features like bipolar measurements, built-in current sources (for resistance 
measurements), programmable gain amp, built-in temperature sensor, and more

Product link:
 
This example sketch demonstrates how to make a precision voltage measurement. This sketch uses the awesome 
library that Molorius put together which can be found on Github using the link below. There are more than 
one libary on Github for this ADC so be sure you find the correct one (or just use the link below). The ADS126X
ADC / precision datalogger IC has a lot of different features and settings so it can be a little intimidating to
get started with. But the goal of this example sketch is to make it as painless as possible. To communicate with
the precision logger using this sketch you just need SPI communication pins including chip select. There are 
other pins such as "start' and "ready" that help control ADC conversion timing, but this sketch uses delays to 
handle timing and adjusts delays based on the settings. Note that using the sinc4 filter will lead to a long 
measurement time. The best place to start in the code is the global "PrecisionLoggerSettings" structure, which 
has a lot of the key settings for using the ADC that you can adjust and test out. The first settings in the 
structure are the channels you want to use for your measurement. If you are making a differential measurement 
you will connect both channels to whatever you want to measure. If you are making a single ended measurement 
then just tie the negative channel to ground. If you are making multiple single ended measurements a good approach 
is to use AIN_COM for the ground connections and use that as the ground channel for all your single ended measurements. 

Link to ADS126X.h library on Github: https://github.com/Molorius/ADS126X

Please report any issue with the sketch to the Anagit forum: https://anabit.co/community/forum/analog-to-digital-converters-adcs
Example code developed by Your Anabit LLC © 2025
Licensed under the Apache License, Version 2.0.
********************************************************************************************************/

#include <ADS126X.h>

//ADC settings
#define ADC1_BITS (float)2147483647.0 //this is for 31 bits with the 32nd bit being signed
#define ADC2_BITS (float)8388608.0 //this is for 31 bits with the 32nd bit being signed
#define VREF_2048 (float)2.048 //optional Ext reference voltage
#define VREF_INT (float)2.5 //internal ref voltage
#define PGA_BYPASSED   0b101 //created this value that is not in the library to allow me to know if we want amp bypassed
//Volt ref
#define INT_2_5_VREF 0
#define EXT_2048_VREF 1

ADS126X adc; // start the class

const uint8_t cSelect = 10; //chip select pin for SPI comm, this is required by the sketch to work correctly
float convTime = 0; //holds time (in msec) it takes to complete measurement conversion, calcualted at run time based on ADC settings

//the purpose of this structure is to set all the precision meter settings that will be used while this sketch is running
//adjust this to set measurement pins, PGA, filter type, sample rate, etc
//please note not all sample rates can be used with all the filter settings. See datasheet table 9-13 for more information
struct PrecisionLoggerSettings{ 
    uint8_t posMeasPin = ADS126X_AIN9; //sets positive measurement channel / pin 
    uint8_t negMeasPin = ADS126X_AIN8; //sets negative measurement channel / pin, for single ended measurements tie this pin to ground
    uint8_t sampleRateSetting = ADS126X_RATE_2_5;    // Sample rate in samples per second
    uint8_t filterType = ADS126X_SINC2; //See library file ADS126X_definitions.h for constnat to set filter type
    uint8_t progGainAmp = PGA_BYPASSED; //determines amp gain and allows you to bypass amp (default)
    uint8_t vRef = INT_2_5_VREF; //set voltage reference to internal
    bool choppingEnabled = true;  // Chopping Mode helps improve measurement accuracy but at the cost of more measurement time
    bool iDACRotation = false; //only applicable when using current sensor (for resistance measurements), increases IDAC accuracy but doubles measurement time
} PrecLoggerConfig;


void setup() {
  convTime = calculateADS1262Timing(PrecLoggerConfig); //based on filter, sample rate, and chop mode calculate measurement conversion time, we will use this to pace our measurements
  Serial.begin(115200);
  adc.begin(cSelect); // setup with chip select pin, required by ADC library. ADC library starts SPI communication using default settings
  initLogger(PrecLoggerConfig); //init ADC settings, this function sets up the settings form the structure
  Serial.print("It is measurement time, conversion time based on ADC settings is: "); Serial.println(convTime); //prints out conversion time for each measurement in msec
  discardFirstReadingDelayConTime(convTime,PrecLoggerConfig.posMeasPin,PrecLoggerConfig.negMeasPin); //stsrt ADC, delay, and burn first reading
}

void loop() {
  delay(convTime); // Need to wait at least 1.6 sec for sinc4 filter at lowest sampler rate second
  makeLoggerMeasurement(PrecLoggerConfig.posMeasPin,PrecLoggerConfig.negMeasPin); //get measurement
  //ADC2
  //long bits2 = adc.readADC2(pos_pin,neg_pin); // read the voltage
  //float voltage = getADCtoVoltage(bits2,ADC2_BITS,VREF_INT,1); //get voltage from ADC reading
  //Serial.print("Measured voltage: "); Serial.println(voltage,4); // send voltage through serial
  //adc.readADC2(uint8_t pos_pin,uint8_t neg_pin);
}

//This function starts ADC1 (stops it first), delays for conversion time, 
//and clears buffer by disgarding first reading
//Input argument is ADC conversion time
void discardFirstReadingDelayConTime(float cTime, uint8_t posChan, uint8_t negChan) {
  adc.stopADC1(); //stop ADC 1
  adc.startADC1(); // start conversion on ADC1
  delay(cTime); //delay conversion time
  adc.readADC1(posChan,negChan); //burn first reading to clear buffer
}

//converts ADC reading to voltage, voltage is returned as float
//input arguments are measured ADC value, ADC resolution, ADC ref voltage, volt measurement range
float getADCtoVoltage(int32_t aVal,float bits, uint8_t rVoltSetting) {
  float vRef;
  if(rVoltSetting == INT_2_5_VREF) vRef = VREF_INT; //use internal ref value
  else vRef = VREF_2048;
  return (((float)aVal / bits) * vRef);
}


//function to scale voltage value based on the PGA setting and gain
//input argument is measured voltage value and PGA gain we are in
//returns voltage value based on gain we are in
float scaleVoltMeas4Gain(float vVal, uint8_t gain) {
  float gainFactor;    
  if(gain == ADS126X_GAIN_1 || gain == PGA_BYPASSED) gainFactor = 1; 
  else if(gain == ADS126X_GAIN_2) gainFactor = 2; 
  else if(gain == ADS126X_GAIN_4) gainFactor = 4; 
  else if(gain == ADS126X_GAIN_8) gainFactor = 8; 
  else if(gain == ADS126X_GAIN_16) gainFactor = 16; 
  else gainFactor = 32; 
  return vVal/gainFactor; 
}

//calculates resistance based on measured voltage and current source setting
//returns calculated resistance. Inputs are measured voltage and current source value
float calculate2WireResistance(float measuredVoltage, float currentSource) {
  return measuredVoltage / currentSource; 
}

//settings for ADC1 PGA, allows you to enable or disable and sets gain
//input argument true for enable or false for disable.
//second argument sets gain. If disabled, gain setting is ignored
void setADC1PGA(uint8_t gain) {
  if(gain == PGA_BYPASSED) adc.bypassPGA(); //do not use PGA
  else {
    adc.enablePGA(); //use PGA
    adc.setGain(gain); //set gain of PGA
  }
}

//set voltage reference, input argument is vref you want to use
//For the external ref pins are hard coded, if you have the external ref option be sure the DIP switches are set correctly
void setsUpADCVoltRef(uint8_t voltRef) {
  adc.enableInternalReference(); //turn on internal reference, needed for IDACs
  if(voltRef == INT_2_5_VREF) adc.setReference(ADS126X_REF_NEG_INT, ADS126X_REF_POS_INT); //use internal ref
  else adc.setReference(ADS126X_REF_NEG_AIN3, ADS126X_REF_POS_AIN0); //Set this up based on DIP switch settings, AIN3 tied to analog ground and AIN0 to 2.048V ref
}

//this function sets up the ADC settings
//input argument is the structure with ADC settings
void initLogger(PrecisionLoggerSettings loggerSettings) {
  setsUpADCVoltRef(loggerSettings.vRef); //turn on internal ref and select between int and ext
  setADC1PGA(loggerSettings.progGainAmp); //set default range of meter
  //give time for voltage reference to settle
  delay(50); //give current source and ref time to settle
  adc.calibrateSelfOffsetADC1(); //do internal offset calibration
  if(loggerSettings.choppingEnabled && loggerSettings.iDACRotation) adc.setChopMode(ADS126X_CHOP_3); //11 enables both chop mode and IDAC rotation
  else if(loggerSettings.choppingEnabled)  adc.setChopMode(ADS126X_CHOP_1); //01 enable chop mode, IDAC is off
  else if(loggerSettings.iDACRotation)  adc.setChopMode(ADS126X_CHOP_2); //10 enable IDAC rotation, not chop mode
  else adc.setChopMode(ADS126X_CHOP_0); //00 disable chop mode and IDAC rotation
  adc.setFilter(loggerSettings.filterType); //set ADC1 filter type ADS126X_SINC4
  adc.setRate(loggerSettings.sampleRateSetting); //sets ADC sample rate ADS126X_RATE_2_5
  adc.clearResetBit(); //clear reset bit if it is set
}

//Function to make a voltage reading and print it to serial terminal
//input arguments are the channels the measurements it made on
void makeLoggerMeasurement(uint8_t posMeasChan, uint8_t negMeasChan) {
  int32_t aDCReading; //holds the ADC code
  float valMeas; //holds the calculated voltage value
  aDCReading = adc.readADC1(posMeasChan,negMeasChan); // read the voltage
  valMeas = getADCtoVoltage(aDCReading,ADC1_BITS,PrecLoggerConfig.vRef); //get voltage from ADC reading
  Serial.print("Measured voltage: "); Serial.println(valMeas,6); // send voltage through serial
}

// Function to compute conversion and settling time in milliseconds of ADC1
//input arguments include filter type (reg), data rate (reg), is chop mode enabled
//returns conversion time in milliseconds 
float calculateADS1262Timing(PrecisionLoggerSettings loggerSettings) {
  uint8_t settlingFactor;
  float sampleRate;

  // Identify settling factor for each filter
  switch (loggerSettings.filterType) {
    case ADS126X_FIR:
      settlingFactor = 3;
      break;
    case ADS126X_SINC1:
      settlingFactor = 1;
      break;
    case ADS126X_SINC2:
      settlingFactor = 2;
      break;
    case ADS126X_SINC3:
      settlingFactor = 3;
      break;
    case ADS126X_SINC4:
      settlingFactor = 4;
      break;
    default:
      settlingFactor = 4; //invalid filter type so set to highest settling factor
      break;
  }

   //get sample rate value
  switch (loggerSettings.sampleRateSetting) {
    case ADS126X_RATE_2_5:
      sampleRate = 2.5;
      break;
    case ADS126X_RATE_5:
      sampleRate = 5.0;
      break;
    case ADS126X_RATE_10:
      sampleRate = 10.0;
      break;
    case ADS126X_RATE_16_6:
      sampleRate = 16.6;
      break;
    case ADS126X_RATE_20:
      sampleRate = 20.0;
      break;
    case ADS126X_RATE_50:
      sampleRate = 50.0;
      break;
    case ADS126X_RATE_60:
      sampleRate = 60.0;
      break;
    case ADS126X_RATE_100:
      sampleRate = 100.0;
      break;
    case ADS126X_RATE_400:
      sampleRate = 400.0;
      break;
    case ADS126X_RATE_1200:
      sampleRate = 1200.0;
      break;
    case ADS126X_RATE_2400:
      sampleRate = 2400.0;
      break;
    case ADS126X_RATE_4800:
      sampleRate = 4800.0;
      break;
    case ADS126X_RATE_7200:
      sampleRate = 7200.0;
      break;
    case ADS126X_RATE_14400:
      sampleRate = 14400.0;
      break;
    case ADS126X_RATE_19200:
      sampleRate = 19200.0;
      break;
    case ADS126X_RATE_38400:
      sampleRate = 38400.0;
      break;
    default:
      sampleRate = 2.5; //invalid filter type so set to highest settling factor
      break;
  }

  float Tconv_ms = 1000.0 / sampleRate;  // Base conversion period in ms

  // Calculate settling time
  //there is a ~400usec delay at any filter setting beyond the conversion rate
  Tconv_ms = (Tconv_ms * settlingFactor) + 1; //add one because most filters have a ~400usec delay

  // Double conversion period if chopping mode is enabled
  if (loggerSettings.choppingEnabled) {
    Tconv_ms *= 2.0;  // Chopping mode A doubles conversion time
  } 
  //double conversion period if IDAC rotation mode is enabled
  if (loggerSettings.iDACRotation) {
    Tconv_ms *= 2.0;  // Chopping mode A doubles conversion time
  } 

  return Tconv_ms; //return delay time between measurements
}

