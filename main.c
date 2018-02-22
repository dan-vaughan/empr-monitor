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
char packet[512];

int interrupt = 0;

void packet_begin();
void packet_detector();

void myaction(int button) {

	printchar(labels[button]);

	if(labels[button] == '7'){
		pc.write("Packet begin...\n\r");
		packet_begin();
		state = 255;
		return;
	}
	if(labels[button] == '8'){
		packet_detector();
	}
	
	//From state 0 to {1,2}	
	if (state == 0){
		if (labels[button] == 'A'){
			state = 1;
			clear_display();
			printstr("Real-time...");
			shift_line();
			pc.write("Real mode selected.\n\r");
		}

		if (labels[button] == 'B'){
			state = 2;	
			clear_display();
			printstr("DMX Inspector -");
			shift_line();
			pc.write("DMX mode selected.\n\r");
			printstr("Byte number:   ");
		}
	}
	// From state 1 (real) to 2 (packet inspector)
	if (state == 1){
		if (labels[button] == 'B'){
			state = 2;
			pc.write("DMX mode selected.\n\r");
			return_home();
			clear_display();
			printstr("DMX Inspector -");
			shift_line();
			printstr("Byte number:   ");
		}
	}
	// From state 2 (packet inspector) to 1 (real-time)	
	if (state == 2){
		if (labels[button] == 'A'){
			state = 1;
			pc.write("Real mode selected.\n\r");
			return_home();
			clear_display();
			printstr("Real-time...");
			shift_line();
		}
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

void print_packet(char* packet)
{	
	char str_buf[20];

	for(int i=0;i<512;i++){
		sprintf(str_buf, "Slot %d: %#2.2X\n\r", i+1, packet[i]);
		pc.write(str_buf);
	}	
}

void print_buflen(int n){

	//Test function 
	//Prints the next n bytes received

	uint8_t buf[n];
	uint32_t success;
	char strbuf[18];
		success = dmx.read(buf);
		for (int i =0; i<n;i++){ 
		sprintf(strbuf,"success: %d - %d\n\r", success, buf[i]);	
		pc.write(strbuf);
		}


};

void packet_begin(){

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

void grab_four_bytes(char* cbuf){

	//Reads and writes the next four bytes to cbuf

	int count = 0;
	int read_success;
	uint8_t buf[5]; 
	char strbuf[10];
	int j=0;
	while(1){

		if(count>6){
			pc.write("Terminating...\n\r");
			return;
		}

		read_success = dmx.read(buf);

		for(int i = 0; i<read_success; i++){
			if(i==0){
				sprintf(strbuf,"%d - %#2.2x\n\r",read_success, buf[i]);
				pc.write(strbuf);
				cbuf[j] = buf[i];
			}else{
				sprintf(strbuf,"  - %#2.2x\n\r",buf[i]);
				pc.write(strbuf);
				cbuf[j] = buf[i];
			}
			j++;
			count++;
		}
	}
}


void grab_packet(char* packet){

	//Reads and writes the next 512 bytes into packet char array

	int count = 0;
	int read_success;
	uint8_t buf[5]; 
	char strbuf[10];
	int j=0;
	while(1){

		if(count>507){
//			pc.write("Terminating...\n\r");
			return;
		}

		read_success = dmx.read(buf);

		for(int i = 0; i<read_success; i++){
			if(i==0){
//				sprintf(strbuf,"%d- %d - %#2.2x\n\r", j, read_success, buf[i]);
//				pc.write(strbuf);
				packet[j] = buf[i];
			}else{
//				sprintf(strbuf,"     - %#2.2x\n\r",buf[i]);
//				pc.write(strbuf);
				packet[j] = buf[i];
			}
			j++;
			count++;
		}
	}
}

int check_zeroes(uint8_t* buf, int len){

	for(int i = 0;i<len;i++){

		if(buf[i] != 0){
			return 0;
		}
	}
	return 1;
}

void packet_detector(){

	//Reads, keeping track of byte and packet numbers
	//Prints whenever a packet is 'found' i.e. detects three 0x00 in a row
	
	uint8_t buf[5];
	char strbuf[24];
	int count = 0;
	int packet_count = 0;
	int notch = 0;
	int read_success = 0;
	
	while(1){
		read_success = dmx.read(buf);
		if(read_success < 1){
			buf[0] = 0;
			buf[1] = 0;
			buf[2] = 0;
			buf[3] = 0;
			buf[4] = 0;
			continue;
		}
		if((buf[0] == 0) && read_success == 1){
				notch++;
				pc.write("notch++\n\r");
		}
		if(notch > 1){
				if(buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] > 250){
				sprintf(strbuf,"Packet %d detected at byte %d\n\r", packet_count,count);
				pc.write(strbuf);
				count = 0;
				notch = 0;
				packet_count++;
				continue;
				}
			}
		count++;
	}
}

int main()
{
	pc.write("Starting...\n\r");

	uint8_t buf[5];
	char strbuf[32]; //[32]
	char packet[512];	
	char charbuf[4] = {0xaa, 0xbb, 0xcc, 0xdd};
	char cbuf[5];
	int read_success = 0;
	int total_packets = 0;
	uint8_t lsr;
	uint8_t bi; 
	uint8_t rbr;
	
startup_screen();

	while (1)
	{
		
		//Note: if dmx.read() does not read anything, it will return 0.
		//But it won't write 0 to the buffer. It will just be junk.

		//We should only use data that has successfully read.

		
		if(state == 0){
			keypad_check(myaction);
		}
		else if(state == 1){
			return_home();
			clear_display();
			printstr("Real-time:");	

			while (state == 1){

				pc.write("Loop begin\n\r");				

				lsr = LPC_UART1->LSR & 255;
				bi = lsr & (1<<4);
			
				while(bi == 0){
					keypad_check(myaction);
					lsr = LPC_UART1->LSR & 255;
					bi = lsr & (1<<4);
					rbr = LPC_UART1->RBR;
				}
				
				grab_four_bytes(cbuf);
	
				return_home();
				shift_line();			
				sprintf(strbuf, "%3d %3d %3d %3d", cbuf[1], cbuf[2], cbuf[3], cbuf[4]); //decimal
				//sprintf(strbuf, "x%2.2x x%2.2x x%2.2x x%2.2x", cbuf[1], cbuf[2], cbuf[3], cbuf[4]); //hex
				printstr(strbuf);
				sprintf(strbuf, "%3d %3d %3d %3d\n\r", cbuf[1], cbuf[2], cbuf[3], cbuf[4]);	
				pc.write(strbuf);		
				total_packets++;	
	}
}	

		else if(state == 2){
			keypad_check(myaction);
			clear_display();
			printstr("DMX Inspector -");
			shift_line();
			printstr("Byte number:   ");

			while(state==2){

				clear_display();
				printstr("DMX Inspector - ");
				shift_line();
				printstr("Byte number:   ");	

				lsr = LPC_UART1->LSR & 255;
				bi = lsr & (1<<4);

				while(bi == 0){
					keypad_check(myaction);
					lsr = LPC_UART1->LSR & 255;
					bi = lsr & (1<<4);
					rbr = LPC_UART1->RBR;
				}
				//add keypad_checks() throughout
				grab_packet(packet);
				total_packets++;
				sprintf(strbuf, "Packets: %d\n\r", total_packets);
				pc.write(strbuf);
			}
		}
	
		else if(state == 255){
			return 0;
		}	
	}
}

