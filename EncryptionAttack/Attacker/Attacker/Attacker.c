/*
Joseph Finnegan
Maynooth University
Summer 2015

For use with an ATmega128RFA1 microcontroller
Works with an opener and a gate, to show that an attacker can't imitate an opener if messages are encrypted.

Attacker listens in to an open request done nearby.
From the exchange it learns the encrypted "Open" keyword, the encrpyted number sent by the gate, and the encrypted number sent back by the opener in response.
Since the numbers are encrypted the attacker cannot figure out the protocol, and so any attack it make will fail.

Once this is done the attacker can send an open message, but cannot understand the prompt given back or respond with the correct response.
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

unsigned short opener;
unsigned short gate;
//
// Constants
//

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
	printf("dst: %d\r\n", dst);
	printf("src: %d\r\n", src);
	printf("received a message\r\n");
	char string[17];
	for(int i = 4; i<len; i++)
	{
		printf("%c", msgdata[i]);
		string[i-4] = msgdata[i];
	}
	string[16] = '\0';

	if(!canOpen){
		if(firstPacket){
			gate = dst;
			opener = src;
			
			printf("gate: %d\r\n", gate);
			printf("opener: %d\r\n", opener);
			
			sprintf(openKeyword, string);
			firstPacket = false;
			secondPacket = true;
		}
		else if(src == gate && secondPacket){
			sprintf(challenge, string);
			secondPacket = false;
			thirdPacket = true;
		}
		else if(src == opener && thirdPacket){
			sprintf(response,string);
			thirdPacket = false;
			fourthPacket = true;
		}
		//going to assume that the attacker will know if the attack is successful or not (ack or nack). In reality user could input into device that the opening was successful or not.
		else if(fourthPacket /*&& strcmp(string, "ACKACKacACKACKac") == 0*/){
			//the opening was successful - there should be some recognisable difference between the challenge and response.
			//going to simplify and assume its an addition. Would be better to learn what the difference is.
			canOpen = true;
			difference = atol(response) - atol(challenge); //but, these two numbers are encrypted. So the difference calculated will be wrong - and there's no way for the attacker to figure out the right difference.
			
			leds_off(LED_RED);
			leds_on(LED_ORANGE);
			leds_off(LED_GREEN);
		}
		else if(fourthPacket /*&& strcmp(string, "NACKNACKNACKNACK") == 0*/){
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
	else
	{
		if(strcmp(string, "NACKNACKNACKNACK") == 0){
			//Damn.
			leds_off(LED_GREEN);
			leds_off(LED_ORANGE);
			leds_on(LED_RED);
		}
		else if(strcmp(string, "ACKACKacACKACKac") == 0){
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
		newPkt.src = opener; //pretend :)
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
	
}