
// set Arduino to Adafruit Trinket (ATtiny85 8MHz)

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

// this is the probe's low side
#define PROBE_PIN 0

#define ESP_POWER_PIN 2

// we use the LED pin (1) to turn signal whether or not there is a water-level event or low battery check
#define MESSAGE_PIN 1

#define k_sleep_interval_secs 8

// define this to wait far less for the timeouts and battery checks
//#define DEBUG_INTERVALS

#ifdef DEBUG_INTERVALS
#define k_notification_timeout_secs   16
#define k_battery_check_interval_secs 32
#else
#define k_notification_timeout_secs    3600 // notify every hour until water level goes low
#define k_battery_check_interval_secs 86400 // check battery every 24 hours
#endif

#define k_notification_timeout      (k_notification_timeout_secs / k_sleep_interval_secs)
#define k_battery_check_interval    (k_battery_check_interval_secs / k_sleep_interval_secs)
#define k_wait_for_esp32_timeout    3000 // this is a loop counter, each loop delays k_loop_delay_ms
#define k_loop_delay_ms             10

static uint64_t s_watchdog_count = 0;
static uint64_t s_last_msg_sent  = 0;
static uint64_t s_last_wakeup    = 0;

// WDT Interrupt needs some code for coming out of sleep-- it just needs to exist...
ISR( WDT_vect ) 
{
}


// Enters the arduino into sleep mode.
void enterSleep()
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode(); // Start sleep mode

  // Hang here until WDT timeout

  sleep_disable();
  power_all_enable();
}


// Setup the Watch Dog Timer (WDT)
void setupWDT() 
{
   MCUSR &= ~(1<<WDRF); // Clear the WDRF (Reset Flag).

   // Setting WDCE allows updates for 4 clock cycles end is needed to change WDE or the watchdog pre-scalers.
   WDTCR |= (1<<WDCE) | (1<<WDE);

   // 8 seconds timeout (k_sleep_interval_secs)
   WDTCR  = (1<<WDP3) | (0<<WDP2) | (0<<WDP1) | (1<<WDP0);

   WDTCR |= _BV(WDIE); // Enable the WDT interrupt.
}


void setup() 
{
  // default to input pullup so we don't use any power from them when sleeping...
  pinMode(1, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);

  // possibly redo a few pins
  pinMode( ESP_POWER_PIN, OUTPUT ); 
  pinMode( PROBE_PIN, INPUT ); 
  pinMode( MESSAGE_PIN, OUTPUT );

  digitalWrite( ESP_POWER_PIN, LOW ); // start with it off 
  digitalWrite( MESSAGE_PIN, LOW ); // start with it off 
  
  ADCSRA &= ~(1<<ADEN); // Disable ADC
  setupWDT();
}


void delayLoop( int del ) 
{ 
  delay( del );
}


// !!@ not sure what this entrypoint is for
void _loop() 
{
  // Re-enter sleep mode.
  enterSleep();
}


void blink_led( int pin, int duration )
{
   digitalWrite( pin, HIGH );
   delayLoop( duration );
   digitalWrite( pin, LOW );
   delayLoop( duration );
}


void wait_for_esp32_probe_pin_low()
{
  int time_out = 0;
  
   while( digitalRead( PROBE_PIN ) && (time_out++ < k_wait_for_esp32_timeout) )
      delayLoop( k_loop_delay_ms );  
}


void wait_for_esp32_message_pin_high()
{
  int time_out = 0;
  
   while( !digitalRead( MESSAGE_PIN ) && (time_out++ < k_wait_for_esp32_timeout) )
      delayLoop( k_loop_delay_ms );  
}


void loop() 
{
  ++s_watchdog_count; // multiply by 8 to get seconds (as we sleep for 8 seconds)

  auto water_sensor_reading = digitalRead( PROBE_PIN );

  if( water_sensor_reading && !s_last_msg_sent )
  {
    // set ESP32 will look at the probe line-- high to indicate that this is a water high message (low battery check)
    pinMode( MESSAGE_PIN, OUTPUT );
    digitalWrite( MESSAGE_PIN, LOW );
    pinMode( MESSAGE_PIN, INPUT ); // leave as input

    // turn on power to the ESP32
    digitalWrite( ESP_POWER_PIN, HIGH );
 
    delayLoop( 500 );

    // wait for the ESP32 to send a txt message, the ESP32 will bring the message pin high to let us know it's done...
    wait_for_esp32_message_pin_high();

    // turn off ESP32 before sleeping
    digitalWrite( ESP_POWER_PIN, LOW );
   
    s_last_msg_sent = s_watchdog_count;
  }


  if( s_watchdog_count > s_last_msg_sent + k_notification_timeout )
  {
    // reset to zero so we start listening to the probe again
    s_last_msg_sent = 0;
  }


  if( s_watchdog_count > s_last_wakeup + k_battery_check_interval )
  {
    // turn on ESP32 and tell it to check battery
    pinMode( MESSAGE_PIN, OUTPUT );
    digitalWrite( MESSAGE_PIN, LOW );
    pinMode( MESSAGE_PIN, INPUT ); // leave as input

    // turn on power to the ESP32
    digitalWrite( ESP_POWER_PIN, HIGH );
 
    delayLoop( 500 );

    // wait for the ESP32 to send a txt message, the ESP32 will bring the message pin high to let us know it's done...
    wait_for_esp32_message_pin_high();

    // turn off ESP32 before sleeping
    digitalWrite( ESP_POWER_PIN, LOW );
    
    // update wakeup
    s_last_wakeup = s_watchdog_count;
  }

   
    // turn off ESP32 before sleeping
    digitalWrite( ESP_POWER_PIN, LOW );
//    digitalWrite( MESSAGE_PIN, LOW );

//  pinMode( ESP_POWER_PIN, INPUT_PULLUP ); 
//  pinMode( PROBE_PIN, INPUT_PULLUP); 
//  pinMode( MESSAGE_PIN, INPUT_PULLUP);


  enterSleep();

//  pinMode( PROBE_PIN, INPUT ); 
//  pinMode( ESP_POWER_PIN, OUTPUT ); 
//  pinMode( MESSAGE_PIN, OUTPUT );
}


// EOF
