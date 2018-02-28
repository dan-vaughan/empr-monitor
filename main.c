#include "lpc_types.h"
#include "lpc17xx_uart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "lib/pindef.h"
#include "lib/keypad.h"
#include "lib/display.h"

#define TIME 10000000

I2C i2c(SDA, SCL);        // sda, scl
Serial pc; // tx, rx
DMX dmx;

int state = 0;
int interrupt = 0;
int total_packets = 0;
int inputlen = 0;


char packet[512];	

void packet_begin();

void myaction(int button)
{
	//printchar(labels[button]);

	if(state == 1 && labels[button] == '7'){
		pc.write("Packet begin...\n\r");
		packet_begin();
		state = 255;
		return;
	}
	
	//From state 0 to {1,2}	
	if (state == 0){
		if (labels[button] == 'A'){
			state = 1;
			menu(1);
			pc.write("Real mode selected.\n\r");
		}

		if (labels[button] == 'B'){
			state = 2;	
			menu(2);
			pc.write("DMX mode selected.\n\r");
		}
	}
	// From state 1 (real) to 2 (packet inspector)
	if (state == 1){
		if ((labels[button] == 'B') || (labels[button] == 'C') || (labels[button] == 'D')){
			state = 2;
			menu(2);
			pc.write("DMX mode selected.\n\r");
		}
	}
	// From state 2 (packet inspector) to 1 (real-time)	
	if (state == 2){
		if (labels[button] == 'A'){
			state = 1;
			menu(1);
			pc.write("Real mode selected.\n\r");
		}

		if (labels[button] == 'C'){
			state = 3;
			menu(3);
			pc.write("Capturing next packet...\n\r");
		}
		
		if (labels[button] == 'D'){
			state = 5;
			menu(5);
			pc.write("Trigger mode selected.\n\r");
		}
	}

	if (state == 3){
		if(labels[button] == 'A'){
			state = 1;
			menu(1);
		}
		
		if(labels[button] == 'B'){
			state = 2;
			menu(2);
		}
		
		if(labels[button] == 'D'){
			state = 5;
			menu(5);
		}
	}

	if (state == 4){

		printchar(labels[button]);

		inputlen++;	
	
		if(labels[button] == 'A'){
			state = 1;
			menu(1);
		}
		
		if(labels[button] == 'B'){
			state = 2;
			menu(2);
		}
		
		if((labels[button] == 'C') || (labels[button] == 'D')){
			state = 2;
			menu(2);
		}

	
	}


}

void menu(int screen)
{
	clear_display();
	return_home();

	if (screen == 0){
		setup_display();
		clear_display();
		printstr("A: Real-time");
		shift_line();
		printstr("B: DMX Inspector");
		return_home();
	}
	
	if (screen == 1){
		clear_display();
		printstr("Real-time:    B");
		putcustom(0x20);
		shift_line();
	}
	
	if (screen == 2){
		clear_display();
		return_home();
		printstr("C: Capture Next");
		shift_line();
		printstr("D: Set Trigger");
	}

	if (screen == 3){
		clear_display();
		return_home();
		printstr("Capturing Packet...");
	}

	if (screen == 4){
		clear_display();
		return_home();
		printstr("Select Slot:");

	}

	if (screen == 5){
		clear_display();
		return_home();
		printstr("Trigger...");

	}
}

void UART1_IRQHandler(void)
{
//	uint32_t intsrc;
//	intsrc = UART_GetIntId((LPC_UART_TypeDef*)LPC_UART1);
//	pc.write("Interrupt Detected!\n\r");
//    NVIC_SetPriority(UART1_IRQn, 32);
	
	uint32_t iir = LPC_UART1->IIR;
	uint32_t rls = LPC_UART1->LSR;
//	NVIC_ClearPendingIRQ(UART1_IRQn);
	interrupt = 1;
}

void startup_screen()
{
	setup_display();
	printstr("                ");
	shift_line();
	printstr("                ");
	return_home();
	printstr("A: Real-time");
	shift_line();
	printstr("B: DMX Inspector");
	return_home();
}

void packet_begin()
{
	//Reads and prints the first 20 bytes.

	//Check if 1-reads correspond to BREAK and MAB
 
	int count = 0;
	int read_success;
	uint8_t buf[5]; 
	char cbuf[5];
	char strbuf[10];

	while(1){

		if(count>20){
			pc.write("Terminating...\n\r");
			return;
		}

		read_success = dmx.read((uint8_t*)cbuf);

		if(read_success < 1){
			cbuf[0] = 0;		//if UART_Read fails, set buf to 0
			cbuf[1] = 0;		//so we don't print garbage
			cbuf[2] = 0;
			cbuf[3] = 0;
			cbuf[4] = 0;
			continue;
		}
	
		for(int i = 0; i<read_success; i++){
			if(i==0){
				sprintf(strbuf,"%d - %#2.2x\n\r",read_success, cbuf[i]);
				pc.write(strbuf);
			}else{
				sprintf(strbuf,"  - %#2.2x\n\r",cbuf[i]);
				pc.write(strbuf);
			}
			count++;
		}
	}
}

