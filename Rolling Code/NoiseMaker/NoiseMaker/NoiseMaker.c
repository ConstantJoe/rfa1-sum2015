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

//
// Global Variables
//
static timer timer1;

// Buffer for transmitting radio packets
unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false; // Check false and set to true before sending a message. Set to false in tx_done
uint16_t oldnum;
uint16_t newnum;

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
bool canOpen = false;
uint16_t gate = 0x02;
char str[8];
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
	
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 1000);
	timer_start(&timer1);
}
//
// Timer tick handler
//
void application_timer_tick(timer *t)
{
	//call this over and over very fast
	/* if (radio_rxed()){ //haven't done any processing whatsoever
		//send out noise?
	 }*/
	 
	 //str = 0;
	 int ind = 0;
	 
	 /*for(int i=0;i<8;i++){
		 str[i] = 0;
	 }*/
	 
	 /*int try = read();
	 printf("%d", try);*/
	 
	 /*while(serial_ready()){ //I've a bad feeling about this -> can use read instead?
		 leds_on(LED_RED);
		 str[ind] = serial_get();
		 if(str[ind] == 'Y'){
				leds_on(LED_GREEN);
			  break;
		 }
		 ind++;
	 }*/
	 
	 if(serial_ready()){
		 for(int i=0;i<8;i++){
			 leds_on(LED_RED);
			 //printf("a");
			 
			 
			 str[i] = serial_get();
			 printf("%c", str[i]);
			 
		 }
	 }
	 leds_off(LED_RED);
	 
	 //leds_off(LED_RED);
	 ser.start = str[0];
	 ser.msg = str[2];
	 ser.num = str[4];
	 ser.endd = str[6];
	 
	 if(str[3] == '1'){
		//save the number transmitted
		leds_on(LED_GREEN);
		oldnum = ser.num;
		
		//block the message
		newPkt.dst = 0xFF;
		newPkt.num = 0xFF;
		memcpy(&tx_buffer, &newPkt, sizeof(Packet));	
		radio_send(tx_buffer, sizeof(Packet), 0xFF);
	 }
	 else if(str[3] == '2'){
		 leds_on(LED_ORANGE);
		//save the number transmitted
		newnum = ser.num;
		
		//block the message
		newPkt.dst = 0xFF;
		newPkt.num = 0xFF;
		memcpy(&tx_buffer, &newPkt, sizeof(Packet));	
		radio_send(tx_buffer, sizeof(Packet), 0xFF);
		
		//send the old saved number - gate opens.
		if(tx_buffer_inuse == false)
		{
			tx_buffer_inuse = true;
			
			newPkt.dst = gate;
			newPkt.num = oldnum;
			
			memcpy(&tx_buffer, &newPkt, sizeof(Packet));
			
			radio_send(tx_buffer, sizeof(Packet), newPkt.dst);
		}
		
		//now on button press we can open the gate.
		canOpen = true;
	 }
}

void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{

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

void application_button_pressed()
{
	if(canOpen){
		if(tx_buffer_inuse == false)
		{
			tx_buffer_inuse = true;
			
			newPkt.dst = gate;
			newPkt.num = newnum;
			
			memcpy(&tx_buffer, &newPkt, sizeof(Packet));
			
			radio_send(tx_buffer, sizeof(Packet), newPkt.dst);
			
			canOpen = false;
		}
	}
	
	for(int i=0;i<8;i++){
		printf("%c\r\n", str[i]);
	}
	printf("blah");
}

void application_button_released()
{
	//
}