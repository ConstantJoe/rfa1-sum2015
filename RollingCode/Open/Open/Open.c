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
// Constants
//
typedef struct Packet {
	uint16_t dst;
	uint16_t src;
	//uint64_t data;
	unsigned char dt[17];
} Packet;


//
// Global Variables
//
static timer timer1;

// Buffer for transmitting radio packets
unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false; // Check false and set to true before sending a message. Set to false in tx_done

int dest = 0x02;
int key = 123; //hardcoded for now.
bool canSend = false;
Packet newPkt;

//
// App init function
//
unsigned char temp;
unsigned short i,j;
char plaintext[17]="This is a test!!";
char   decrypt[17]="................";
unsigned char crypto[17]="_______________";

char aesKey[16]={'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A'}; //TODO: key generation.

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

void application_start()
{
	leds_init();
	button_init();
	radio_init(NODE_ID, false);
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
	
	if(strcmp(decrypt, "NACKNACKNACKNACK") == 0){
		//Uh oh, our last attempt at opening the gate failed.
		leds_off(LED_GREEN);
		leds_off(LED_ORANGE);
		leds_on(LED_RED);
	}
	else if(strcmp(decrypt, "ACKACKacACKACKac") == 0){
		//Yay, the gate has opened!
		leds_off(LED_RED);
		leds_off(LED_ORANGE);
		leds_on(LED_GREEN);
	}
	else{
		//its a number TODO: only go in here if expecting a number (having already sent one out)
		
		//cast decrypted string as a long
		long temp1 = atol(decrypt);
		temp1++; //this is the protocol
		
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
		
		decryptMeth();
		canSend = true;
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
	newPkt.dst = dest;
	newPkt.src = NODE_ID;
	sprintf(plaintext, "OPENOPENOPENOPEN");
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
	decryptMeth();
	
	leds_off(LED_RED);
	leds_on(LED_ORANGE);
	leds_off(LED_GREEN);
}

void application_button_released()
{
	
}