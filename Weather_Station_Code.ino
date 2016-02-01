/*
   Dorm Weather Station code by Avery Brock. The original concept was to take the readings from an indoor and an outdoor
   temperature sensor and be able to display the current temperature, time, highs and lows.

   Code is based on several libraries, see below for details and licenses for
   each. Thank you to the companies and people that wrote them!


   Current hardware-
    Arduino Due 32-bit 84MHz ARM SAM3X8E
 *  *
    BMP180 atmospheric pressure sensor (I2C channel 20,21)
    AM2302 temperature and humidity sensor (oneWire) for indoor temp on 53
    AM2302 temperature and humidity sensor (oneWire) for outdoor temp on 51
    DS3234 Real Time Clock module (SPI- using Due extended SPI capabilities), CS on pin 4
 *  *
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

  Print Temperature function takes readings and changes font color based on the temperature reading

  All functions are prototyped and explained below:

  RTC_init();          initializes the RTC
  ReadTimeDate();             Returns the output of the RTC module as a string- used in Time, Date and Data functions
  SetTimeDate(int d, int mo, int y, int h, int mi, int s);   Use to set the RTC, enter current day, month, year, hour, minute, second

  TakeTemp("Decimal"); Use to get the current temperature and humidity from BOTH sensors- enter "Decimal" or "Rounded", used in print temp function
  TakePressure("Decimal"); Use to get the current pressure, enter "Decimal" or "Rounded", used in print pressure function

  gatherData(); Takes readings from all sensors, timestamps them and saves them to the datalog file on the SD

  MainPage();
  DataPage();   Display pages. Main() cycles through them
  DataPage2();

  GraphData(); //Graph data function. Reads the SD file and count file, stores data in an array, determines the
              highs and lows and overall high and overall low, scales data to fit and graphs it. Highs and lows from
               each source are listed on the graph to give scale

  printDate(0, 0, 3, ILI9341_WHITE);  Function to print date (X location, Y location, font size, font color (16 bit pre-defined in Adafruit GFX library)

  printTime(30, 40, 6, ILI9341_WHITE);  Function to print Time (X location, Y location, font size, font color (16 bit pre-defined)

  printTemp(30, 150, 5, "Outside", "Rounded"); Function to print Temperature (X location, Y location, font size, sensor name ("Inside" or "Outside"), data type ("Decimal" or "Rounded")

  printPressure(30, 200, 3, "Decimal", ILI9341_GREENYELLOW);  Function to print pressure (X location, Y location, font size, data type ("Decimal" or "Rounded"), font color

  printHumidity(0, 40, 6, "Inside", ILI9341_OLIVE, "Decimal"); Function to print humidity (X location, Y location, font size, sensor name ("Inside" or "Outside"), color, data type ("Decimal" or "Rounded")


  Credits for ILI9341 TFT Library. Thanks Adafruit!
  /***************************************************
  This is our GFX example for the Adafruit ILI9341 Breakout and Shield
  ----> http://www.adafruit.com/products/1651

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ***************************************************


  Credits for BMP180 Library. Thanks SparkFun!
 *****************************************************************
  This sketch shows how to use the Bosch BMP180 pressure sensor
  as an altimiter.
  https://www.sparkfun.com/products/11824
  The SFE_BMP180 library uses floating-point equations developed by the
  Weather Station Data Logger project: http://wmrx00.sourceforge.net/
  Our example code uses the "beerware" license. You can do anything
  you like with this code. No really, anything. If you find it useful,
  buy me a beer someday.
  V10 Mike Grusin, SparkFun Electronics 10/24/2013
  V1.1.2 Updates for Arduino 1.6.4 5/2015

* *Credits for AM2302 Library. Thanks Adafruit!
 *****************************************************************
  Example testing sketch for various DHT humidity/temperature sensors
  Written by ladyada, public domain
*/


#include <SPI.h>                //Arduino SPI communications library
#include <Adafruit_GFX.h>       // Adafruit's graphics library for the TFT
#include <Adafruit_ILI9341.h>   // Adafruit's library for the TFT
#include <DHT.h>                //Adafruit's library for the temperature sensors
#include <SFE_BMP180.h>         //SparkFun's library for the pressure sensor
#include <Wire.h>               //Arduino Wire communications library
#include <SD.h>                 //Arduino SD communication library
//*****************************

//******************************
//Okay, time to set up the display

#define TFT_DC 9      //D/C goes to pin 9
#define TFT_CS 10     //CS goes to 10
int backlight = 50;  //I hooked the LED/Backlight pin through a PNP transistor. Gate is wired fr to pin 50 with a 1K pulldown resistor to ground
//Be aware that this method gives you backlight control, but in reverse. Setting the pin high turns it off, pin low turns it on

// Use hardware SPI header on the Due (2*3 right next to the chip) for MOSI, MISO, CLK, RST, see Arduino page for pinout
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

//************************

// Set up for the SD card

const int SDchipSelect = 8; //Chip select pin
const int LEDpin = 13;      //Set up so the arduino LED flashes after a succesful save (pin 13 LED)

//*******************************
//Now lets set up the RTC module
int TimeDate [7], lastTimeDate[7]; //Two arrays to keep track of the time and what was the time

//******************************
//Now that the base components are set up, lets add the temp sensors

#define DHTPIN1 51     // Connect the signal output of each sensor to a different digital pin, in this case 52, 53
#define DHTPIN2 53    //As mine are on LONG cables ~6', I wired them to 5v with a 10k pullup resistor to +5
//As my cables are long enough, I measure the signal output to be around 2v, safe for the due

#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321, the type of sensor being used
DHT dht1(DHTPIN1, DHTTYPE); //create two sensor objects named dht1, dht2.
DHT dht2(DHTPIN2, DHTTYPE); //they are on different pins
double TempInfo[6], lastTempInfo[6]; //and lets create two arrays to store the temperature and what was the temperature

//****************************
//Lets then add the pressure sensor
SFE_BMP180 pressure;          //create a pressure object
#define ALTITUDE 786.08 // Altitude of the University of Idaho. in meters
double Pressure, lastPressure;  //and lets create two doubles to store the pressure and what was the pressure