void grab_four_bytes(char* cbuf)
{

	//Reads and writes the next four bytes to cbuf

	int count = 0;
	int read_success;
	uint8_t buf[5]; 
	char strbuf[10];
	int j=0;
	while(1){

		if(count>5){
			//pc.write("Terminating...\n\r");
			return;
		}

		read_success = dmx.read(buf);

		for(int i = 0; i<read_success; i++){
			if(i==0){
			//	sprintf(strbuf,"%d - %#2.2x\n\r",read_success, buf[i]);
			//	pc.write(strbuf);
				cbuf[j] = buf[i];
			}else{
			//	sprintf(strbuf,"  - %#2.2x\n\r",buf[i]);
			//	pc.write(strbuf);
				cbuf[j] = buf[i];
			}
			j++;
			count++;
		}
	}
}


void grab_packet()
{
	//Reads and writes the next 512 bytes into packet char array

	int count = 0;
	int read_success;
	uint8_t buf[5]; 
	char strbuf[10];
	char mstrbuf[512];
	int j=0;

	while(1){

		if(count>507){
			pc.write("\n\rTerminating...\n\r");
			return;
		}

		read_success = dmx.read(buf);
		//pc.write("Trying read...\n\r");
		if(read_success == 0){ continue;}

		for(int i = 0; i<read_success; i++){	
			
//			if (packet[j+i] != buf[i]){				//CODE DETECTS CHANGES BETWEEN NEW AND OLD PACKET
//				sprintf(strbuf,"%d - %#2.2x \n\r", j+i, buf[i]);
//				pc.write(strbuf);
//			}

//			if(i==0){
//				sprintf(strbuf,"%d- %d - %#2.2x\n\r", j, read_success, buf[i]);
//				pc.write(strbuf);
				packet[j] = buf[i];					//THIS FILLS ENTIRE PACKET
//			}else{
//				sprintf(strbuf,"     - %#2.2x\n\r",buf[i]);
//				pc.write(strbuf);
//				packet[j] = buf[i]; 
//			}
		}
			j++;
			count++;
	}
}

void print_packet()
{

	char strbuf[128];
	char strbuf2[5];	
	char message[20];

	strbuf[0] = '\0';
	strbuf2[0] = '\0';

	sprintf(message,"Packet no. %d:\n\r\n\r", total_packets);
	pc.write(message);

	for(int i=0; i<16; i++){
		for (int j=0; j<32;j++){
			sprintf(strbuf2, "%2.2x-",packet[((i*16)+j)]);
			strcat(strbuf, strbuf2);
		}
		pc.write(strbuf);
		strbuf[0] = '\0';
	}
	pc.write("\n\r");
}

void wait_for_break()
{
	uint8_t lsr;
	uint8_t bi; 
	uint8_t rbr;

	lsr = LPC_UART1->LSR & 255;
	bi = lsr & (1<<4);

	while(bi == 0){
		keypad_check(myaction);
		lsr = LPC_UART1->LSR & 255;
		bi = lsr & (1<<4);
		rbr = LPC_UART1->RBR;
	}
}

int main()
{
	pc.write("Starting...\n\r");

	uint8_t buf[5];
	char strbuf[32]; //[32]
	char cbuf[5];
	int read_success = 0;
	
	for(int i = 0; i<512; i++){
		packet[i] = 0;
	}

//	startup_screen();

	menu(0);

	while (1)
	{
		
		//Note: if dmx.read() does not read anything, it will return 0.
		//But it won't write 0 to the buffer. It will just be junk.

		//We should only use data that has successfully read.

		
		if(state == 0){
			keypad_check(myaction);
		}
		else if(state == 1){
			//return_home();
			//clear_display();
			//printstr("Real-time:");	
			
	
			while (state == 1){

				//ADD A PAUSE FUNCTIONALITY


//				pc.write("Loop begin\n\r");				
				return_home();
				shift_line();		
				wait_for_break();	
			
				grab_four_bytes(cbuf);
	
				sprintf(strbuf, "%3d %3d %3d %3d", cbuf[1], cbuf[2], cbuf[3], cbuf[4]); //decimal
				//sprintf(strbuf, "x%2.2x x%2.2x x%2.2x x%2.2x", cbuf[1], cbuf[2], cbuf[3], cbuf[4]); //hex
				printstr(strbuf);
			//	sprintf(strbuf, "%3d %3d %3d %3d\n\r", cbuf[1], cbuf[2], cbuf[3], cbuf[4]);	
			//	pc.write(strbuf);		
				total_packets++;	
			}
		}	
	
		if(state == 2){
			keypad_check(myaction);
		}
	//	else if(state == 2){
			//menu(2);		
	//		keypad_check(myaction);
			//while(state==2 || state==3){ //or state==3 ?

			//	keypad_check(myaction);

				//menu(2);
	
				//wait_for_break();

			//	while(state == 2){
			//		keypad_check(myaction);
			//	}
				
	//			if (state ==1){
	//				break;
	//			}

			//	state = 2;
			//	keypad_check(myaction);
				//add keypad_checks() throughout
				
			//	grab_packet();
			//	total_packets++;
			//	sprintf(strbuf, "Total packets: %d State: %d\n\r", total_packets, state);
			//	pc.write(strbuf);
//
//				print_packet();
		//	}
	//	}
	
		else if(state == 3){
			
			wait_for_break();
			grab_packet();
			total_packets++;
			sprintf(strbuf, "Total packets: %d State: %d\n\r", total_packets, state);
			pc.write(strbuf);
			print_packet();	
			state = 4;
			menu(4);
		}

		else if(state == 4){
			keypad_check(myaction);	
		}
	
		else if(state == 255){
			return 0;
		}	
	}
}

