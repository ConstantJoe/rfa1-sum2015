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

unsigned short opener;
unsigned short gate;
//
// Constants
//
typedef struct Packet {
	uint16_t dst;
	uint16_t src;
	//uint64_t data;
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

bool firstPacket = true;
bool secondPacket = false;
bool thirdPacket = false;
bool fourthPacket = false;
bool canOpen = false;
bool expectingAck = false;
long difference = 0;
char openKeyword[17];
char challenge[17];
char response[17];
char ackackack[17];
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
	
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 100);
	timer_start(&timer1);
}
//
// Timer tick handler
//
void application_timer_tick(timer *t)
{
	if(canSend){
		printf("sending a message\r\n");
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
// You must copy any data you need out of the packet - as 'msgdata' will be overwritten by the next message
//

//print out the encrypted message, show that you can't increment it
//(you know that the opening protocol is increment - if your security relies on the protocol not being known then it will eventually be broken)
//try increment it anyway, show that the response isn't the equivalent of ACKACKACK.
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
			printf("1\r\n");
			gate = dst;
			opener = src;
			
			printf("gate: %d\r\n", gate);
			printf("opener: %d\r\n", opener);
			
			sprintf(openKeyword, string);
			firstPacket = false;
			secondPacket = true;
		}
		else if(src == gate && secondPacket){
			printf("2\r\n");
			sprintf(challenge, string);
			secondPacket = false;
			thirdPacket = true;
		}
		else if(src == opener && thirdPacket){
			printf("3\r\n");
			sprintf(response,string);
			thirdPacket = false;
			fourthPacket = true;
		}
		//going to assume that the attacker will know if the attack is successful or not (ack or nack). In reality user could input into device that the opening was successful or not.
		else if(fourthPacket){
			//the opening was successful - there should be some recognisable difference between the challenge and response.
			//going to simplify and assume its an addition. Would be better to learn what the difference is.
			printf("4\r\n");
			canOpen = true;
			difference = atol(response) - atol(challenge);
			sprintf(ackackack, string);
			leds_off(LED_RED);
			leds_on(LED_ORANGE);
			leds_off(LED_GREEN);
		}	
	}
	else
	{
		if(expectingAck){
			expectingAck = false;
			if(strcmp(string, ackackack) == 0){
				//Yes, gate has opened!
				leds_off(LED_RED);
				leds_off(LED_ORANGE);
				leds_on(LED_GREEN);
			}
			else{
				//Damn.
				leds_off(LED_GREEN);
				leds_off(LED_ORANGE);
				leds_on(LED_RED);
			}
		}
		else{
			if(src == gate){
				long temp1 = atol(string); //its gonna try incrementing the value. But since we don't know the key for encryption, we can't do this properly.
				temp1 += difference;

				newPkt.dst = src;
				if(opener != 0){
					newPkt.src = opener;
				}
				else{
					newPkt.src = NODE_ID;
				}
				sprintf(newPkt.dt, "%lu", temp1);
				canSend = true;
				
				leds_off(LED_RED);
				leds_on(LED_ORANGE);
				leds_off(LED_GREEN);
				
				expectingAck = true;
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
	}
}

void application_button_released()
{
	
}