unsigned long previousMillis = 0;      // will store last time LED was updated

// constants won't change :
const long interval = 30000;           // interval at which to change the display page (milliseconds)
int displayPage = 0;                  //startup page is the home page (page 0);

int dataPoints;     //Count for the datapoints taken

double MinTemp1, MaxTemp1, MinTemp2, MaxTemp2, MinHumid1, MaxHumid1, MinHumid2, MaxHumid2, MinPressure, MaxPressure;  //Data points for the graph function
int MinT1Time, MaxT1Time, MinT2Time, MaxT2Time, MinH1Time, MaxH1Time, MinH2Time, MaxH2Time, MinP, MaxP, numDataPoints;

//Okay, now that we have things defined, lets run setup
//Setup overview: runs setup of all sensors, TFT and SD card. Prints progress in classic green.

//=========================================================================================
void setup() {

  Serial.begin(9600);                 //start a serial output
  Serial.println("Okay, we are on!"); //print out a test line
  pinMode(backlight, OUTPUT);         //Set the backlight pin as an output
  digitalWrite(backlight, LOW);       //Turn on the backlight
  pinMode(LEDpin, OUTPUT);            //Set the SD indicator as an output
  digitalWrite(LEDpin, LOW);           //make sure the LED is off

  tft.begin();                        //initialize the TFT display
  tft.setRotation(3);   //**********IMPORTANT LINE- This sets the display in landscape mode. Input 1-4 to change rotation
  tft.fillScreen(ILI9341_BLACK);      // Clear the screen and set the background to black
  tft.setTextColor(ILI9341_GREEN);    //Lets use green font
  tft.setTextSize(2);                 // set the text size to 2

  RTC_init();                         //intialize the RTC module- see function below for details - on extended SPI ch 4
  //SetTimeDate(29,1,16,20,33,20);    //uncomment to program the time into the RTC, (Day, Month, Year, hour (24hr), minute, second )
  Serial.println("RTC INT");
  tft.println("RTC INT");             //troubleshoot/startup lines for serial and the TFT
  dht1.begin();                       //initialize one temp sensor
  Serial.println("DHT1 INT");
  tft.println("DHT1 INT");            //troubleshoot/startup lines for the TFT
  dht2.begin();                       //initialize the other temp sensor
  Serial.println("DHT2 INT");         //troubleshoot/startup lines for serial and the TFT
  tft.println("DHT2 INT");

  pressure.begin();                    //initialize the pressure sensor
  Serial.println("Pressure INT");      //troubleshoot/startup lines for serial and the TFT
  tft.println("Pressure INT");

  //Okay, so lets try to talk to the screen. If this doesn't work, check the SPI connections (you need RST connected) and libraries.
  // read diagnostics (optional but can help debug problems)
  
  uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX); //Hex readouts should not be 0
  tft.print("Display Power Mode: 0x"); tft.println(x, HEX);     //troubleshoot/startup lines for serial and the TFT
  x = tft.readcommand8(ILI9341_RDMADCTL);
  Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
  tft.print("MADCTL Mode: 0x"); tft.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDPIXFMT);
  Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
  tft.print("Pixel Format: 0x"); tft.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDIMGFMT);
  Serial.print("Image Format: 0x"); Serial.println(x, HEX);
  tft.print("Image Format: 0x"); tft.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDSELFDIAG);
  Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX);
  tft.print("Self Diagnostic: 0x"); tft.println(x, HEX);

  ReadTimeDate();                       //Lets get our first reading from the RTC. See function for details on how
  Serial.println("Reading the RTC");    //troubleshoot/startup lines for serial and the TFT
  tft.println("Reading the RTC");
  for (int i = 0; i < 7; i++) {         //our data array is 7 integers long (month, day, year, hrs, min, sec)
    lastTimeDate[i] = TimeDate[i];      //Data from ReadTimeDate() is saved in TimeDate[]. We want to save this as our previous values to
    Serial.println(lastTimeDate[i]);
  }

  TakeTemp("Decimal");                       //Lets get our first readings from BOTH temperature sensors. See function for details on how
  Serial.println("Reading Temp Sensors");   //troubleshoot/startup lines for serial and the TFT
  tft.println("Reading Temp Sensors");
  for (int j = 0; j < 6; j++) {             //our data array is 6 integers long (temp1, temp2, humidity1, humidity2, feelslike1, feelslike2)
    lastTempInfo[j] = TempInfo[j];          //Data from TakeTemp() is saved in TempInfo[]. We want to save this as our previous values to
    Serial.println(lastTempInfo[j]);        //compare the next ones to.
  }
  TakePressure("Decimal");                  //Lets get our first readings from the pressure sensor. See function for details on how
  Serial.println("Reading Temp Sensors");   //troubleshoot/startup lines for serial and the TFT
  tft.println("Reading Temp Sensors");
  lastPressure = Pressure;                  //Since all we want is the compensated pressure, we only need one variable.
  Serial.println(lastPressure);             //Save the reading to compare the next ones to.

  Serial.print("\nInitializing SD card...");    //troubleshoot/startup lines for serial and the TFT
  tft.println("Initializing SD card...");

  if (!SD.begin(SDchipSelect)) {                    //Start communication with the SD card
    Serial.println("Card failed, or not present");  //troubleshoot/startup lines for serial and the TFT
    tft.println("Card failed, or not present");
  }

  Serial.println("card initialized.");              //troubleshoot/startup lines for serial and the TFT
  tft.println("card initialized.");

  delay(700);
  tft.fillScreen(ILI9341_BLACK);  //okay, clear the screen
  tft.setCursor(0, 0);            //reset cursor position

  File myFile;
  myFile = SD.open("count.txt");  //Try to open the count file. If it isn't there, make one
  if (myFile) {
    Serial.println("test.txt:");  //troubleshoot/startup lines for serial and the TFT
    tft.println("count.csv:");
    dataPoints = myFile.parseInt(); //read the file, convert the text to an int to get the initial count. This way you can turn it off and it will just continue the count
    tft.print(dataPoints); tft.println(":data points on SD");   //troubleshoot/startup lines for serial and the TFT
    myFile.close(); //close the file
  }

  else {
    Serial.println("error opening test.txt");                 //if the file didn't open, print an error:
    tft.println("error opening count.csv");                   //troubleshoot/startup lines for serial and the TFT
    dataPoints = 0;                                           //if no file, default to zero
    tft.print(dataPoints); tft.println(":data points on SD"); //troubleshoot/startup lines for serial and the TFT
  }

  tft.println("Initiation Completed"); //troubleshoot/startup lines for serial and the TFT
  delay(700);
  tft.fillScreen(ILI9341_BLACK);      //okay, we are all done with setup, lets clear the screen

  gatherData();                      // Take the first data reading and save it to the SD
}


