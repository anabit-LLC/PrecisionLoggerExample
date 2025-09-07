# PrecisionLoggerExample
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
