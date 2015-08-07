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
// Include the header files for the various required libraries
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
	
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 100);
	timer_start(&timer1);
}
//
// Timer tick handler
//
void application_timer_tick(timer *t)
{
	 //picks up a packet from the noisemaker - this must be the response that that opener gave. Broadcast it out to open the gate.
	 
	 //This is where the program breaks, I can't pick up data from the serial line properly
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
//
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
	//not needed
}