////======================================================================= Okay, now its time to do something
void loop() {

  if ((lastTimeDate[2] > 22) || (lastTimeDate[2] < 7 )) //This if/else loop reads the hour last read from the RTC module, if it is 11PM or later
    digitalWrite(backlight, HIGH);                    //or earlier than 7PM, it turns off the display backlight. Otherwise it turns it on
  else digitalWrite(backlight, LOW);                  //brackets are not needed after if/else as the run line is only one line
    
  while (displayPage == 0) {                                  // Code block for one of the pages, in this case page 0
    unsigned long currentMillis = millis();                   // since this is a while loop, we need the timer inside of it
    MainPage();                                               //  call the mainPage graphic function
    if (currentMillis - previousMillis >= (interval * 1.5)) { //  after 45 seconds, change the page
      previousMillis = currentMillis;                         //  reset the timer
      tft.fillScreen(ILI9341_BLACK);                          //clear the screen
      displayPage = 1;                                        //go to the next page display
    }
  }

  while (displayPage == 1) {                      //second display page
    unsigned long currentMillis = millis();
    DataPage();
    if (currentMillis - previousMillis >= (interval * .5)) {
      // save the last time you blinked the LED
      previousMillis = currentMillis;
      tft.fillScreen(ILI9341_BLACK);
      displayPage = 2;
    }
  }

  while (displayPage == 2) {                //third display page
    unsigned long currentMillis = millis();
    DataPage2();
    if (currentMillis - previousMillis >= (interval * .5)) {
      // save the last time you blinked the LED
      previousMillis = currentMillis;
      tft.fillScreen(ILI9341_BLACK);

      displayPage = 3;
    }
  }

  if (displayPage == 3) {             //since we only want the graph to calculate and display once, we will use an if statement
    GraphData();
    displayPage = 4;
  }


  while (displayPage == 4) {        //this function simply does not clear the screen for 45 seconds, so the graph stays up
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval * 1.5) {
      // save the last time you blinked the LED
      previousMillis = currentMillis;
      tft.fillScreen(ILI9341_BLACK);

      displayPage = 0;
      gatherData();         //function to read values and save to SD. This way it only takes a data point about every 2 minutes
    }
  }
}
//And thats it for setup and main! Below are all the functions doing the heavy lifitng.
//////////===================================================================

void MainPage() {
  printDate(0, 0, 3, ILI9341_WHITE);  //Print the date in the top left corner

  printTime(30, 40, 6, ILI9341_WHITE); //Print the time below the date

  printTemp(30, 100, 5, "Inside", "Rounded"); //Print one temp
  printTemp(30, 150, 5, "Outside", "Rounded"); //Print the other temp below the first

  //Okay, now its time to label those numbers
  tft.setTextColor(ILI9341_WHITE); //Lets use white font
  tft.setTextSize(3);              //Size 3, so its smaller than the temperature readouts
  tft.setCursor(150, 115);         //Position for the first reading label, you may need to move this around
  tft.print("Inside");             //This sensor is measuring the inside temperature, so I label it 'inside'
  tft.setCursor(150, 165);         //Position for the second reading label, you may need to move this around
  tft.print("Outside");            //This sensor is measuring the outside temperature, so I label it 'outside'
  tft.setCursor(0, 200);
  tft.setTextColor(ILI9341_OLIVE);
  tft.print("Data Points:"); tft.print(dataPoints); //at the bottom, print the number of data points taken
}

void DataPage() {
  printTime(0, 0, 4, ILI9341_WHITE);          //print the time in the corner

  printTemp(0, 40, 6, "Inside", "Decimal");   //On this screen, print out the full reading by calling "Decimal" rather than "Rounded"
  printTemp(0, 120, 6, "Outside", "Decimal");

  printPressure(30, 200, 3, "Decimal", ILI9341_GREENYELLOW);   //Print the pressure at the bottom

  tft.setTextColor(ILI9341_WHITE); //Lets use white font
  tft.setTextSize(3);              //size 3, so its smaller than the temperature readouts
  tft.setCursor(0, 90);            //position for the first reading label, you may need to move this around
  tft.print("Inside");             //This sensor is measuring the inside temperature, so I label it 'inside'
  tft.setCursor(0, 170);           //position for the second reading label, you may need to move this around
  tft.print("Outside");
}

void DataPage2() {
  printTime(0, 0, 4, ILI9341_WHITE);    //print the time in the corner

  tft.setTextColor(ILI9341_LIGHTGREY);  //label the page "humidity"
  tft.setTextSize(3);
  tft.setCursor(150, 0);
  tft.print("Humidity");

  printHumidity(0, 40, 6, "Inside", ILI9341_OLIVE, "Decimal");
  printHumidity(0, 120, 6, "Outside", ILI9341_OLIVE, "Decimal");

  tft.setTextColor(ILI9341_WHITE);//Lets use white font
  tft.setTextSize(3);             //size 3, so its smaller than the temperature readouts
  tft.setCursor(0, 90);           //position for the first reading label, you may need to move this around
  tft.print("Inside");            //This sensor is measuring the inside temperature, so I label it 'inside'
  tft.setCursor(0, 170);          //position for the second reading label, you may need to move this around
  tft.print("Outside");
}


