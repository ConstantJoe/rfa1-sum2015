/*
Joseph Finnegan
Maynooth University
Summer 2015

For use with an ATmega128RFA1 microcontroller
Works with an opener and an attacker, to show that with AES encryption an attacker cannot imitate an opener.

When it receives a packet, it decrypts it.
If that packet contained an open command, it replies with an encrypted random number.
If that packet contained a number, it compares the number with the last number it sent out.
	If newnum = lastnum + 1, it opens/closes the gate and sends an encrypted ACK
	Else it sends an encrypted NACK
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
// You MUST include app.h and implement every function declared
//
#include "app.h"
//
// Include the header files for the various required libraries
//
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
	char dt[17];
} Packet;

unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false;

unsigned long num; //holds the number the gate sends to the opener so the opener's response can be compared to it.
bool open = false; //keeps track of whether the gate is open or not. In reality just indicates whether to turn on or off an LED.
static timer timer1;

//Crypto variables
unsigned char temp;
unsigned short i,j;
char plaintext[17]="This is a test!!";
char   decrypt[17]="................";
unsigned char crypto[17]="_______________";


bool canSend = false;
Packet newPkt;

// Crypto key - hardcoded to this on both sides for now
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
	unsigned char c0=0,c1=0;
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

void application_start()
{
	srand(hw_timer_now_us()); //TODO: use hardware random number generator
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
			printf("sending=   ");
			for (i=0; i<16; i++) printf(" %02x", newPkt.dt[i]);
			printf("\n\r");
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
	
	//For some reason decryption doesn't work unless encrypt has been called. So until I figure out why I'm calling them in pairs.
	encrypt(); //encrypt gibberish
	
	//load in string to be decrypted
	for(int i=0;i<17;i++){
		crypto[i] = string[i];
	}
	
	//Decrypt the string
	decryptMeth();
	
	
	printf("cryptotext=");
	for (i=0; i<16; i++) printf(" %02x", crypto[i]);
	printf("\n\r");
	printf("Decrypted plaintext=\"%s\"\n\r", decrypt);
	
	
	if(strcmp(decrypt, OPEN) == 0){
		
		//getARandomNumber();
		num = rand() % 65534;
		
		newPkt.dst = 0x01;
		newPkt.src = NODE_ID;
		
		//put num into plaintext
		sprintf(plaintext, "%lu", num);
		//encrypt plaintext
		encrypt();
		
		printf("%s\r\n", plaintext);
		printf("cryptotext=");
		for (i=0; i<16; i++) printf(" %02x", crypto[i]);
		printf("\n\r");
		
		//put encrypted plaintext into newPkt.dt
		for(int i=0;i<17;i++){
			newPkt.dt[i] = crypto[i];
		}
		decryptMeth(); //decrypt gibberish
		canSend = true;
	}
	else{
		long temp1 = atol(decrypt);
		
		if(temp1 == num + 1){
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
			sprintf(plaintext, ACK);
			encrypt();
			
			printf("%s\r\n", plaintext);
			printf("cryptotext=");
			for (i=0; i<16; i++) printf(" %02x", crypto[i]);
			printf("\n\r");
			
			//sprintf(newPkt.dt, crypto); //sprintf doesn't like values 0 and 25 -> the /0 and % characters.
			for(int i=0;i<17;i++){
				newPkt.dt[i] = crypto[i];
			}
			canSend = true;
			decryptMeth();
		}
		else{
			//send a nack
			newPkt.dst = 0x01;
			newPkt.src = NODE_ID;
			
			sprintf(plaintext, NACK);
			//encrypt the plaintext
			encrypt();
			
			printf("%s\r\n", plaintext);
			printf("cryptotext=");
			for (i=0; i<16; i++) printf(" %02x", crypto[i]);
			printf("\n\r");
			
			for(int i=0;i<17;i++){
				newPkt.dt[i] = crypto[i];
			}

			canSend = true;
			decryptMeth(); //decrypt gibberish
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
	for(int i=0;i<sizeof(plaintext);i++){
		plaintext[i] = 0;
	}
	for(int i=0;i<sizeof(crypto);i++){
		crypto[i] = 0;
	}
	for(int i=0;i<sizeof(decrypt);i++){
		decrypt[i] = 0;
	}
}

void application_button_pressed(){
	//Not needed.
}

void application_button_released(){
	//Not needed.
}