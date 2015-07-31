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
	uint16_t ender;
} Packet;

typedef struct Req {
	uint16_t dst;
	uint16_t src;
	uint16_t req;
} Req;

Packet response;
uint16_t num;
char str[6];
Req request;
bool canSend;
bool on = false;
//
// Global Variables
//
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
	printf("test\r\n");
	
	//timer_init(&timer1, TIMER_MILLISECONDS, 1000, 100);
	//timer_start(&timer1);
}
//
// Timer tick handler
//
void application_timer_tick(timer *t)
{
	 //there must be some hardware interrupt for this.
	 for(int i=0;i<6;i++){
		 str[i] = 0;
	 }
	 int ind = 0;
	 
	  while(serial_ready()){
		 leds_on(LED_RED);
		 str[ind] = serial_get();
		 if(str[ind] == 'Y'){
			leds_on(LED_GREEN);
			canSend = true;
			break;
		 } 
		 ind++;
	 }
	 
	 response.dst = str[0];
	 response.src = str[2];
	 response.num = str[4];
	 
	 //canSend = true;
	 
	 /*while(serial_ready()){
		 //leds_on(LED_RED);
		 str[ind] = serial_get();
		 ind++;
		 printf("%s\r\n", str);
		 //uint16_t n = str[4];
		 uint16_t e = str[6];
		 if(e == 27){ //Temp method, this isn't a good way of doing this. change to some indicator at the start of the packet to indicate its type. (and an end indicator?)
			leds_on(LED_GREEN);
			 
			//split str into a packet
			response.dst = str[0];
			response.src = str[2];
			response.num = str[4];
			  
			canSend = true;
			//now we can send this response to the gate when we want.
			 break;
		}
		else
		{
			leds_off(LED_GREEN);
		}
	 }*/
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
	//picks up an open from the gate
	//relays it to partner node
	//partner node broadcasts this open
	//partner node picks up the opener's response
	//partner node relays it back here
	//this node sends out the open on button press
	if(on){
		leds_off(LED_ORANGE);
		on = false;
	}else{
		leds_on(LED_ORANGE);
		on = true;
	}

	request.dst = msgdata[0];
	request.src = msgdata[2];
	request.req = msgdata[4];
	
	if(request.src == gate){
		
		if(on = !(on)){
			leds_off(LED_RED);
			
		}
		else{
			leds_on(LED_RED);
			
		}
		//num = newPkt.num;
		unsigned char buf[6];
		//memcpy(&buf, &request, sizeof(Req));
		sprintf(buf, "%02d%02d%02d%c", request.dst, request.src, request.req, 'Y');
		serial_puts(buf); //Relay the whole packet over.
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
	if(canSend){
		if(tx_buffer_inuse == false)
		{
			tx_buffer_inuse = true;
			canSend = false;
			
			memcpy(&tx_buffer, &response, sizeof(Packet));
			
			radio_send(tx_buffer, sizeof(Packet), response.dst);
		}
	}
}

void application_button_released()
{
	
}