//----------------Graphing function, does the SD reading, math and printing
void GraphData() {

  Serial.println("getting graph data");
  String tempName = "", dataLine = "", underscore = "_"; //set up some strings

  File myFile;
  myFile = SD.open("count.txt");  //open the count file

  if (myFile) {
    Serial.println("test.txt:");
    dataPoints = myFile.parseInt(); //figure out how many data points to expect
    Serial.println(dataPoints);
    myFile.close();   //close the count file
  }

  tft.setTextColor(ILI9341_GREEN); //Green troubleshoot text page, clears in .5 seconds, lets you know if something isn't reading right
  tft.setTextSize(2);
  tft.setCursor(0, 0);   //reset cursor position
  tft.println("Opening card data..");

  tempName.concat(TimeDate[5]); //using the last saved time from the RTC (called before this in a printTime function), piece together
  tempName.concat(underscore);                  //the name of the file to open day_month_year.csv
  tempName.concat(TimeDate[4]);
  tempName.concat(underscore);
  tempName.concat(TimeDate[6]);
  tempName.concat(".csv");        //comma separated values file type, excel readable

  double temp1[dataPoints], temp2[dataPoints], humid1[dataPoints], humid2[dataPoints], pressure0[dataPoints]; //set up arrays to save the SD info in. This is why we figured out how many data points there are

  long placeholder;  //something to hold the data we don't use in (time and date, since we have that)
  int i = 0;  //array count

  File dataFile = SD.open(tempName.c_str());  //open the data file for the day
  tft.println(tempName);

  if (dataFile) {
    tft.println("Reading card data..");

    while (i < dataPoints) {
      placeholder = dataFile.parseInt();    //since we know how the data saved, we know what order it is in and how to parse it
      placeholder = dataFile.parseInt();
      placeholder = dataFile.parseInt();
      placeholder = dataFile.parseInt();
      placeholder = dataFile.parseInt();
      placeholder = dataFile.parseInt();
      humid1[i] = dataFile.parseFloat();
      humid2[i] = dataFile.parseFloat();
      temp1[i] = dataFile.parseFloat();
      temp2[i] = dataFile.parseFloat();
      pressure0[i] = dataFile.parseFloat();
      i++;   //add the count
    }

    tft.println("Closing File");
    dataFile.close();   //close the file
    numDataPoints = i;
    tft.print("Read:"); tft.print(numDataPoints); tft.println(" datapoints");
  }

  else {
    tft.println("File Error");                  //if the file didn't open, lets make there be a way of knowing what happened
    Serial.println("error opening datalog.txt");
  }

  tft.println("Calculating..");

  MinTemp1 = temp1[0];    //Plug in some inital values for the highs and the lows, that way we know they are in the right range
  MaxTemp1 = MinTemp1;
  MinTemp2 = temp2[0];
  MaxTemp2 = MinTemp2;
  MinHumid1 = humid1[0];
  MaxHumid1 = MinHumid1;
  MinHumid2 = humid2[0];
  MaxHumid2 = MinHumid2;
  MinPressure = pressure0[0];
  MaxPressure = MinPressure;

  for (int j = 1; j < (dataPoints); j++) {    //This is just a big for loop that checks all the other data points to see if any are higher or lower
    if (temp1[j] <= MinTemp1) {
      MinTemp1 = temp1[j];
      MinT1Time = j;
    }
    else if (temp1[j] >= MaxTemp1) {
      MaxTemp1 = temp1[j];
      MaxT2Time = j;
    }

    if (temp2[j] <= MinTemp2) {
      MinTemp2 = temp2[j];
      MinT2Time = j;
    }
    else if (temp2[j] >= MaxTemp2) {
      MaxTemp2 = temp2[j];
      MaxT2Time = j;
    }

    if (humid1[j] <= MinHumid1) {
      MinHumid1 = humid1[j];
      MinH1Time = j;
    }
    else if (humid1[j] >= MaxHumid1) {
      MaxHumid1 = humid1[j];
      MaxH2Time = j;
    }

    if (humid2[j] <= MinHumid2) {
      MinHumid2 = humid2[j];
      MinH2Time = j;
    }
    else if (humid2[j] >= MaxHumid2) {
      MaxHumid2 = humid2[j];
      MaxH2Time = j;
    }

    if (pressure0[j] <= MinPressure) {
      MinPressure = pressure0[j];
      MinP = j;
    }
    else if (pressure0[j] >= MaxPressure) {
      MaxPressure = pressure0[j];
      MaxP = j;
    }                                                   //At this point we should have a high and low value for each sensor and each data type
  }

  Serial.print("MinTemp1:"); Serial.println(MinTemp1);
  Serial.print("MaxTemp1:"); Serial.println(MaxTemp1);
  Serial.print("MinTemp2:"); Serial.println(MinTemp2);
  Serial.print("MaxTemp2:"); Serial.println(MaxTemp2); //Print out the values so we know they all worked

  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(2);
  tft.setCursor(0, 100);
  tft.print("Min Inside:"); tft.print(MinTemp1); tft.print(","); tft.println(MinT1Time);  //Print out the values so we know they all worked
  tft.print("Max Inside:"); tft.print(MaxTemp1); tft.print(","); tft.println(MaxT1Time);
  tft.print("Min Outside:"); tft.print(MinTemp2); tft.print(","); tft.println(MinT2Time);
  tft.print("Max Outside:"); tft.print(MaxTemp2); tft.print(","); tft.println(MaxT2Time);

  delay(500);  //lets leave it up so we can actualy read it
  tft.fillScreen(ILI9341_BLACK); //clear the screen


  ///---Actual graphic graphing section-----------------

  int x1, y1, x2, y2;  //set up some screen locations

  tft.setTextColor(ILI9341_OLIVE); //Lets use greenish for the inside graph
  tft.setTextSize(3);
  tft.setCursor(55, 220);         // position for the second reading label, you may need to move this around
  tft.print("Inside ");
  tft.setTextColor(ILI9341_DARKGREY); //Lets use grey for the outside graph
  tft.setCursor(165, 220);            //position for the second reading label, you may need to move this around
  tft.print(" Outside");

  int absMin, absMax;                       //to scale the y-axis, we need to know what our absolute lowest and higest readings are from both sensors
  if (MinTemp2 < MinTemp1)absMin = MinTemp2;    //compare the lows
  else absMin = MinTemp1;

  if (MaxTemp1 > MaxTemp2)absMax = MaxTemp1;  //compare the highs
  else absMax = MaxTemp2;

  int xScale, xPos = 35, skipScale;   //y-axis is set up, lets make some x-axis variables

  if (numDataPoints > 290) {            //since the screen is 240*320, we can't display every data point if there are more than 290 (label space)
    skipScale = numDataPoints / 290;    //figure out how many to skip
    xScale = (290 / (numDataPoints / skipScale));   //figure out an even spacing distance
  }
  else if (numDataPoints <= 290) {    //if there are less than 290, we can display all of them or we may need to spread them out
    skipScale = 0;                    //we don't need to skip any
    xScale = (290 / numDataPoints);   //figure out an even spacing
  }
  if (xScale < 1) xScale = 1;   //since we only have whole pixels, the smallest increment we can do is 1

  for (int a = 0; a < (numDataPoints - 1 - skipScale); (a = a + 1 + skipScale)) {   //For loop. Prints all the lines between data points.
    int y1T1 =  map(temp1[a], absMin, absMax, 200, 10);                             //map the temperature to the abs max and min
    int y2T1 =  map(temp1[(a + 1 + skipScale)], absMin, absMax, 200, 10);           //the next reading is one reading more plus however many we skip. map that one too.
    int x1T1 = xPos;                                                                //the first x coordinate
    int x2T1 = xPos + xScale;                                                       //move along the x axis our scale distance
    //Draw the lines between our two points
    if ((temp1[a] == MinTemp1) || (temp1[(a + 1 + skipScale)] == MinTemp1)) tft.drawLine(x1T1, y1T1, x2T1, y2T1, ILI9341_CYAN); //if a point is a low, make it blue
    else if ((temp1[a] == MaxTemp1) || (temp1[(a + 1 + skipScale)] == MaxTemp1)) tft.drawLine(x1T1, y1T1, x2T1, y2T1, ILI9341_RED); //if a point is a hugh, make it red
    else tft.drawLine(x1T1, y1T1, x2T1, y2T1, ILI9341_OLIVE); //if its just a point, make it green to match our legend
    xPos = xPos + xScale; //the x position of the next dot will be the x position of our last dot. That way they connect. We will just re-plot our second dot as our new first one

  }

  xPos = 35;  //IMPORTANT--RESET THE X-POS FOR THE SECOND LINE

  for (int a = 0; a < (numDataPoints - 1 - skipScale); (a = a + 1 + skipScale)) {  //do the above but for the other sensor
    int y1T2 =  map(temp2[a], absMin, absMax, 200, 10);
    int y2T2 =  map(temp2[(a + 1 + skipScale)], absMin, absMax, 200, 10);
    int x1T2 = xPos;
    int x2T2 = xPos + xScale;
    if ((temp2[a] == MinTemp2) || (temp2[(a + 1 + skipScale)] == MinTemp2))tft.drawLine(x1T2, y1T2, x2T2, y2T2, ILI9341_CYAN);
    else if ((temp2[a] == MaxTemp2) || (temp2[(a + 1 + skipScale)] == MaxTemp2))tft.drawLine(x1T2, y1T2, x2T2, y2T2, ILI9341_RED);
    else tft.drawLine(x1T2, y1T2, x2T2, y2T2, ILI9341_DARKGREY);
    xPos = xPos + xScale;
  }

  int yLow1, yHigh1, yLow2, yHigh2;   //okay, now we need a y scale


  yLow1    = map(MinTemp1, absMin, absMax, 200, 10);  //figure out the y location of all the highs and lows. Just repeat what we did above
  yHigh1    = map(MaxTemp1, absMin, absMax, 200, 10);
  yLow2    = map(MinTemp2, absMin, absMax, 200, 10);
  yHigh2    = map(MaxTemp2, absMin, absMax, 200, 10);

  tft.setTextColor(ILI9341_CYAN); //Lets use blue font for lows
  tft.setTextSize(2);
  tft.setCursor(0, yLow1);       //go the the correct height
  tft.print(MinTemp1);           //print the low
  tft.setCursor(0, yLow2);       //repeat for other line
  tft.print(MinTemp2);

  tft.setTextColor(ILI9341_RED);    //repeat but for the highs in red
  tft.setCursor(0, yHigh1);
  tft.print(MaxTemp1);
  tft.setCursor(0, yHigh2);
  tft.print(MaxTemp2);
}
//And your data is graphed!

