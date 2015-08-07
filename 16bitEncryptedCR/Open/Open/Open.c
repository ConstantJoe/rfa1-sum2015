/*
Joseph Finnegan
Maynooth University
Summer 2015

For use with an ATmega128RFA1 microcontroller
Works in tandem with a gate.

On button press, sends an encrypted OPEN request to a partnered gate.
When it receives a packet, it decrypts it.
If that packet contained a number, it increments it, re-encrypts it, and sends it back to the gate.
If that packet contained an ack or nack, it knows if its open attempt has been successful or not.
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

//
// Constants
//
#define OPEN "OPENOPENOPENOPEN"
#define ACK "ACKACKacACKACKac"
#define NACK "NACKNACKNACKNACK"

typedef struct Packet {
	uint16_t dst;
	uint16_t src;
	unsigned char dt[17];
} Packet;


//
// Global Variables
//
static timer timer1;

// Buffer for transmitting radio packets
unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false; // Check false and set to true before sending a message. Set to false in tx_done

int dest = 0x02; //We're assuming the opener knows the id of its gate
bool canSend = false;
Packet newPkt;

//crypto variables
unsigned char temp;
unsigned short i,j;
char plaintext[17]="This is a test!!";
char   decrypt[17]="................";
unsigned char crypto[17]="_______________";

//AES key is hardcoded on both sides for now
char aesKey[16]={'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A'};

/*
Encryption part of AES cipher
See section 9.8.8 of Atmega128RFA1 datasheet for details
*/
void encrypt(){
	// reset AES pointers
	temp=AES_CTRL;

	for (i=0; i<16; i++)
	AES_KEY = aesKey[i];

	// set mode=encrypt
	AES_CTRL = 0;

	// write data - must be 16 bytes
	for (i=0; i<16; i++)
	AES_STATE = plaintext[i];

	// start operation - with ECB encrypt no interrupts
	AES_CTRL = (1<<AES_REQUEST);

	// poll for completion
	unsigned char c0=0;
	while (!AES_STATUS_struct.aes_done) c0++;

	// read data
	for (i=0; i<16; i++)
	crypto[i] = AES_STATE;
	
	// read key for decryption
	for (i=0; i<16; i++)
	aesKey[i] = AES_KEY;

	crypto[16] = '\0';
}

/*
Decryption part of AES cipher
See section 9.8.8 of ATmega128RFA1 datasheet for details
*/
void decryptMeth(){
	// read ctrl to reset pointers
	temp=AES_CTRL;

	// rewrite key
	for (i=0; i<16; i++)
	AES_KEY = aesKey[i];

	// set mode=decrypt
	AES_CTRL = (1<<AES_DIR);

	// write crypto text
	for (i=0; i<16; i++)
	AES_STATE = crypto[i];

	// start operation - ECB decrypt with no interrupts
	AES_CTRL = (1<<AES_REQUEST) | (1<<AES_DIR);

	// poll for completion
	while (!AES_STATUS_struct.aes_done);

	// read data
	for (i=0; i<16; i++)
	decrypt[i] = AES_STATE;
}

//
// Application init
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

void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{
	char string[17];
	for(int i = 4; i<len; i++)
	{
		string[i-4] = msgdata[i];
	}
	string[16] = '\0';

	encrypt();
	
	//put string into buffer
	for(int i=0;i<17;i++){
		crypto[i] = string[i];
	}
	//decrypt buffer
	decryptMeth();
	
	printf("cryptotext=");
	for (i=0; i<16; i++) printf(" %02x", crypto[i]);
	printf("\r\n");
	printf("   Decrypted plaintext=\"%s\"\n\r", decrypt);
	
	if(strcmp(decrypt, NACK) == 0){
		//Uh oh, our last attempt at opening the gate failed.
		leds_off(LED_GREEN);
		leds_off(LED_ORANGE);
		leds_on(LED_RED);
	}
	else if(strcmp(decrypt, ACK) == 0){
		//Yay, the gate has opened!
		leds_off(LED_RED);
		leds_off(LED_ORANGE);
		leds_on(LED_GREEN);
	}
	else{
		//its a number TODO: only go in here if expecting a number (having already sent one out)
		
		//cast decrypted string as a long
		long temp1 = atol(decrypt);
		temp1++; //this is the protocol - add one to the received number and send it back.
		
		//put long back into buffer
		sprintf(plaintext, "%lu", temp1);
		
		//encrypt the buffer
		encrypt();
		
		printf("%s\r\n",plaintext);
		printf("cryptotext=");
		for (i=0; i<16; i++) printf(" %02x", crypto[i]);
		printf("\n\r");
		
		newPkt.dst = src;
		newPkt.src = NODE_ID;
		for(int i=0;i<17;i++){
			newPkt.dt[i] = crypto[i];
		}
		
		decryptMeth(); //decrypt gibberish - encrypt and decrypt need to be called as pairs for now.
		canSend = true; //indicate the packet is ready to be sent
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
	for(int i=0;i<sizeof(plaintext);i++){
		plaintext[i]= 0;
	}
	for(int i=0;i<sizeof(crypto);i++){
		crypto[i]= 0;
	}
	for(int i=0;i<sizeof(decrypt);i++){
		decrypt[i]= 0;
	}
}

void application_button_pressed()
{
	//Send a message to the gate asking it to open.
	
	newPkt.dst = dest;
	newPkt.src = NODE_ID;
	
	sprintf(plaintext, OPEN);
	//encrypt plaintext
	encrypt();
	
	printf("%s\r\n", plaintext);
	printf("cryptotext=");
	for (i=0; i<16; i++) printf(" %02x", crypto[i]);
	printf("\r\n");
	
	//put encrypted text into the packet
	for(int i=0;i<17;i++){
		newPkt.dt[i] = crypto[i];
	}
	
	canSend = true;
	
	decryptMeth(); //decrypt gibberish - encrypt and decrypt need to be called as pairs for now.
	
	leds_off(LED_RED);
	leds_on(LED_ORANGE);
	leds_off(LED_GREEN);
}

void application_button_released()
{
	//Not needed.
}