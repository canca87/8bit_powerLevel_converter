/* Power level converter
 *  
 *  (c) 2016. Adam Chrimes.
 *  
 *  Atmega328p (same as arduino uno). Using onboard RC at 8MHz.
 *  
 *  This simple code reads an 8-bit signal, adds 18% to the value
 *  and outputs the modified 8-bit signal. There is a Volt-meter 
 *  (display) attached to Timer2A output compare pin to visualise
 *  the 8-bit signals. There are two LED's (PB4 and PB5) to provide
 *  on/off feedback and warn the user of any errrors, which are also
 *  wired to two buttons.
 *  
 *  The two buttons allow the user to change the power multiplier.
 *  One button increments the multiplier by 0.1% and the other reduces
 *  by 0.1%. The value is stored in the EEPROM location 1 on each button
 *  press, so it should not be changed often (users should not be given
 *  access the buttons, PCB buttons are recommended).
 *  EEPROM loation 0 contains an initaliser. If the location 0 != 143 then
 *  the default value of 18% is recorded in location 1. The % value is 
 *  stored as % * 10: eg 18% = 180. The value is stored as a byte, therefore
 *  the maximum multiplier is 25.4% and the minimum is 0%.
 *  
 *  The watchdog timer is in play to make sure the device does not 
 *  lock-up. No IO buffers or input multiplexers are used. The native
 *  micro has enough pins to support the project.
 *  
 *  INPUT:-------------------------------
 *    LSB(0) (1)  (2)  (3)  (4)  (5)  (6)  (7)
 *     PC0   PC1  PC2  PC3  PC4  PC5  PB0  PB1
 *  OUTPUT:-------------------------------
 *    LSB(0) (1)  (2)  (3)  (4)  (5)  (6)  (7)
 *     PD0   PD1  PD2  PD3  PD4  PD5  PD6  PD7
 *  ALARM INPUTS:-------------------------
 *    PB6  PB7   Detail
 *     0    0     Temperature Alarm
 *     0    1     Normal - no alarm
 *     1    0     Reflection alarm
 *     1    1     Preamplifier alarm
 *  MISC INPUTS:------------------------
 *    PB2 - Laser on/off (high/low)
 *    PB4 - Dual purpose pin btnA active low
 *    PB5 - Dual purpose pin btnB active low
 *  MISC OUTPUTS:----------------------
 *    PB3 - PWM output to voltage display
 *    PB4 - LED A (active high)
 *    PB5 - LED B (active high)
 *  LED OUTPUT TABLE: ------------------
 *    PB2  PB6  PB7  LED info
 *     0    0    1    Mode 0 : ledA on, ledB off
 *     1    0    1    Mode 1 : ledA off, ledB on
 *     x    0    0    Mode 2 : ledA flashing, ledB off
 *     x    1    0    Mode 3 : ledA flashing alternate ledB
 *     x    1    1    Mode 4 : ledA off, ledB flashing
*/

//-------Includes ---------------
#include<EEPROM.h> //stores the current multiplier in the EEPROM

//-------Defines ---------------
// Using direct port access - not arduino pins #'s

//-------Variables ---------------
byte displayValue = 0; //to store the value that is displayed on the VoltMeter.
volatile uint16_t checkBTN = 0; //a flag used in the main loop to check the button states.
uint16_t changeLED = 0; //a flag used in the main loop to flash the LED's.
byte modVal = 180; //the current power multiplier (ie, 180 = 18%)
byte previousBTN = 3; //stores the previous state of the button presses (2=a,1=b,0=both,3=none)

//-------Objects ---------------
// no objects

//-------Setup ---------------
void setup() {
  init_ports(); //initialise the IO ports
  init_pwm(); //initalise the PWM output
  init_EEPROM(); //initalise the EEPROM
}

//-------Main ---------------
void loop() {
  byte tempValue = readInput(); //read the 8-bit input value
  displayValue = modifyInput(tempValue);
  writeOutput(displayValue); //write an 8-bit output value
  
  if (checkBTN > 1000){ //if the check button flag rolls over (happens at ~30Hz)
    cli(); //stop interrupts while we reset the flag
    checkBTN = 0; //reset the flag
    sei(); //start the interrupts again : this will cause a tiny jitter on the PWM, but it should be okay.
    changeLED++; //increment the changeLED flag;
    checkBTNstates(); //checks the states of the buttons
  }
  if(changeLED > 20){ //change the LED's (flash) and check any button presses.
    changeLED = 0;//reset the flag to zero
    changeLEDstates(getMode()); //gets the current mode and updates the LED's
  }
}