///////////////---------------PRINT DATE------------//////////////////////////////////////
void printDate(int x, int y, int s, uint16_t color) {
  tft.setTextSize(s); //set the text size the the input value
  ReadTimeDate();   //get the current date

  String day, month, slash;  //set up some strings

  slash = "/";  //define what a slash is

  if (TimeDate[5] != lastTimeDate[5]) {   //check to see if the month changed
    tft.setTextColor(ILI9341_BLACK);      //this is weird, but to clear the screen we will 'erase' the last month by printing it in black
    tft.setCursor(x, y);                  //go the the initial coordinates
    month = lastTimeDate[5] + slash;
    tft.print(month);                     //print the last time and date
    lastTimeDate[5] = TimeDate[5];        //save it as the new data
  }

  tft.setTextColor(color);                //now we can print the real month
  tft.setCursor(x, y);
  month = lastTimeDate[5] + slash;
  tft.print(month);

  if (TimeDate[4] != lastTimeDate[4]) {   //repeat the same thing for day and year
    SD.remove("count.txt");               //----SD data- start a new count file if it is a new day
    dataPoints = 0;                       //-reset the number of datapoints
    tft.setCursor(x, y);
    tft.setTextColor(color);
    tft.print(month);                     // leave the unchanged data alone
    tft.setTextColor(ILI9341_BLACK);
    day = lastTimeDate[4] + slash;
    tft.print(day);
    lastTimeDate[4] = TimeDate[4];
  }

  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.print(month);                       //we need to reprint the day so our spacing works out
  month = lastTimeDate[4] + slash;
  tft.print(day);

  if (TimeDate[6] != lastTimeDate[6]) {
    tft.setCursor(x, y);
    tft.setTextColor(color);
    tft.print(month);
    tft.print(day);
    tft.setTextColor(ILI9341_BLACK);
    tft.print(lastTimeDate[6]);
    lastTimeDate[6] = TimeDate[6];
  }

  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(month);                         //reprint both so the spacing works out
  tft.print(day);
  tft.print(lastTimeDate[6]);
}
///////////////////////////---PRINT TIME---///////////////////////////////////

