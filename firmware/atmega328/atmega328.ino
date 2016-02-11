#include "WS2812.h"	// thanks Matthias Riegler!
#include "PINS.h"
#include <Wire.h>

#define START_BYTE		0xCE
#define CMD_SET_SIMPLE 		0xF0
#define CMD_CONFIG		0xF1
#define CMD_GET			0xF2
#define CMD_RESET		0xF3
#define CMD_DIMM		0xF4

#define MODE_PWM			0x01
#define MODE_ANALOG_INPUT		0x02
#define MODE_SINGLE_COLOR_WS2812	0x03
#define MODE_MULTI_COLOR_WS2812		0x04
#define MODE_DIGITAL_INPUT		0x05
#define MODE_DIGITAL_OUTPUT		0x06
#define MAX_MODE			MODE_DIGITAL_OUTPUT
// non user mode
#define MODE_PWM_DIMMING 	0xF1


#define TRANSFER_TIMEOUT 	50
#define I2C_ADDRESS		0x04
#define MAX_CHANNEL_COUNT 	17

//#define DEBUG			1		// if you enabled this: remember to slow your request down! serial.print is sloooow
#define MAX_RESPONSE_LENGTH 	4

uint8_t intens[100] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,27,28,29,30,31,32,33,34,35,36,38,39,40,41,43,44,45,47,48,50,51,53,55,57,58,60,62,64,66,68,70,73,75,77,80,82,85,88,91,93,96,99,103,106,109,113,116,120,124,128,132,136,140,145,150,154,159,164,170,175,181,186,192,198,205,211,218,225,232,239,247,255};


uint8_t		m_channel=0;
uint8_t 	m_receive_buffer[32];
uint8_t 	m_receive_length=0;
uint8_t 	m_response_buffer[MAX_RESPONSE_LENGTH];
uint8_t 	m_response_length=0;
cRGB 		value;

PIN m_pins[MAX_CHANNEL_COUNT];