//--------PWM interrupt---------
ISR(TIMER2_OVF_vect){
  OCR2A = 255-displayValue; //set the PWM duty cycle (100% - value)
  checkBTN ++; //increment the check button flag (rollover = check buttons)
}

//-------Port INIT -----------
void init_ports(void){
  // Port B initialization
  // Function: 7=In 6=In 5=OUT 4=OUT 3=OUT 2=In 1=In 0=In 
  DDRB=(0<<DDB7) | (0<<DDB6) | (1<<DDB5) | (1<<DDB4) | (1<<DDB3) | (0<<DDB2) | (0<<DDB1) | (0<<DDB0);
  // State: 7=Tri 6=Tir 5=0 4=0 3=0 2=Tri 1=Tri 0=Tri 
  PORTB=(0<<PORTB7) | (0<<PORTB6) | (0<<PORTB5) | (0<<PORTB4) | (0<<PORTB3) | (0<<PORTB2) | (0<<PORTB1) | (0<<PORTB0);
  
  // Port C initialization
  // Function: 5=In 4=In 3=In 2=In 1=In 0=In 
  DDRC= (0<<DDC5) | (0<<DDC4) | (0<<DDC3) | (0<<DDC2) | (0<<DDC1) | (0<<DDC0);
  // State: 5=T 4=T 3=T 2=T 1=T 0=T 
  PORTC= (0<<PORTC5) | (0<<PORTC4) | (0<<PORTC3) | (0<<PORTC2) | (0<<PORTC1) | (0<<PORTC0);
  
  // Port D initialization
  // Function: 7=OUT 6=OUT 5=OUT 4=OUT 3=OUT 2=OUT 1=OUTn 0=OUT 
  DDRD=(1<<DDD7) | (1<<DDD6) | (1<<DDD5) | (1<<DDD4) | (1<<DDD3) | (1<<DDD2) | (1<<DDD1) | (1<<DDD0);
  // State: 7=0 6=0 5=0 4=0 3=0 2=0 1=0 0=0 
  PORTD=(0<<PORTD7) | (0<<PORTD6) | (0<<PORTD5) | (0<<PORTD4) | (0<<PORTD3) | (0<<PORTD2) | (0<<PORTD1) | (0<<PORTD0);
}

//-------PWM INIT--------------
void init_pwm(void){
  //Using timer 2 (8-bit) in FastPWM mode:
  cli(); //turn off interrupts while the timer is modified
  //TCCR2A register: set to intverting PWM mode - clear at bottom, set on compare match.
  TCCR2A = (1<<COM2A1) | (1<<COM2A0) | (0<<COM2B1) | (0<<COM2B0) | (1<<WGM21) | (1<<WGM20);
  //TCCR2B register: set prescaler to 1 (run at the fastest speed possible, ~32kHz)
  TCCR2B = (0<<FOC2A) | (0<<FOC2B) | (0<<WGM22) | (0<<CS22) | (0<<CS21) | (1<<CS20);
  //TIMSK2 register: set the timer overflow interrupt - we set TCNT every interrupt.
  TIMSK2 = (0<<OCIE2B) | (0<<OCIE2B) | (1<<TOIE2);
  //OCR2A = 255 (max) - displayValue;
  OCR2A = 255-displayValue;
  sei(); //turn on the interrupts
}

//-------EEPROM INIT--------------
void init_EEPROM(void){
  //Get the modVal from EEPROM
  if (EEPROM.read(1) != 143){ //if the flag 143 is not present, reset to the default 18%
    EEPROM.write(0,180); //write the 18% to the memory
    EEPROM.write(1,143); //set the default OK flag
  }
  modVal = EEPROM.read(0); //read the multiplier value from the EEPROM
}

//------ Read 8-bit input value ---------
byte readInput(void){
  /*  INPUT:-------------------------------
   *    MSB(7) (6)  (5)  (4)  (3)  (2)  (1)  (0)LSB
   *     PB1   PB0  PC5  PC4  PC3  PC2  PC1  PC0
   */
   byte value = (((PINB & 0x03) << 6) | (PINC & 0x3F));
   return value;
}

//-------Write 8-bit output value --------
void writeOutput(byte value){
  PORTD = value; //the output is directly to the portD.
}