void printTime(int x, int y, int s, uint16_t color) {     //this function works just like print date
  tft.setTextSize(s);
  ReadTimeDate();

  String hour, minute, zero, colon, Time, TDay;
  zero = "0";   //we need a zero in case the hour or minute is a singe digit number
  colon = ":";

  int hour12, checkTDay = 0;

  if ((TimeDate[2] >= 0) && (TimeDate[2] <= 11)) TDay = "AM";
  else  TDay = "PM";

  if (TimeDate[2] != lastTimeDate[2]) {
    checkTDay = 1;                        //the hour changed, which means we need to check AM/PM (see below)
    tft.setTextColor(ILI9341_BLACK);
    tft.setCursor(x, y);
    if (lastTimeDate[2] > 12) hour12 = (lastTimeDate[2]) - 12;    //convert the time into 12 hr time
    else if (lastTimeDate[2] == 0) hour12 = 12;                   // hour zero is 12AM
    else if (lastTimeDate[2] <= 12) hour12 = lastTimeDate[2];     //if the time is less than 12, leave it alone
    hour = hour12 + colon;  //add in the colon
    tft.print(hour);
    lastTimeDate[2] = TimeDate[2];
  }

  tft.setTextColor(color);
  tft.setCursor(x, y);
  if (lastTimeDate[2] > 12) hour12 = (lastTimeDate[2]) - 12;  //we need to do the math again so everything lines up
  else if (lastTimeDate[2] == 0) hour12 = 12;
  else if (lastTimeDate[2] <= 12) hour12 = lastTimeDate[2];
  hour = hour12 + colon;
  tft.print(hour);

  if (TimeDate[1] != lastTimeDate[1]) {   //minute is a little different
    tft.setCursor(x, y);
    tft.setTextColor(color);
    tft.print(hour);
    tft.setTextColor(ILI9341_BLACK);
    minute = lastTimeDate[1];
    if (lastTimeDate[1] < 10) minute =  zero + lastTimeDate[1]; // if the minute is a singe digit, add a zero in front of it so it looks right
    else minute = lastTimeDate[1];
    tft.print(minute);
    lastTimeDate[1] = TimeDate[1];
  }

  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.print(hour);      //since we all ready saved hour, we don't need to redo the math
  if (lastTimeDate[1] < 10) minute =  zero + lastTimeDate[1];
  else minute = lastTimeDate[1];
  tft.print(minute);

  if (checkTDay == 1) {   //AM/PM labels, erases old label
    tft.setCursor(x, y);
    tft.setTextColor(color);
    tft.print(hour);
    tft.print(minute);
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextSize(abs(s - 2));    //make the AM/PM smaller
    if (TDay == "PM") tft.print("AM");
    else tft.print("PM");
  }

  tft.setCursor(x, y);
  tft.setTextColor(color);    //final print of everything
  tft.setTextSize(s);
  tft.print(hour);
  tft.print(minute);
  if (TDay == "PM")  tft.setTextColor(ILI9341_ORANGE);
  if (TDay == "AM")  tft.setTextColor(ILI9341_BLUE);
  tft.setTextSize(abs(s - 2));
  tft.print(TDay);
}

////////////////////////////----RTC INIT, credits to SFE ------//////////////////////////////////////
//THIS DEVICE IS USING SPI CH4. CS GOES TO PIN 4, ALL OTHERS ARE SHARED (MOSI, MISO, SCK)

int RTC_init() {
  //pinMode(cs,OUTPUT); // chip select
  // start the SPI library:
  SPI.begin(4);
  SPI.setBitOrder(4, MSBFIRST);
  SPI.setDataMode(4, SPI_MODE3); // both mode 1 & 3 should work
  //set control register
  //digitalWrite(cs, LOW);
  SPI.transfer(4, 0x8E, SPI_CONTINUE);
  SPI.transfer(4, 0x60); //60= disable Osciallator and Battery SQ wave @1hz, temp compensation, Alarms disabled
  // digitalWrite(cs, HIGH);
  delay(10);
}
//////////////////////////////---READ DATE credits to SFE---/////////////////////////////////////
String ReadTimeDate() {

  String temp;

  for (int i = 0; i <= 6; i++) {
    if (i == 3)
      i++;
    //digitalWrite(cs, LOW);
    SPI.transfer(4, i + 0x00, SPI_CONTINUE);
    unsigned int n = SPI.transfer(4, 0x00);
    //digitalWrite(cs, HIGH);
    int a = n & B00001111;
    if (i == 2) {
      int b = (n & B00110000) >> 4; //24 hour mode
      if (b == B00000010)
        b = 20;
      else if (b == B00000001)
        b = 10;
      TimeDate[i] = a + b;
    }
    else if (i == 4) {
      int b = (n & B00110000) >> 4;
      TimeDate[i] = a + b * 10;
    }
    else if (i == 5) {
      int b = (n & B00010000) >> 4;
      TimeDate[i] = a + b * 10;
    }
    else if (i == 6) {
      int b = (n & B11110000) >> 4;
      TimeDate[i] = a + b * 10;
    }
    else {
      int b = (n & B01110000) >> 4;
      TimeDate[i] = a + b * 10;
    }
  }
  temp.concat(TimeDate[5]);   //put together the output string
  temp.concat(",") ;
  temp.concat(TimeDate[4]);
  temp.concat(",") ;
  temp.concat(TimeDate[6]);
  temp.concat(",") ;
  temp.concat(TimeDate[2]);
  temp.concat(",") ;
  temp.concat(TimeDate[1]);
  temp.concat(",") ;
  temp.concat(TimeDate[0]);
  return (temp);
}
//////////////////////////////-----SET TIME credits SFE----------///////////////////////////////////////////////////
int SetTimeDate(int d, int mo, int y, int h, int mi, int s) {
  int TimeDate [7] = {s, mi, h, 0, d, mo, y};
  for (int i = 0; i <= 6; i++) {
    if (i == 3)
      i++;
    int b = TimeDate[i] / 10;
    int a = TimeDate[i] - b * 10;
    if (i == 2) {
      if (b == 2)
        b = B00000010;
      else if (b == 1)
        b = B00000001;
    }
    TimeDate[i] = a + (b << 4);

    SPI.transfer(4, i + 0x80, SPI_CONTINUE);
    SPI.transfer(4, TimeDate[i]);
  }
}
//////////////////////////////-----TAKE TEMP credits Adafruit--------///////////////////////////////////////////

