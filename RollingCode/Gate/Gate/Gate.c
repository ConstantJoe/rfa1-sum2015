
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


typedef struct Packet {
	uint16_t dst;
	uint16_t src;
	char dt[17];
} Packet;

unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false;

int key = 123; //hardcoded for now.
unsigned long num;
bool open = false;
static timer timer1;
unsigned char temp;
unsigned short i,j;
char plaintext[17]="This is a test!!";
char   decrypt[17]="................";
unsigned char crypto[17]="_______________";
bool canSend = false;
Packet newPkt;

char aesKey[16]={'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A'};

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
	leds_init();
	button_init();
	
	radio_init(NODE_ID, false);
	radio_set_power(1);
	radio_start();
	serial_init(9600);
	printf("test");
	
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 100);
	timer_start(&timer1);
}

//This stops working once the board receives a packet for some reason. Must be going into some mode that disables random number generation (RX_SAFE_MODE?). Going back to software generation for now.
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
	
	encrypt(); //For some reason decryption doesn't work unless encrypt has been called. So until I figure out why I'm doing this.
	
	for(int i=0;i<17;i++){
		crypto[i] = string[i];
	}
	
	decryptMeth();
	
	printf("cryptotext=");
	for (i=0; i<16; i++) printf(" %02x", crypto[i]);
	printf("\n\r");
	printf("Decrypted plaintext=\"%s\"\n\r", decrypt);
	
	if(strcmp(decrypt, "OPENOPENOPENOPEN") == 0){
		
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
		decryptMeth();
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
			sprintf(plaintext, "ACKACKacACKACKac");
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
			
			sprintf(plaintext, "NACKNACKNACKNACK");
			
			encrypt();
			
			printf("%s\r\n", plaintext);
			printf("cryptotext=");
			for (i=0; i<16; i++) printf(" %02x", crypto[i]);
			printf("\n\r");
			
			for(int i=0;i<17;i++){
				newPkt.dt[i] = crypto[i];
			}

			canSend = true;
			decryptMeth();
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