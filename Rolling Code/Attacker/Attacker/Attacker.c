//
// AVR C library
//
#include <avr/io.h>
//
// Standard C include files
//
#include <stdbool.h>
#include <stdio.h>
#include <stdio.h>
//
// You MUST include app.h and implement every function declared
//
#include "app.h"
#include "string.h"
//
// Include the header files for the various required libraries
//
#include "simple_os.h"
#include "button.h"
#include "leds.h"
#include "radio.h"
#include "serial.h"
#include "hw_timer.h"

unsigned short gate = 0x02;
//
// Constants
//
typedef struct Packet {
	uint16_t dst;
	uint16_t src;
	uint16_t num;
} Packet;

typedef struct SerialMessage {
	uint16_t start;
	uint16_t msg;
	uint16_t num;
	uint16_t endd;
} SerialMessage;

Packet newPkt;
SerialMessage ser;
uint16_t oldnum;
uint16_t newnum;
char str[10];
uint16_t justsent = 0x00;
//
// Global Variables
//
static timer timer1;

// Buffer for transmitting radio packets
unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false; // Check false and set to true before sending a message. Set to false in tx_done
bool savedAMessage = false;
bool canOpen = false;
//
// App init function
//

void application_start()
{
	leds_init();
	button_init();
	radio_init(NODE_ID, true); //true indicates receives radio message for ALL nodes
	radio_set_power(1);
	radio_start();
	serial_init(9600);
	printf("test\r\n");
	ser.start = 'X';
	ser.endd = 'Y';
	
}
//
// Timer tick handler
//
void application_timer_tick(timer *t)
{

}

void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{
	newPkt.dst = msgdata[0];
	newPkt.src = msgdata[2];
	newPkt.num = msgdata[4];
	
	//if this message is meant for the gate, then we should block this message, and send it on ourselves when we want.
	
	//SEND OUT NOISE
	//send message down serial line indicating that the partner node should blast out noise.
	//wait for a response saying first ahead
	//then send the num yourself.
	leds_on(LED_RED);
	
	if(newPkt.dst == gate && newPkt.num != justsent){
		
		if(!savedAMessage){
			leds_on(LED_ORANGE);
			leds_off(LED_GREEN);
			ser.msg = 1;
			ser.num = newPkt.num;
		
			unsigned char buf[8];
			
			//sprintf(buf, " %c%02d%02d %c", ser.start, ser.msg, ser.num, ser.endd);
			//printf(buf);
			printf(" %c%02d%02d %c", ser.start, ser.msg, ser.num, ser.endd);
			justsent = ser.num;
			
			//serial_puts(buf);
			
			savedAMessage = true;
		}
		else{
			leds_on(LED_GREEN);
			leds_off(LED_ORANGE);
			ser.msg = 2;
			ser.num = newPkt.num;
			
			unsigned char buf[8];
			
			sprintf(buf, " %c%02d%02d %c", ser.start, ser.msg, ser.num, ser.endd);
			serial_puts(buf);
			
			savedAMessage = false;
		}		
	}
}

//
// This function is called whenever a radio message has been transmitted
// You need to free up the transmit buffer here
//
void application_radio_tx_done()
{
	tx_buffer_inuse = false;
	
	//empty buffers after data has been sent
	for(int j=0; j<RADIO_MAX_LEN; j++)
	{
		tx_buffer[j] = 0x00;
	}
}

void application_button_pressed()
{


}

void application_button_released()
{
	
}