//========================= i2c config and init? =========================//
void setup(){
	// Activate Pin PB6 and PB7 input pull up and read i2c address adder value
	DDRB &= ~((1 << 6) & (1 << 7));
	PORTB |= (1 << 6) | (1 << 7); 
	delay(10); // 10ms to pull the pin up
	uint8_t i2c_adder = ((PINB ^ 0xff)>>6);

	// prepare
	//set(bool analog_in, bool digital, bool pwm, bool ws2812, uint8_t arduino_pin);
	m_pins[0].set(false, true, true, true, 10);
	m_pins[1].set(false, true, true, true, 9);
	m_pins[2].set(false, true, true, true, 6);
	m_pins[3].set(false, true, true, true, 5);
	m_pins[4].set(true, true, true, false, 15);
	m_pins[5].set(false, false, false, false, 0xff);
	m_pins[6].set(false, true, false, true, 2);
	m_pins[7].set(false, true, false, true, 11);
	m_pins[8].set(true, true, true, false, 16);
	m_pins[9].set(false, true, false, true, 3);
	m_pins[10].set(true, true, true, false, 17);
	m_pins[11].set(false, true, false, true, 8);
	m_pins[12].set(false, true, false, true, 7);

	m_pins[13].set(false, true, true, false, 12); // internal pin to motor driver
	m_pins[14].set(false, true, true, false, 13); // internal pin to motor driver
	m_pins[15].set(false, true, true, false, 4);  // internal pin to CHIP
	m_pins[16].set(false, true, true, false, 14); // internal pin to LED

	
	Wire.begin(I2C_ADDRESS+i2c_adder);                // join i2c bus with address #4,5,6 or 7
	Wire.onReceive(receiveEvent); // register event
	Wire.onRequest(requestEvent); // register event
	
	#ifdef DEBUG
	Serial.begin(9600);
	Serial.print("woop woop online at ID ");
	Serial.println(I2C_ADDRESS+i2c_adder);
	#endif
}
//========================= i2c config and init? =========================//
//========================= check for incoming  timeouts =========================//
void loop(){
	for(uint16_t i=0; i<MAX_CHANNEL_COUNT; i++){
		if(m_pins[i].m_mode == MODE_PWM_DIMMING){
			if(m_pins[i].m_next_action <= millis()){
				if(m_pins[i].m_value < m_pins[i].m_target_value){
					m_pins[i].m_value++;
				} else if(m_pins[i].m_value > m_pins[i].m_target_value){
					m_pins[i].m_value--;
				}
				
				// write value to pin
				analogWrite(m_pins[i].m_arduino_pin, intens[m_pins[i].m_value]);
				
				if(m_pins[i].m_value == m_pins[i].m_target_value){
					m_pins[i].m_mode = MODE_PWM;
				} else {
					m_pins[i].m_next_action += m_pins[i].m_dimm_interval;
				};
			};	
		};
	};

	if(m_receive_length){
		parse();
		m_receive_length=0;
	};
}
//========================= check for incoming  timeouts =========================//
//========================= declare reset function @ address 0 =========================//
void(* resetFunc) (void) = 0; 
//========================= declare reset function @ address 0 =========================//
//========================= setup - on-the-fly =========================//
void config_pin(){
	m_response_buffer[0] = 0xff; // assume NACK
	m_response_length = 1;

	// PWM pins for dimming
	if(m_pins[m_channel].m_mode==MODE_PWM){
		if(m_pins[m_channel].is_pwm_out()){	
			m_response_buffer[0]=0; 
			
			#ifdef DEBUG
			Serial.print("PWM at pin ");
			Serial.print(m_channel);
			Serial.print(" that is ");
			Serial.println(m_pins[m_channel].m_arduino_pin,DEC);
			#endif

			pinMode(m_pins[m_channel].m_arduino_pin,OUTPUT);
		}
	} 

	// analog input 
	else if(m_pins[m_channel].m_mode==MODE_ANALOG_INPUT){
		if(m_pins[m_channel].is_analog_in()){
			m_response_buffer[0]=0; 
			
			#ifdef DEBUG
			Serial.print("analog in at pin ");
			Serial.print(m_channel);
			Serial.print(" that is ");
			Serial.println(m_pins[m_channel].m_arduino_pin,DEC);
			#endif
			
			pinMode(m_pins[m_channel].m_arduino_pin,INPUT);
		};
	} 

	// digital input 
	else if(m_pins[m_channel].m_mode==MODE_DIGITAL_INPUT){
		if(m_pins[m_channel].is_digital_inout()){	
			m_response_buffer[0]=0;
			pinMode(m_pins[m_channel].m_arduino_pin,INPUT);
		};
	} 

	// digital output
	else if(m_pins[m_channel].m_mode==MODE_DIGITAL_OUTPUT){
		if(m_pins[m_channel].is_digital_inout()){
			m_response_buffer[0]=0;
			pinMode(m_pins[m_channel].m_arduino_pin,OUTPUT);

			#ifdef DEBUG
			Serial.print("digital output at pin ");
			Serial.println(m_pins[m_channel].m_arduino_pin,DEC);
			#endif		

		};
	} 

	// WS2812 LEDs
	else if(m_pins[m_channel].m_mode==MODE_SINGLE_COLOR_WS2812 || m_pins[m_channel].m_mode==MODE_MULTI_COLOR_WS2812){
		if(m_pins[m_channel].is_ws2812()){
			m_response_buffer[0]=0;
			
			#ifdef DEBUG
			Serial.print("ws2812 at pin ");
			Serial.println(m_pins[m_channel].m_arduino_pin,DEC);
			#endif
			
			WS2812 *ws2812 = new WS2812(m_pins[m_channel].m_ws2812_count);
			ws2812->setOutput(m_pins[m_channel].m_arduino_pin); 
			m_pins[m_channel].m_ws2812_pointer=ws2812; // save pointer
		};
	}
	
	// un-configure pin-mode if the mode was rejected
	if(m_response_buffer[0]!=0){
		m_pins[m_channel].m_mode = 0xff;
	}
}
//========================= setup - on-the-fly =========================//
//========================= i2c input statemachine =========================//
// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int msgLength){
	
	
	#ifdef DEBUG
	Serial.print(msgLength);
	Serial.println(" byte in");
	#endif

	// copy all bytes to have them available at once
	for(uint8_t i=0; Wire.available(); i++){ // loop through all
		m_receive_buffer[i]=Wire.read();
		
		#ifdef DEBUG
		Serial.print(m_receive_buffer[i],HEX);
		Serial.print(",");
		#endif
	}

	#ifdef DEBUG
	Serial.println("");
	#endif
	m_receive_length = msgLength;
}
//========================= i2c input statemachine =========================//

