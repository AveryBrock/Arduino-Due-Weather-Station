My name is Avery Brock and I am currently a Freshman at the University of Idaho majoring in Electrical Engineering. I have been interested in electronics and science from a very young age and I have participated many robotics events including the MATE ROV competition and FIRST FRC. In my spare time I like to tinker and work on random projects which I hope to document and publish more often then I have been. 

#Arduino-Due-Weather-Station
Arduino Due based weather station, data logger and grapher with a TFT display. 

Dorm Weather Station code by Avery Brock. The original concept was to take the readings from an indoor and an outdoor
   temperature sensor and be able to display the current temperature, time, highs and lows.

   Code is based on several libraries, see code for details and licenses for
   each. Thank you to the companies and people that wrote them!


   Current hardware-
    Arduino Due 32-bit 84MHz ARM SAM3X8E
 *  
    BMP180 atmospheric pressure sensor (I2C channel 20,21)
*
    AM2302 temperature and humidity sensor (oneWire) for indoor temp on 52
*
    AM2302 temperature and humidity sensor (oneWire) for outdoor temp on 51
*
    DS3234 Real Time Clock module (SPI- using Due extended SPI capabilities), CS on pin 4
 *  
    ILI9341 2.8" TFT display (SPI -using main Due SPI header)

  Current Code Capabilities-
  Takes readings from all sensors and displays them on the TFT. However, only values that have changed are
  re-printed on the display to minimize flickers. Functions have been made for each data 'module' (printTime,
  printDate, printTemp..) to make it easy to change the display arrangement. All data is also sent out via serial.

  SD card on TFT breakout logs data at a set interval. Data is stored in CSV format for excel compatablility.
  New data files are created each day and a count of how many datapoints have been taken is also stored on the
  SD card in a .txt file. Count is reset when a new data file is started. If the SD card is missing or has no files to read
  files are either created or the system simply does not log. All data and data files are timestamped.

  Graphing function reads the current day's data file and scales the data based on the number of datapoints and
  the calculated highs and lows. High points are traced in red, lows in blue. Highs and lows are also labeled on
  the side of the graph to give scale.

