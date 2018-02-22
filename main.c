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

void dmx_inspector()
{



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

	//Reads and returns the next four bytes

	int count = 0;
	int read_success;
	uint8_t buf[5]; 
	//char cbuf[5];
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
		//return cbuf;
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
	char strbuf[24]; //[32]
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
	char* packet[512];	
	char charbuf[4] = {0xaa, 0xbb, 0xcc, 0xdd};
	char cbuf[5];
	int count = 0;
	int packet_count = 0;
	int notch = 0;
	int read_success = 0;
	int j = 0;

	char red[5] = {0xaa, 0x00, 0x00, 0xff, 0x00};
	char green[5] = {0x00, 0x00, 0xbb, 0x00, 0x00};
	char blue[5] = {0x00, 0xcc, 0x00, 0x00, 0x00};
	char full[5] = {0x00, 0xff, 0xff, 0xff, 0x00};
	char empty[5] = {0x00, 0x00, 0x00, 0x00, 0x00};

	char* sequence[5] = {red,green,blue,full,empty};

	startup_screen();

	while (1)
	{
		
		//Note: if dmx.read() does not read anything, it will return 0.
		//But it won't write 0 to the buffer. It will just be junk.

		//We should only use data that has successfully read.

		

			
	//	uint8_t byte = UART_ReceiveByte((LPC_UART_TypeDef *)LPC_UART1);
	
	//	if(byte != 0){
	//		count++;
	//		sprintf(strbuf,"byte: %#2.2x\n\r", byte);
	//		pc.write(strbuf);	
	//	}

	//	if(count > 14){
	//		return 0;
	//	}

	//	continue;	
	//	packet_detector();


//		packet_begin();
//		return 0;

//		dmx.send(sequence[j], 5);		
		
	//	dmx.write(sequence[j], 1);
		


/*		read_success = dmx.read(buf);
		if(read_success < 1){
			buf[0] = 0;
			buf[1] = 0;
			buf[2] = 0;
			buf[3] = 0;
			buf[4] = 0;
			continue;
		}

*/	
//		sprintf(strbuf,"%d - %#2.2x\n\r", read_success, buf);
//		for(int i = 0; i<5;i++){
//		sprintf(strbuf,"%d - %#2.2x - %#2.2x\n\r", i, sequence[j][i], buf[i]);
//		pc.write(strbuf);
//}	
//		j++;
//		continue;
//
	//	sprintf(strbuf, "%#2.2x\n\r");
//		sprintf(bu, "%d\n\r");
//		pc.write(buf);


		if(state == 0){
			keypad_check(myaction);
		}
		else if(state == 1){
			return_home();
			clear_display();
			printstr("Real-time:");	
			char real[8] = {0,0,0,0,0,0,0,0};
			uint8_t lsr;
			uint8_t bi; 
			uint8_t rbr;
			while (state == 1){

				pc.write("Loop begin\n\r");				

				count = 0;	
				lsr = LPC_UART1->LSR & 255;
				bi = lsr & (1<<4);
			
				while(bi == 0 || count > 0){
					keypad_check(myaction);
				//	pc.write("Checking for breaks...\n\r");
				//	sprintf(strbuf, "bi: %d, lsr: %d, count: %d\n\r", bi, lsr, count);
				//	pc.write(strbuf);	
					lsr = LPC_UART1->LSR & 255;
					bi = lsr & (1<<4);
					rbr = LPC_UART1->RBR;
				}
				rbr = LPC_UART1->RBR;
				count = 0;			

	
				//TRY WITH PACKET BEGIN HERE TO SEE START OF PACKETS {2,3...}
				grab_four_bytes(cbuf);
				j++;
	
				return_home();
				shift_line();			
				sprintf(strbuf, "%3d %3d %3d %3d", cbuf[1], cbuf[2], cbuf[3], cbuf[4]);
				printstr(strbuf);
				sprintf(strbuf, "%3d %3d %3d %3d\n\r", cbuf[1], cbuf[2], cbuf[3], cbuf[4]);	
				pc.write(strbuf);		
					
				//if(j==2){return 0;}
				while (count < 512){
				//pc.write("Break out\n\r");

// WE NEED TO: FROM COUNT 0
// STORE ALL BYTES READ INTO REAL[8]
// CUT ALL 0x00s FROM START OF REAL

				read_success = dmx.read(buf);
				count += read_success;
			
				if(count<4){
					real[0] = buf[1];
				}				
				
				/*if(count>4 && count < 8){

					for (int i=0; i <count; i++){

					}

					return_home();
					shift_line();
					real[1] = buf[0];
					real[2] = buf[1];
					real[3] = buf[2];
					sprintf(strbuf, "%3d %3d %3d %3d", real[0], real[1], real[2], real[3]);
					printstr(strbuf);
					sprintf(strbuf, "%3d %3d %3d %3d\n\r", real[0], real[1], real[2], real[3]);	
					pc.write(strbuf);		
				}	
	
*/
				//pc.write("Read success.\n\r");	
				if(count > 512){ count = 0;};	
				//sprintf(strbuf,"Count: %d\n\r", count);		
				//pc.write(strbuf);	
			
/*				if(count == 4){
					return_home();
					shift_line();
					//sprintf(strbuf, "%#2.2x %#2.2x %#2.2x %#2.2x", buf[0],buf[1],buf[2],buf[3]);
					sprintf(strbuf, "%3d %3d %3d %3d", buf[0], buf[1], buf[2], buf[3]);
					printstr(strbuf);
					pc.write(strbuf);
				}
*/
	
		/*		if(read_success==1){

					while(read_success < 2){
						read_success = dmx.read(buf);
						count += read_success;
						if (buf[0] != 0)
						real[0] = buf[0];
					}				
					
						real[1] = buf[0];
						real[2] = buf[1];
						real[3] = buf[2];	

					return_home();
					shift_line();
					sprintf(strbuf, "%#2.2x %#2.2x %#2.2x %#2.2x\n\r", real[0],real[1],real[2],real[3]);
					printstr(strbuf);
					pc.write(strbuf);
				}

				if(count==3){
					pc.write("Count was 3.\n\r");
					real[0] = buf[0];
					while (read_success != 4){
					pc.write("Trying...\n\r");
					read_success = dmx.read(buf);
					}
					count+= read_success;
					if(read_success > 4){
						real[1] = buf[0];
						real[2] = buf[1];
						real[3] = buf[2];
					}
					return_home();
					shift_line();
					sprintf(strbuf, "%#2.2x %#2.2x %#2.2x %#2.2x\n\r", real[0],real[1],real[2],real[4]);
					printstr(strbuf);
					pc.write(strbuf);
				}
*/				if(count>4 && count<512){
				//	pc.write("reading...\n\r");	
					read_success = dmx.read(buf);
					count += read_success;

				}			
				//pc.write("read over.\n\r");
			}
				count = 0;
				pc.write("count set to zero\n\r");
		}
}	

		else if(state == 2){
			keypad_check(myaction);
			clear_display();
			printstr("DMX Inspector -");
			shift_line();
			printstr("Byte number:   ");
			

		}
	
		if(state == 255){
			return 0;
		}	
	}

}

void break_primitive()
{

		char strbuf[32];

		while(1){
		uint8_t lsr= LPC_UART1->LSR & 255;// & (1<<4);
		uint8_t ier = LPC_UART1->IER &= (1<<2);;
		uint32_t iir = LPC_UART1->IIR & 14;
		int rbr2 = LPC_UART1->RBR;
		sprintf(strbuf, "BreakInt: %d IER: %d IIR: %d\n\r", lsr, ier, iir);
		pc.write(strbuf);

		if(lsr != 96){ 
			pc.write("BREAK DETECTED\n\r");
			uint32_t iir = LPC_UART1->IIR & 14;
			sprintf(strbuf, "IIR: %d\n\r", iir);
			pc.write(strbuf);
			//NVIC_ClearPendingIRQ(UART1_IRQn);
			LPC_UART1->IER |= (1<<2);
			int rbr = LPC_UART1->RBR;
			//return 0;
		}


	}
}
