/*
Joseph Finnegan
Maynooth University
Summer 2015

For use with an ATmega128RFA1 microcontroller
Works with an opener and an attacker, to show that an attacker can imitate an opener easily if messages are not encrypted.

When it receives a packet:
If that packet contained an open command, it replies with a random number.
If that packet contained a number, it compares the number with the last number it sent out.
	If newnum = lastnum + 1, it opens/closes the gate and sends an ACK
	Else it sends a NACK
	
Even if the gate was modified to only accept packets from certain nodes, with no encryption the attacker could easily steal the id of an opener and use that.	
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

//Constants
#define OPEN "OPENOPENOPENOPEN"
#define ACK "ACKACKacACKACKac"
#define NACK "NACKNACKNACKNACK"

typedef struct Packet {
	uint16_t dst;
	uint16_t src;
	char dt[16];
} Packet;

unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false;

unsigned long num; //holds the number the gate sends to the opener so the opener's response can be compared to it.
bool open = false; //keeps track of whether the gate is open or not. In reality just indicates whether to turn on or off an LED.
static timer timer1;
bool canSend = false;
Packet newPkt;

void application_start()
{
	srand(hw_timer_now_us()); //TODO: use hardware random number generator. This produces the same sequence of random numbers because its called immediately. But it'll do for now.
	//Since this is at the very start does it generate the same random numbers every time?
	leds_init();
	button_init();
	
	radio_init(NODE_ID, false);
	radio_set_power(1);
	radio_start();
	
	serial_init(9600);
	
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 100);
	timer_start(&timer1);
}

/*
Hardware random number generation.
This stops working once the board receives a packet for some reason. 
Must be going into some mode that disables random number generation (RX_SAFE_MODE?). 
Going back to software generation for now.
*/
/*void getARandomNumber(){
	unsigned int rand = 0;
	for(unsigned int i=0;i<8;i++){
		unsigned int ra = PHY_RSSI && 0b01000000;
		unsigned int ra2 = PHY_RSSI && 0b00100000;
		//printf("random number: %u\r\n", ra);
		//printf("random number: %u\r\n", ra2);
		ra  = ra << ((2*i)+1);
		ra2 = ra2 << (2*i);
		
		rand = rand | ra;
		rand = rand | ra2;
	}
	printf("random number: %u\r\n", rand);
	num = rand;
}*/

void application_timer_tick(timer *t)
{
	//TODO: after a random amount of time, send a NACK.
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

void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{
	char string[17];
	
	for(int i = 4; i<len; i++)
	{
		printf("%c", msgdata[i]);
		string[i-4] = msgdata[i];
	}
	string[16] = '\0';
	
	if(strcmp(string, OPEN) == 0){
		printf("sending a random number\r\n");
		
		num = rand() % 65534;
		char number[17];
		sprintf(number, "%lu", num);
		
		newPkt.dst = 0x01;
		newPkt.src = NODE_ID;
		
		sprintf(newPkt.dt, number);
		canSend = true;
	}
	else{
		long temp1 = atol(string);
		
		if(temp1 == num + 1){
			printf("sending an ack\r\n");
			//turn on leds, open gate.
			if(open){
				leds_off(LED_GREEN);
				leds_off(LED_RED);
				leds_off(LED_ORANGE);
				open = false;
			}
			else{
				leds_on(LED_GREEN);
				leds_off(LED_RED);
				leds_off(LED_ORANGE);
				open = true;
			}
			newPkt.dst = src;
			newPkt.src = NODE_ID;
			sprintf(newPkt.dt, ACK);
			canSend = true;
		}
		else{
			//send a nack
			printf("sending a nack\r\n");
			newPkt.dst = 0x01;
			newPkt.src = NODE_ID;
			sprintf(newPkt.dt, NACK);
			canSend = true;
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
	newPkt.dst = 0;
	newPkt.src = 0;
	for(int i=0;i<sizeof(newPkt.dt);i++){
		newPkt.dt[i] = 0;
	}
}

void application_button_pressed(){
	//Not needed.
}

void application_button_released(){
	//Not needed.
}