String TakeTemp(String dataOut) {
  String temp;
  float h1 = dht1.readHumidity();
  float h2 = dht2.readHumidity();

  // Read temperature as Celsius (the default)
  float t1 = dht1.readTemperature();
  float t2 = dht2.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f1 = dht1.readTemperature(true);
  float f2 = dht2.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h1) || isnan(t1) || isnan(f1)) {
    Serial.println("Failed to read from DHT sensor!");
    //return;
  }
  if (isnan(h2) || isnan(t2) || isnan(f2)) {
    Serial.println("Failed to read from DHT sensor!");
    //return;
  }
  // Compute heat index in Fahrenheit (the default)
  float hif1 = dht1.computeHeatIndex(f1, h1);
  float hif2 = dht2.computeHeatIndex(f2, h2);

  if (dataOut == "Decimal") {   //now if we want Decimal values, we have those ready
    TempInfo[0] = f1;
    TempInfo[1] = f2;
    TempInfo[2] = h1;
    TempInfo[3] = h2;
    TempInfo[4] = hif1;
    TempInfo[5] = hif2;
  }
  if (dataOut == "Rounded") {   //if we want rounded values, we need to create them
    TempInfo[0] = ((int)f1);
    TempInfo[1] = ((int)f2);
    TempInfo[2] = ((int)h1);
    TempInfo[3] = ((int)h2);
    TempInfo[4] = ((int)hif1);
    TempInfo[5] = ((int)hif2);
  }

  Serial.print("  Humidity 1: ");  //Serial print everything to be safe
  Serial.print(h1);
  Serial.print(" %\t");
  Serial.print("Temperature 1: ");
  Serial.print(f1);
  Serial.print(" *F ");
  Serial.print("Humidity2: ");
  Serial.print(h2);
  Serial.print(" %\t");
  Serial.print("Temperature 2: ");
  Serial.print(f2);
  Serial.println(" *F ");

  temp.concat(h1);
  temp.concat(",");
  temp.concat(h2);
  temp.concat(",");
  temp.concat(f1);
  temp.concat(",");
  temp.concat(f2);
  return (temp);
}