//================================ parse,  =================================//
// receiveEvent is callen in interrupt
// if we block to long, the i2c interface will be busy
// therefore copy it to a new buffer and process it in the userspace
// this will free the i2c buffer to receive a new message "in parallel"

void parse(){
	// only read it if there is a magic startbyte
	if(m_receive_buffer[0] == START_BYTE){
		/////////////////////////////  RESET CONFIGURATION /////////////////////////////
		if(m_receive_buffer[1] == CMD_RESET){
			resetFunc();  
		} 
		/////////////////////////////  RESET CONFIGURATION /////////////////////////////
		////////////////////////////  SIMPLE SETTING A VALUE ///////////////////////////
		else if(m_receive_buffer[1] == CMD_SET_SIMPLE && m_receive_length>=4){
			m_channel = m_receive_buffer[2];

			//## digital write for output or pull ups or PWM ##
			if(m_pins[m_channel].m_mode==MODE_DIGITAL_OUTPUT || m_pins[m_channel].m_mode==MODE_DIGITAL_INPUT || m_pins[m_channel].m_mode==MODE_ANALOG_INPUT || m_pins[m_channel].m_mode==MODE_PWM || m_pins[m_channel].m_mode==MODE_PWM_DIMMING){
				m_pins[m_channel].m_value = m_receive_buffer[3];

				#ifdef DEBUG
				Serial.print("digitalWrite ");
				Serial.print(m_pins[m_channel].m_value);
				Serial.print(" at pin ");
				Serial.println(m_pins[m_channel].m_arduino_pin,DEC);
				#endif		

				
				digitalWrite(m_pins[m_channel].m_arduino_pin,m_pins[m_channel].m_value);
				
				// end dimming if we override it with hard value
				if( m_pins[m_channel].m_mode==MODE_PWM_DIMMING){
					m_pins[m_channel].m_mode = MODE_PWM;
				};
			} 

			//## write a pwm signal ##
			else if(m_pins[m_channel].m_mode==MODE_PWM || m_pins[m_channel].m_mode==MODE_PWM_DIMMING){ 
				m_pins[m_channel].m_value = m_receive_buffer[3];

				#ifdef DEBUG
				Serial.print("dutycycle of ");
				Serial.print(m_pins[m_channel].m_value);
				Serial.print(" at pin ");
				Serial.println(m_pins[m_channel].m_arduino_pin,DEC);
				#endif

				analogWrite(m_pins[m_channel].m_arduino_pin,m_pins[m_channel].m_value);
				
				// end dimming if we override it with hard value
				if( m_pins[m_channel].m_mode==MODE_PWM_DIMMING){
					m_pins[m_channel].m_mode = MODE_PWM;
				};
			}

			//## single color for all LEDs ##
			else if(m_pins[m_channel].m_mode==MODE_SINGLE_COLOR_WS2812 && m_receive_length>=6){
				value.r = m_receive_buffer[3]; 
				value.g = m_receive_buffer[4]; 
				value.b = m_receive_buffer[5]; // RGB Value -> Blue

				#ifdef DEBUG
				Serial.print("single color for ");			
				Serial.print(m_pins[m_channel].m_ws2812_count);
				Serial.println(" leds");
				#endif

				for(uint8_t i=0; i<m_pins[m_channel].m_ws2812_count; i++){
					m_pins[m_channel].m_ws2812_pointer->set_crgb_at(i, value); // Set value at all LEDs
				}
				m_pins[m_channel].m_ws2812_pointer->sync(); // Sends the value to the LED
			} 

			//## multiple values, for each LED value ##
			else if(m_pins[m_channel].m_mode==MODE_MULTI_COLOR_WS2812){
				uint8_t m_current_led=m_receive_buffer[3];
				for(uint8_t i=4; i<m_receive_length; i+=3){
					value.r = m_receive_buffer[i+0]; 
					value.g = m_receive_buffer[i+1]; 
					value.b = m_receive_buffer[i+2]; // RGB Value -> Blue
					m_pins[m_channel].m_ws2812_pointer->set_crgb_at(m_current_led, value); // Set value at LED found at RUNNING INDEX
					m_current_led++;
				}

				if(m_current_led>=m_pins[m_channel].m_ws2812_count){
					m_pins[m_channel].m_ws2812_pointer->sync(); // Sends the value to the LED only when all value are known
				}	
			}

		} 
		////////////////////////////  SIMPLE SETTING A VALUE ///////////////////////////
		///////////////////////////////  READING A VALUE ///////////////////////////////
		else if(m_receive_buffer[1] == CMD_GET && m_receive_length>=3){
			m_channel = m_receive_buffer[2];
			
			#ifdef DEBUG
			Serial.print("Reading pin");
			Serial.println(m_pins[m_channel].m_arduino_pin,DEC);
			#endif
				
			if(m_pins[m_channel].m_mode == MODE_DIGITAL_INPUT){
				m_response_buffer[0] = digitalRead(m_pins[m_channel].m_arduino_pin);
				m_response_length = 1;

				#ifdef DEBUG
				Serial.print("digital read value: ");
				Serial.println(m_response_buffer[0]);
				#endif

			} else 	if(m_pins[m_channel].m_mode == MODE_ANALOG_INPUT){
				m_pins[m_channel].m_value=analogRead(m_pins[m_channel].m_arduino_pin);
				m_response_buffer[1] =  m_pins[m_channel].m_value >> 8;
				m_response_buffer[0] =  m_pins[m_channel].m_value & 0xff;
				m_response_length = 2;

				#ifdef DEBUG
				Serial.print("Value: ");
				Serial.println(m_pins[m_channel].m_value);
				Serial.print("Sending as ");
				Serial.print(m_response_buffer[1]);
				Serial.print("/");
				Serial.println(m_response_buffer[0]);
				#endif

			}
			

			#ifdef DEBUG
			Serial.print("Buffer: ");
			Serial.println(m_response_length);
			#endif
			
		} 
		///////////////////////////////  READING A VALUE ///////////////////////////////
		///////////////////////////////  CONFIGURE A PIN ///////////////////////////////
		else if(m_receive_buffer[1] == CMD_CONFIG && m_receive_length>=4){
			m_channel = m_receive_buffer[2];
			m_pins[m_channel].m_mode = m_receive_buffer[3];

			if( m_pins[m_channel].m_mode == MODE_SINGLE_COLOR_WS2812 ||  m_pins[m_channel].m_mode == MODE_MULTI_COLOR_WS2812 ){
				m_pins[m_channel].m_ws2812_count = m_receive_buffer[4]; 
			}
			config_pin();
		}
		///////////////////////////////  CONFIGURE A PIN ///////////////////////////////
		///////////////////////////////  DIMM THE PIN ///////////////////////////////
		else if(m_receive_buffer[1] == CMD_DIMM && m_receive_length>=5){
			m_channel = m_receive_buffer[2];
			if(m_pins[m_channel].m_mode==MODE_PWM){
				m_pins[m_channel].m_target_value = max(m_receive_buffer[3], 99); 	// 0-99 = 0-100%
				m_pins[m_channel].m_dimm_interval = m_receive_buffer[4];					// ms
				m_pins[m_channel].m_mode = MODE_PWM_DIMMING;
				m_pins[m_channel].m_next_action  = millis();
			 }
		}
		///////////////////////////////  DIMM THE PIN ///////////////////////////////
	}

	#ifdef DEBUG
	Serial.println("end");
	#endif
};
//================================ parse,  =================================//


//========================= i2c response =========================//
// the arduino wire lib is clearing the i2c buffer whenever it receives the SLAVE READ from the I2C Master
// therefore we can NOT pre-set the tx buffer, but have to do it after this read-call.
// this read-call triggers the onRequestService, to whom we've subscribed. So filling the buffer now should work
// remember to set the MAX_RESPONSE_LENGTH according to your answer. 
void requestEvent(){
	
	#ifdef DEBUG
	Serial.println("Request event");
	#endif
	
	if(m_response_length && m_response_length<MAX_RESPONSE_LENGTH){

		#ifdef DEBUG
		Serial.println("writing");
		for(int i=0; i<m_response_length;i++){
			Serial.print(m_response_buffer[i]);
		}
		#endif

		Wire.write(m_response_buffer,m_response_length);
		m_response_length = 0;
	}
}
//========================= i2c response =========================//
