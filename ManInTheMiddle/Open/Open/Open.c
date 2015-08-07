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
this response to the attacker, who broadcasts it, opening the gate.

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
// Include the header files for the various required libraries
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
} Packet;

typedef struct Req {
	uint16_t dst;
	uint16_t src;
	uint16_t req;
} Req;

uint16_t num = 15;
Packet newPkt;
Req request;
static timer timer1;
bool canSend = false;
uint16_t gate = 0x02;
bool open = false;

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
	
	radio_init(NODE_ID, false);
	radio_set_power(1);
	radio_start();
	
	serial_init(9600);
	
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 100);
	timer_start(&timer1);
}

//
// Timer tick handler
//
void application_timer_tick(timer *t)
{
	if(canSend){
		if(tx_buffer_inuse == false)
		{
			tx_buffer_inuse = true;
			canSend = false;
			
			memcpy(&tx_buffer, &newPkt, sizeof(Packet));
			
			radio_send(tx_buffer, sizeof(Packet), newPkt.dst);
		}
	}
	
}

//
// This function is called whenever a radio message is received
//
void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{
	request.dst = msgdata[0];
	request.src = msgdata[2];
	request.req = msgdata[4];
	
	if(request.req == 1){
		//sweet, the gate opened.
		/*if(!open){
			leds_on(LED_GREEN);
			leds_off(LED_RED);
			leds_off(LED_ORANGE);
			open = true;
		}
		else{
			leds_off(LED_GREEN);
			leds_off(LED_RED);
			leds_off(LED_ORANGE);
			open = false;
		}*/
		newPkt.dst = request.src;
		newPkt.src = NODE_ID;
		newPkt.num = num;
		canSend = true;
		num++;
	}
}

//
// This function is called whenever a radio message has been transmitted
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

//This is not strictly necessary but it allows the opener to begin communication instead of waiting for a packet from the gate.
void application_button_pressed()
{
	newPkt.dst = gate;
	newPkt.src = NODE_ID;
	newPkt.num = num;
	canSend = true;
	num++;
}

void application_button_released()
{
	//not needed
}