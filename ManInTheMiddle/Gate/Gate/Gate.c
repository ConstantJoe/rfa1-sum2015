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
#include <string.h>
#include "inttypes.h"

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

typedef struct Client {
	uint16_t id;
	uint16_t num;
} Client;

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

unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false;

static timer timer1;
Client cl; //in reality should have an array of these.
Packet newPkt;
Req request;
uint16_t rollingCodeSize = 5;
bool canSend = false;
bool on = false;

void application_start()
{
	cl.id = 0x01; //Initial setup of clients
	cl.num = 15;
	
	leds_init();
	button_init();
	
	radio_init(NODE_ID, false);
	radio_set_power(1);
	radio_start();
	
	serial_init(9600);
	
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 2000);
	timer_start(&timer1);
	
	request.dst = 0xFF; //broadcast the request
	request.src = NODE_ID;
	request.req = 1;
}

void application_timer_tick(timer *t)
{
	//every 2 seconds, send out a request for opening.
	if(tx_buffer_inuse == false){
		tx_buffer_inuse = true;
		
		
		memcpy(&tx_buffer, &request, sizeof(Req));
		printf("sending req\r\n");
		radio_send(tx_buffer, sizeof(Req), 0xFFFF); //0xFFFF for broadcast
	}
}

void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{
	newPkt.dst = msgdata[0];
	newPkt.src = msgdata[2];
	newPkt.num = msgdata[4];
	
	printf("Open request from: %d\r\n", newPkt.src);
	printf("Rolling code value sent: %d\r\n", newPkt.num);
	printf("Actual code: %d\r\n", cl.num);
	printf("Size of window: %d\r\n", rollingCodeSize);
	
	//TODO: fix for wraparound.
	if(newPkt.src == cl.id){ //change this to search array of clients
		if(newPkt.num < cl.num){
			//imposter, the number given is too small. Trying to reuse an already accepted response.
			printf("Number too small, don't open the gate!\r\n\r\n");
			leds_off(LED_GREEN);
			leds_on(LED_RED);
			leds_off(LED_ORANGE);
		}
		else{
			uint16_t temp = newPkt.num - cl.num;
			if(temp <= rollingCodeSize){
				//acceptable packet
				cl.num = newPkt.num + 1; //this is the value we expect the NEXT packet to have.
				printf("Open the gate!\r\n\r\n");
				//flip green light
				if(!on){
					leds_on(LED_GREEN);
					leds_off(LED_RED);
					leds_off(LED_ORANGE);
					on = true;
				}
				else{
					leds_off(LED_GREEN);
					leds_off(LED_RED);
					leds_off(LED_ORANGE);
					on = false;
				}
			}
			else{
				//number given is too big. Probably also an imposter.
				printf("Number too big, don't open the gate!\r\n\r\n");
				leds_off(LED_GREEN);
				leds_off(LED_RED);
				leds_on(LED_ORANGE);
			}
		}
	}
}

void application_radio_tx_done()
{
	tx_buffer_inuse = false;
	
	//empty buffers after data has been sent
	for(int j=0; j<RADIO_MAX_LEN; j++)
	{
		tx_buffer[j] = 0x00;
	}
}

void application_button_pressed(){
	//Not needed.
}

void application_button_released(){
	//Not needed.
}