//-------Get operating mode-------
byte getMode(void){
  /*
   *  PB2  PB6  PB7  LED info
   *   0    0    1    Mode 0 : ledA on, ledB off
   *   1    0    1    Mode 1 : ledA off, ledB on
   *   x    0    0    Mode 2 : ledA flashing, ledB off
   *   x    1    0    Mode 3 : ledA flashing alternate ledB
   *   x    1    1    Mode 4 : ledA off, ledB flashing
   */
   if (PINB & ((1<<PB6) | (1<<PB7))){
      return 4; //Mode 4 : ledA off, ledB flashing
   }
   else if (PINB & (1<<PB6)){
      return 3; //Mode 3 : ledA flashing alternate ledB
   }
   else if (PINB & (1<<PB7)){
      //can be either mode 1 or 0
      if(PINB & (1<<PB2)){
          return 1; //Mode 1 : ledA off, ledB on
      }
      else{
          return 0; //Mode 0 : ledA on, ledB off
      }
   }
   else{
    return 2; //Mode 2 : ledA flashing, ledB off
   }
}

//--------Change the LED's---------------
void changeLEDstates(byte mode){
  /* LED info
   * Mode 0 : ledA on, ledB off
   * Mode 1 : ledA off, ledB on
   * Mode 2 : ledA flashing, ledB off
   * Mode 3 : ledA flashing alternate ledB
   * Mode 4 : ledA off, ledB flashing
    */
  //switch case here to manage the states
  switch (mode){
    case 0:
      //Mode 0 : ledA on, ledB off
      PORTB |= (1<<PORTB4); //set ledA
      PORTB &= ~(1<<PORTB5); //clear ledB
      break;
    case 1:
      //Mode 1 : ledA off, ledB on
      PORTB &= ~(1<<PORTB4); //clear ledA
      PORTB |= (1<<PORTB5); //set ledB
      break;
    case 2:
      //Mode 2 : ledA flashing, ledB off
      PORTB ^= (1<<PORTB4); //toggle ledA
      PORTB &= ~(1<<PORTB5); //clear ledB
      break;
    case 3:
      //Mode 3 : ledA flashing alternate ledB
      if ((PINB>>PB4) & 1){ //if ledA on: turn it off (and ledB on)
        PORTB &= ~(1<<PORTB4); //clear ledA
        PORTB |= (1<<PORTB5); //set ledB
      }
      else{//else if ledA off: turn it on (and ledB off)
        PORTB |= (1<<PORTB4); //set ledA
        PORTB &= ~(1<<PORTB5); //clear ledB
      }
      break;
    case 4:
      //Mode 4 : ledA off, ledB flashing
      PORTB &= ~(1<<PORTB4); //toggle ledA
      PORTB ^= (1<<PORTB5); //clear ledB
      break;
    default:
      //Unknown mode: both off.
      PORTB &= ~(1<<PORTB4); //clear ledA
      PORTB &= ~(1<<PORTB5); //clear ledB
      break;
  }
}

//--------Check the button states--------------
void checkBTNstates(void){
  //make outputs as inputs
  //read the current LED state first
  byte LEDstate = PORTB & 0x30; //save only PB4 and PB5 values
  DDRB &= ~((1<<DDB5) | (1<<DDB4)); //change DDRD to zero for input
  PORTB &= ~((1<<PORTB5) | (1<<PORTB4));//make sure they are tri-state (no pullups)
  //read in the value
  byte val = ((PINB & (1<<PB5)) << 1) | (PINB & (1<<PB4)); //normally 3; 2 = PB4 low; 1 = PB5 low.
  //make input back to outputs
  DDRB |= ((1<<DDB5) | (1<<DDB4)); //change DDRD to one for output
  PORTB |= (LEDstate & 0x30);//put the LED's back to their previous values
  //work out if any buttons were pressed
  if (val != previousBTN){ //YES - so determine which one(s)
    previousBTN = val; //store the current button press (button lock-out)
    //adjust the values as necessary
    if (val == 2){ 
      modVal++; //increment the power multiplier by 0.1%
      EEPROM.write(0,modVal); //save it to EEPROM
    }
    else if (val == 1){
      modVal--; //decrement the power multiplier by 0.1%
      EEPROM.write(0,modVal); //save it to the EEPROM
    }
  }
}

//--------- Add x% to the input value ------------------
byte modifyInput(byte val){
  // result = input * 1.18 (ie add 18%) : this is done with float variables for precision (but its slow).
  float result = (((float)val * ((float)(modVal+ 1000) / 1000.0)));
  // if the result is too big for a byte, limit it to the max byte value.
  if (result > 255){
    result = 255; //max byte value (result was bigger than a byte).
  }
  return (byte)result; //return the result as a byte
}

