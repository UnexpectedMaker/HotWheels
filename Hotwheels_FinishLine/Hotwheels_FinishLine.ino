#define MASTER

// MASTER is the side that has the display on it
#ifdef MASTER
 
  // define pins attached to display
  #define MAX7219_DIN           6
  #define MAX7219_CS            5
  #define MAX7219_CLK           4
  
  
  // enumerate the MAX7219 registers
  // See MAX7219 Datasheet, Table 2, page 7
  enum {  MAX7219_REG_DECODE    = 0x09,  
          MAX7219_REG_INTENSITY = 0x0A,
          MAX7219_REG_SCANLIMIT = 0x0B,
          MAX7219_REG_SHUTDOWN  = 0x0C,
          MAX7219_REG_DISPTEST  = 0x0F };
  
  // enumerate the SHUTDOWN modes
  // See MAX7219 Datasheet, Table 3, page 7
  enum  { OFF = 0,  
          ON  = 1 };

  // Some constants used for the display
  const byte RDY = 42;
  const byte NONE = 0b01111111;
  const byte DP = 0b10000000;  

#else // SLAVE is the side that has the button on it

  #define SLAVE

  // Using the OneButton library for button press support
  #include "OneButton.h"

  // Using pin 9 for button
  #define BUTTON_G              9
  // Create an OneButton reference with default of LOW
  OneButton buttonG(BUTTON_G, false);

  // Using analog pin 0 for LED
  #define STATUS_LED            A0

#endif

// define pins for IR sensor input - sam pins on both sides

#define IR_0                  2
#define IR_1                  3


// 0 = READY, 1 = RUNNING, 2 = FINISHED
int currentState = 0;

// just a bunch of variables
unsigned long timer0 = 0;
unsigned long timer1 = 0;
unsigned long mainTimer = 0;
unsigned long updateTimer = 0;
unsigned long flashTimer = 0;

bool timer0running = false;
bool timer1running = false;
bool flashWinner = true;
bool showReady = true;

int winner = -1;
int counter = 0;


String data = "";


// ... setup code here, to run once
void setup()  
{
    // initialize the serial port - bluetooth uses 38400 baud
    Serial.begin(38400);           // initialize serial communication

 #ifdef MASTER
 
    Serial.println("MASTER");

    // define type of pins
    pinMode(MAX7219_DIN, OUTPUT);   // serial data-in
    pinMode(MAX7219_CS, OUTPUT);    // chip-select, active low    
    pinMode(MAX7219_CLK, OUTPUT);   // serial clock
    digitalWrite(MAX7219_CS, HIGH);

    // reset the MAX2719 display
    resetDisplay();
    // Display the ready message 
    displayReady();
    
 #else
 
    Serial.println("SLAVE");

    // Attach the button method to the button
    buttonG.attachClick(singleClick_G);

     // define type of pins
    pinMode( BUTTON_G, INPUT );
    pinMode( STATUS_LED, OUTPUT );
    
 #endif

     // define type of pins for IR on both boards
    pinMode( IR_0, INPUT );
    pinMode( IR_1, INPUT );
}

// SLAVE handles button pressing and car timer starts and sends the state via serial to the MASTER
void PushStateFromSlave( int state )
{
  // if our current state is the new state, skip it
  if ( currentState == state )
    return;

  currentState = state;

  if ( currentState == 0 )
  {
    timer0running = false;
    timer1running = false;
  }

  #ifdef SLAVE
  // set the LED state
  analogWrite( STATUS_LED, ( currentState == 1 ? 255 : 0 ) );
  #endif

  // Sending the state via serial
  Serial.write( state );
}

// Master sets it's own state based on what it recieves from the sale BT
void SetMasterState( int state )
{
  // if our current state is the new state, skip it
  if ( currentState == state )
    return;

    currentState = state;

  if ( currentState == 0 ) // ready
  {
    timer0 = 0;
    timer1 = 0;
    timer0running = false;
    timer1running = false;
    winner = -1;
    flashWinner = false;
    showReady = true;
  }

  // Print the state in the output log
  Serial.print("State: ");
  Serial.println( currentState );

}

void SetWinner( int w )
{
  // Print the winner to the serial output log
  Serial.print("Set Winner: ");
  Serial.println( w );

  // if the current winner is not set, set it
  if ( winner == -1 )
  {
    winner = w;
  }

  // stop the winners timer immediatly
  if ( w == 0 )
    timer0running = false;
   else if ( w == 1 )
    timer1running = false;
}

