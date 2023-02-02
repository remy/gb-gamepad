#include <SPI.h>
#include "Gamepad.h" // from https://github.com/MiSTer-devel/Retro-Controllers-USB-MiSTer/

#define UP 0x04
#define DOWN 0x08
#define LEFT 0x02
#define RIGHT 0x01
#define SELECT 0x40
#define START 0x80
#define A 0x10
#define B 0x20

// ATT: 20 chars max (including NULL at the end) according to Arduino source code.
// Additionally serial number is used to differentiate arduino projects to have different button maps!
const char *gp_serial = "GB DMG to USB";

volatile boolean received;
volatile byte input;

uint8_t lastInput = 0;
uint8_t bitSelect = 0;
uint8_t bitStart = 0;

int SCK_PIN = 15;

// Set up USB HID gamepads
Gamepad_ Gamepad;

ISR (SPI_STC_vect)        //Interput routine function 
{
  input = SPDR;   // Get the received data from SPDR register
  received = true;       // Sets received as True 
}

void setup()
{
  Gamepad.reset();

  pinMode(9, OUTPUT);
  digitalWrite(9, LOW); // hard wire the CS (going to the IC on pin 8)

  pinMode(LED_BUILTIN_TX,INPUT);

  received = false;

  // Because the Gameboy is running in, what is effectively Controller mode
  // and it doesn't do a Chip Select (we do that manually on the Arduino)
  // then this code will read the clock signal until it's clocked in 8 ticks
  // then unlatch and run as if it's a proper SPI signal.
  //
  // The way I've done this is naive, but seems to work - I read the pulse
  // length on the LOW, for it to be 8Mhz it's actually around 120us but
  // the gap between bytes is between ~850us and ~1200us, so 200um is fine
  // if the time is more, then we figure we're between bytes and reset the
  // counter.
  //
  // Once 8 ticks in a row have been counted, I unlatch and carry on.

  bool latched = true;
  uint8_t bits = 0;
  uint8_t length = 0;

  while (latched) {
    length = pulseIn(SCK_PIN, LOW);    
    if (length == 0) {
      bits = 0;
    } else if (length < 200) { // approx a pulse length
      bits++;
    } else {
      bits = 0;
    }

    if (bits == 8) {
      latched = false;
    }
  }
  
  // The gameboy clock and messaging looks like this:
  // https://remysharp.com/shot/SCR-20230129-hq8-min.png

  // turn on SPI in peripheral mode
  SPCR |= _BV(SPE);

  // turn on interrupts
  SPCR |= _BV(SPIE);

  // CPOL = 1 - the gameboy is "like" SPI but the clock is HIGH in the idle state
  SPCR |= _BV(CPOL);

  SPDR = 0; //initialise SPI register
}


void loop()
{ 
  if (received) {                        
    received = false;

    if (input != lastInput) {
      lastInput = input;

      Gamepad._GamepadReport.hat = dpad2hat(input);

      // I need to swap around start and select bit from Gameboy byte to be compatible
      // with the gamepad byte.
      bitStart = (input >> 7) & 1;
      bitSelect = (input >> 6) & 1;
      input ^= (bitStart ^ bitSelect) << 7 | (bitStart ^ bitSelect) << 6;

      Gamepad._GamepadReport.buttons = (input >> 4); // first 4 bytes are the dpad
      
      Gamepad.send();
    }
  }
}

uint8_t dpad2hat(uint8_t dpad)
{
  switch(dpad & (UP|DOWN|LEFT|RIGHT))
  {
    case UP:         return 0;
    case UP|RIGHT:   return 1;
    case RIGHT:      return 2;
    case DOWN|RIGHT: return 3;
    case DOWN:       return 4;
    case DOWN|LEFT:  return 5;
    case LEFT:       return 6;
    case UP|LEFT:    return 7;
  }
  return 15;
}