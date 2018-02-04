#include "lpc_types.h"
#include <stdio.h>
#include <string.h>

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
		if (labels[button] == '0'){
			state = 1;
			clear_display();
			printstr("Real-time...");
			shift_line();
		}

		if (labels[button] == '1'){
			state = 2;	
			clear_display();
			printstr("DMX Inspector...");
			shift_line();
		}
	}
	// From state 1 (real) to 2 (packet inspector)
	if (state == 1){
		if (labels[button] == '1'){
			state = 2;
			return_home();
			clear_display();
			printstr("DMX Inspector...");
			shift_line();
		}
	}
	// From state 2 (packet inspector) to 1 (real-time)	
	if (state == 2){
		if (labels[button] == '0'){
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
			sprintf(buf, "%#2.2x");	
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
	printstr("0: Real-time");
	shift_line();
	printstr("1: DMX Inspector");
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

int main()
{
	pc.write("Starting...\n\r");
	
	char buf[32];
	char strbuf[32];
	char packet[512];	
	int count = 0;

	startup_screen();

	while (1)
	{
		dmx.read(buf);
	//	sprintf(strbuf, "%#2.2x\n\r");	
	sprintf(strbuf, "%#x\n\r");	

		// Here we filter out malformed packets.
		// But probably also filters out start codes and breaks...	
//		if(strlen(buf) > 4){
			pc.write(strbuf);
//		}

		if(state == 0){
			keypad_check(myaction);
		}
		if(state == 1){
			count++;
			if (count > 5){
			packets_to_lcd(buf);
			count = 0;
			}
		}
		
	}

}

