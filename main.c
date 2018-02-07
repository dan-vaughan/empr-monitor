#include "lpc_types.h"
#include "lpc17xx_uart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "lib/pindef.h"
#include "lib/keypad.h"
#include "lib/display.h"

I2C i2c(SDA, SCL);        // sda, scl
Serial pc; // tx, rx
DMX dmx;

int state = 0;
char packet[512];

void myaction(int button) {

	printchar(labels[button]);
	//From state 0 to {1,2}	
	if (state == 0){
		if (labels[button] == 'A'){
			state = 1;
			clear_display();
			printstr("Real-time...");
			shift_line();
		}

		if (labels[button] == 'B'){
			state = 2;	
			clear_display();
			printstr("DMX Inspector...");
			shift_line();
		}
	}
	// From state 1 (real) to 2 (packet inspector)
	if (state == 1){
		if (labels[button] == 'B'){
			state = 2;
			return_home();
			clear_display();
			printstr("DMX Inspector...");
			shift_line();
		}
	}
	// From state 2 (packet inspector) to 1 (real-time)	
	if (state == 2){
		if (labels[button] == 'A'){
			state = 1;
			return_home();
			clear_display();
			printstr("Real-time...");
			shift_line();
		}
	}

}

void packets_to_lcd(char* buf)
{
//			sprintf(buf, "%#2.2x");	
//		if(strlen(buf) > 4){
			return_home();
			shift_line();
			printstr(buf);
//		}
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
 
	int count = 0;
	int read_success;
	uint8_t buf;
	char strbuf[10];
	while(1){
		if(count>20){
			pc.write("Terminating...\n\r");
			return;
		}

		read_success = dmx.read(&buf);

		if(read_success < 1){
			buf = 0;
			continue;
		}
		count++;
		sprintf(strbuf,"%d - %#2.2x\n\r",read_success, buf);
		pc.write(strbuf);
	
	}
}

void packet_detector(){

	//Reads, keeping track of byte and packet numbers
	//Prints whenever a packet is 'found' i.e. detects three 0x00 in a row
	
	uint8_t buf;
	char strbuf[24]; //[32]
	int count = 0;
	int packet_count = 0;
	int notch = 0;
	int read_success = 0;
	
	while(1){
		read_success = dmx.read(&buf);
		if(read_success < 1){
			buf = 0;
			continue;
		}
		if((buf == 0) && read_success == 1){
			if(notch == 2){
			sprintf(strbuf,"Packet %d detected at byte %d\n\r", packet_count,count);
				pc.write(strbuf);
				count = 0;
				notch = 0;
				packet_count++;
			}
			notch++;	
			continue;
		}
		
		count++;
	}

}

int main()
{
	pc.write("Starting...\n\r");
	
	uint8_t buf;
	char strbuf[24]; //[32]
	char* packet[512];	
	int count = 0;
	int packet_count = 0;
	int notch = 0;
	int read_success = 0;
	startup_screen();

	while (1)
	{
		
		//Note: if dmx.read() does not read anything, it will return 0.
		//But it won't write 0 to the buffer. It will just be junk.

		//We should only use data that has successfully read.

		//int reg = UART_GetIntId((LPC_UART_TypeDef *)LPC_UART1);
		

		packet_detector();

//		packet_begin();
//		return 0;

//		read_success = dmx.read(&buf);
//		sprintf(strbuf,"%d - %#2.2x\n\r", read_success, buf);
//		pc.write(strbuf);
//		continue;

	//	sprintf(strbuf, "%#2.2x\n\r");
//		sprintf(buf, "%d\n\r");
//		pc.write(buf);


		if(state == 0){
			keypad_check(myaction);
		}
		if(state == 1){
			keypad_check(myaction);
			count++;
			if (count > 10){
			//packets_to_lcd(strbuf);
				//strbuf[0] = buf[0];
				//strbuf[1] = buf[1];
				//strbuf[2] = buf[2];
				return_home();
				shift_line();
				printstr(strbuf);
				count = 0;
			}
		}

		if(state == 2){
			keypad_check(myaction);
		}
		
	}

}