void loop()  
{
#ifdef SLAVE

    // required for button press check
    buttonG.tick();

    // we are in race mode... so start timers on each track if the respective car hits the IR sensor
    if ( currentState == 1 )
    {
      if ( !timer0running )
      {
        if ( !digitalRead(IR_0) )
        {
          Serial.write("A"); // Sending character A - 65 on recieving end
          timer0running = true;
        }
      }

      if ( !timer1running )
      {
        if ( !digitalRead(IR_1) )
        {
        
          Serial.write("B"); // Sending character B - 66 on recieving end
          timer1running = true;
        }
      }
    }
    
  #else

  // I AM THE MASTER
  // Check to see if any data has come in via BT
  if ( Serial.available() > 0 )
  {
    data = Serial.read();
    int state = data.toInt();

    if ( state == 65 ) // 65 == A ... this means car 1 has hit teh start IR sensor, so start the timer
    {
      timer0 = 0;
      timer0running = true;
      Serial.println("Start Car 1");
    }
    else if ( state == 66 )  // 66 == B ... this means car 1 has hit teh start IR sensor, so start the timer
    {
      timer1 = 0;
      timer1running = true;
      Serial.println("Start  Car 2");
    }
    else
    {    
      // display some serial debub info on what states I got
      if ( state == 0 )
        Serial.print("Reset System to RDY state!");
      else if ( state == 1 )
        Serial.print("Activate Race Mode!");
      else
      {
        Serial.print(" Goty unknown state: ");
        Serial.println( state );
      }

      // Send the current state to the master
      SetMasterState( state );
    }
  }

  // we are in ready/reset state - show dashes on display
  if ( currentState == 0 )
  {
    if ( millis() - updateTimer > 100 )
    {
      displayReady();
      updateTimer = millis();
    }
    return;
  }

  // we are not in ready/reset state so we must be starting or running a race
  unsigned long timeStep = (millis() - mainTimer);

  // if timer 0 (car 1 ) is running, increment it's time
  if ( timer0running )
  {
    // if the finish IR sensor is hit, stop the timer
    if ( !digitalRead(IR_0) )
      timer0running = false;
    
    timer0 += timeStep;
  }

  // if timer 1 (car 2 ) is running, increment it's time
  if ( timer1running )
  {
     // if the finish IR sensor is hit, stop the timer
    if ( !digitalRead(IR_1) )
      timer1running = false;
    
    timer1 += timeStep;
  }

  // If both timers are satopped but no winner is set, work out the winner
  if ( !timer0running && !timer1running && winner == -1 )
  {
    if ( timer0 < timer1 ) 
      SetWinner( 0 );
     else if ( timer0 > timer1 ) 
      SetWinner( 1 );

      
  }
  
  mainTimer = millis();
  
  if ( millis() - updateTimer > 50 )
  {
    // we want teh times in seconds with 2 decimal places
    String string0 =  String(((float)timer0 / 1000), 2);
    String string1 =  String(((float)timer1 / 1000), 2);
  
    if ( string0.length() == 4 )
      string0 = String(" " + string0);
  
    if ( string1.length() == 4 )
      string1 = String(" " + string1);

    // every 500 miliseconds we flash the winner time
    if ( millis() - flashTimer > 500 )
    {
      flashTimer = millis();
      
      if ( winner == 0 )
      {
          flashWinner = !flashWinner;
  
          if ( !flashWinner )
            string0 = "     ";
      }
      else if ( winner == 1 )
      {
          flashWinner = !flashWinner;
  
          if ( !flashWinner )
            string1 = "     ";
      }
    }

    // concatinate the sides of the display
    String str = String( string0 + string1 );
  
    displayTimes(str);
  
    updateTimer = millis();
  }

  #endif
}

void singleClick_G()
{
  if ( currentState == 0 )
  {
    PushStateFromSlave( 1 );
  }
  else
  {
    PushStateFromSlave( 0 );
  }
}


#ifdef MASTER
/***************************************/
/************ MAX7219 Stuff ************/
/***************************************/

// ... write a value into a max7219 register 
// See MAX7219 Datasheet, Table 1, page 6
void set_register(byte reg, byte value)  
{
    digitalWrite(MAX7219_CS, LOW);
    shiftOut(MAX7219_DIN, MAX7219_CLK, MSBFIRST, reg);
    shiftOut(MAX7219_DIN, MAX7219_CLK, MSBFIRST, value);
    digitalWrite(MAX7219_CS, HIGH);
}


// ... reset the max7219 chip
void resetDisplay()  
{
    set_register(MAX7219_REG_SHUTDOWN, OFF);   // turn off display
    set_register(MAX7219_REG_DISPTEST, OFF);   // turn off test mode
    set_register(MAX7219_REG_INTENSITY, 0x0D); // display intensity
}

void displayReady()
{
    set_register(MAX7219_REG_SHUTDOWN, OFF);  // turn off display
    set_register(MAX7219_REG_SCANLIMIT, 7);   // scan limit 8 digits
    set_register(MAX7219_REG_DECODE, 0b11111111); // decode all digits

    for ( int i = 1; i < 9; i++ )
    {
      set_register(i, RDY );
    }

    set_register(MAX7219_REG_SHUTDOWN, ON);   // Turn on display
}

void displayTimes(String timeString)  
{
    set_register(MAX7219_REG_SHUTDOWN, OFF);  // turn off display
    set_register(MAX7219_REG_SCANLIMIT, 7);   // scan limit 8 digits
    set_register(MAX7219_REG_DECODE, 0b11111111); // decode all digits

    if ( timeString.charAt(9) == ' ' )
      set_register(1, NONE );
    else
      set_register(1, timeString.charAt(9));

    if ( timeString.charAt(8) == ' ' )
      set_register(2, NONE );
    else
      set_register(2, timeString.charAt(8));

    if ( timeString.charAt(6) == ' ' )
      set_register(3, NONE );
    else
      set_register(3, timeString.charAt(6) | DP);

    if ( timeString.charAt(5) == ' ' )
      set_register(4, NONE );
    else
      set_register(4, timeString.charAt(5) );
      
    if ( timeString.charAt(4) == ' ' )
      set_register(5, NONE );
    else
      set_register(5, timeString.charAt(4));

    if ( timeString.charAt(3) == ' ' )
      set_register(6, NONE );
    else
      set_register(6, timeString.charAt(3));

    if ( timeString.charAt(1) == ' ' )
      set_register(7, NONE );
    else
      set_register(7, timeString.charAt(1) | DP);

    if ( timeString.charAt(0) == ' ' )
      set_register(8, NONE );
    else
      set_register(8, timeString.charAt(0) );

    set_register(MAX7219_REG_SHUTDOWN, ON);   // Turn on display
}

#endif