////////////////////==================Print Temp===========================
void printTemp(int x, int y, int s, String sensor, String dataType) {
  int sensorNum;
  if (sensor == "Inside") sensorNum = 0; //sensor number references so I can keep the straight rather than 0 and 1.
  if (sensor == "Outside") sensorNum = 1;

  String temp, unit, degree;
  unit = "F";
  degree = ((char)247); //something that looks like a degree symbol

  tft.setTextSize(s);
  TakeTemp(dataType); //get back data fitting the type we want

  if (TempInfo[sensorNum] != lastTempInfo[sensorNum]) {  //These work just like the time and date functions
    tft.setTextColor(ILI9341_BLACK);
    tft.setCursor(x, y);
    if (dataType == "Rounded") temp = ((int)lastTempInfo[sensorNum]) + degree + unit;
    if (dataType == "Decimal") temp = lastTempInfo[sensorNum] + degree + unit;
    tft.print(temp);
    lastTempInfo[sensorNum] = TempInfo[sensorNum];
  }

  if (lastTempInfo[sensorNum] < 20)   //This chunk sets the color of the readout based on the temperature, blue to red
    tft.setTextColor(ILI9341_CYAN);
  else if (lastTempInfo[sensorNum] < 30)
    tft.setTextColor(ILI9341_DARKCYAN);
  else if (lastTempInfo[sensorNum] < 40)
    tft.setTextColor(ILI9341_BLUE);
  else if (lastTempInfo[sensorNum] < 50)
    tft.setTextColor(ILI9341_LIGHTGREY);
  else if (lastTempInfo[sensorNum] < 60)
    tft.setTextColor(ILI9341_DARKGREY);
  else if (lastTempInfo[sensorNum] < 70)
    tft.setTextColor(ILI9341_DARKGREEN);
  else if (lastTempInfo[sensorNum] < 80)
    tft.setTextColor(ILI9341_YELLOW);
  else if (lastTempInfo[sensorNum] < 90)
    tft.setTextColor(ILI9341_ORANGE);
  else if (lastTempInfo[sensorNum] >= 90)
    tft.setTextColor(ILI9341_RED);
  tft.setCursor(x, y);
  if (dataType == "Rounded") temp = ((int)lastTempInfo[sensorNum]) + degree + unit;
  if (dataType == "Decimal") temp = lastTempInfo[sensorNum] + degree + unit;
  tft.print(temp);
}
//-----------------------------
void printHumidity(int x, int y, int s, String sensor, uint16_t color, String dataType) { //works just like temp, minus the changing colors

  String humidity, percent;
  int sensorNum;
  if (sensor == "Inside") sensorNum = 2;
  if (sensor == "Outside") sensorNum = 3;

  percent = ("%");
  tft.setTextSize(s);
  TakeTemp(dataType);

  if (TempInfo[sensorNum] != lastTempInfo[sensorNum]) {
    tft.setTextColor(ILI9341_BLACK);
    tft.setCursor(x, y);
    if (dataType == "Rounded") humidity = ((int)lastTempInfo[sensorNum]) + percent;
    if (dataType == "Decimal") humidity = lastTempInfo[sensorNum] + percent;
    tft.print(humidity);
    lastTempInfo[sensorNum] = TempInfo[sensorNum];
  }
  tft.setTextColor(color);
  tft.setCursor(x, y);
  if (dataType == "Rounded") humidity = ((int)lastTempInfo[sensorNum]) + percent;
  if (dataType == "Decimal") humidity = lastTempInfo[sensorNum]  + percent;
  tft.print(humidity);
}
//------------------------------------- credits to SFE----------------------
String TakePressure(String dataOut) {
  char status;
  double T, P, p0, a;

  String temp;
  // If you want sea-level-compensated pressure, as used in weather reports,
  // you will need to know the altitude at which your measurements are taken.
  // We're using a constant called ALTITUDE in this sketch:
  
#define ALTITUDE 786.08 // Altitude of the University of Idaho in meters

  Serial.println();
  Serial.print("provided altitude: ");
  Serial.print(ALTITUDE, 0);
  Serial.print(" meters, ");
  Serial.print(ALTITUDE * 3.28084, 0);
  Serial.println(" feet");

  // If you want to measure altitude, and not pressure, you will instead need
  // to provide a known baseline pressure. This is shown at the end of the sketch.
  // Start a pressure measurement:
  // The parameter is the oversampling setting, from 0 to 3 (highest res, longest wait).
  // If request is successful, the number of ms to wait is returned.
  // If request is unsuccessful, 0 is returned.

  status = pressure.startPressure(3);
  if (status != 0)
  {
    // Wait for the measurement to complete:
    delay(status);
    // Retrieve the completed pressure measurement:
    // Note that the measurement is stored in the variable P.
    // Note also that the function requires the previous temperature measurement (T).
    // (If temperature is stable, you can do one temperature measurement for a number of pressure measurements.)
    // Function returns 1 if successful, 0 if failure.

    status = pressure.getPressure(P, T);
    if (status != 0)
    {
      // Print out the measurement:
      Serial.print("absolute pressure: ");
      Serial.print(P, 2);
      Serial.print(" mb, ");
      Serial.print(P * 0.0295333727, 2);
      Serial.println(" inHg");

      // The pressure sensor returns abolute pressure, which varies with altitude.
      // To remove the effects of altitude, use the sealevel function and your current altitude.
      // This number is commonly used in weather reports.
      // Parameters: P = absolute pressure in mb, ALTITUDE = current altitude in m.
      // Result: p0 = sea-level compensated pressure in mb

      p0 = pressure.sealevel(P, ALTITUDE); // we're at 1655 meters (Boulder, CO)
      Serial.print("relative (sea-level) pressure: ");
      Serial.print(p0, 2);
      Serial.print(" mb, ");
      Serial.print(p0 * 0.0295333727, 2);
      Serial.println(" inHg");

      Pressure = (p0 * 0.0295333727);
      if (dataOut == "Rounded") Pressure = ((int)Pressure);
      temp = Pressure;
      return (temp);
    }
    else Serial.println("error retrieving pressure measurement\n");
  }
  else Serial.println("error starting pressure measurement\n");
}

//------------------------------------------
void printPressure(int x, int y, int s, String dataType, uint16_t color) { //Works just like the humidity function

  String PressureReadout, unit;
  unit = "inHg";
  tft.setTextSize(s);
  TakePressure(dataType);

  if (Pressure != lastPressure) {
    tft.setTextColor(ILI9341_BLACK);
    tft.setCursor(x, y);
    if (dataType == "Rounded") PressureReadout = ((int)lastPressure) + unit;
    if (dataType == "Decimal") PressureReadout = lastPressure + unit;
    tft.print(PressureReadout);
    lastPressure = Pressure;
  }

  tft.setTextColor(color);

  tft.setCursor(x, y);
  if (dataType == "Rounded") PressureReadout = ((int)lastPressure) + unit;
  if (dataType == "Decimal") PressureReadout = lastPressure + unit;
  tft.print(PressureReadout);
}


//-------------------SD data save function-----------
void gatherData() {

  String dataString = "", tempName = "", underscore = "_", day, month, year;

  dataString += ReadTimeDate();   //This is the string we are going to save as a CSV
  dataString += "," ;
  dataString += TakeTemp("Decimal");
  dataString += "," ;
  dataString += TakePressure("Decimal");

  tempName.concat(TimeDate[5]);   //This is the string of the file we need to write to
  tempName.concat(underscore);
  tempName.concat(TimeDate[4]);
  tempName.concat(underscore);
  tempName.concat(TimeDate[6]);
  tempName.concat(".csv");

  Serial.println(tempName); //make sure we are trying to open the right file

  File dataFile = SD.open(tempName.c_str(), FILE_WRITE);  //open the data file

  // if the file is available, write to it:
  if (dataFile) {
    digitalWrite(LEDpin, HIGH);
    dataFile.println(dataString); //if we write and save it, turn the LED on
    dataFile.close();
    dataPoints ++;                //if it worked, add a data point
  }
  else {
    Serial.println("error opening datalog");
    return;
  }
  File myFile;

  SD.remove("count.txt"); //get rid of the old data point file
  myFile = SD.open("count.txt", FILE_WRITE);
  // if the file opened okay, write to it:
  if (myFile) {
    Serial.print("Writing to count.txt...");

    myFile.print(dataPoints);   //Make a new data point file with the new number of data points
    // close the file:
    myFile.close();
    Serial.println("done.");
    digitalWrite(LEDpin, LOW);  //if the whole thing worked, turn the LED back off
  }
  else {
    // if the file didn't open, print an error:
    Serial.println("error opening count.txt");
  }

  Serial.println("\n data output"); Serial.println(dataString); Serial.println("\n");
}
