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
int total_packets = 0;
int packet_index = 0;
int triggermod = 0;
int trigger_slot = 0;

char packet[512];	

char break_code[3] = {0x42,0x84,0xc6};

void packet_begin();
void packet_viewer(int action);
void wait_for_break();

void myaction(int button)
{
	//printchar(labels[button]);

 	//From state 0 to {1,2}	
	if (state == 0){
		if (labels[button] == 'A'){
			state = 1;
			menu(1);
		//	pc.write("Real mode selected.\n\r");
		}

		if (labels[button] == 'B'){
			state = 2;	
			menu(2);
		//	pc.write("DMX mode selected.\n\r");
		}
       if (labels[button] == '8'){
            state = 8;
            menu(8); 
            return;
        }
	}
	
	// From state 1 (real) to 2 (packet inspector)
	if (state == 1){
		if ((labels[button] == 'B') || (labels[button] == 'C') || (labels[button] == 'D')){
			state = 2;
			menu(2);
		//	pc.write("DMX mode selected.\n\r");
		}

		if(labels[button] == '0'){
			state = 99;
			menu(99);
		//	pc.write("Real mode paused.\n\r");
		}
	}
	
	else if (state == 99){
		if (labels[button] == '0'){
			state = 1;
			menu(1);
		}
	}


	// From state 2 (packet inspector) to 1 (real-time)	
	if (state == 2){
		if (labels[button] == 'A'){
			state = 1;
			menu(1);
		//	pc.write("Real mode selected.\n\r");
		}

		if (labels[button] == 'C'){
			state = 3;
			menu(3);
		//	pc.write("Capturing next packet...\n\r");
		}
		
		if (labels[button] == 'D'){
			state = 5;
			menu(5);
		//	pc.write("Trigger mode selected.\n\r");
		}
	}
    if (state == 5){
        if (labels[button] == 'A'){
            //Capture three digit number here, set it to trigger_slot
            menu(6);
        }
        if (labels[button] == 'B'){
            state = 6;
            menu(7);
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

		if(labels[button] == '*'){

			if((packet_index - 4) < 0){
				packet_index = 512 - packet_index - 4;
			}
		else{
				packet_index = (packet_index - 4) % 512;	
			}
			menu(4);
			packet_viewer(1);
		}	

		if(labels[button] == '#'){
			packet_index = (packet_index + 4) % 512;
			menu(4);
			packet_viewer(2);
		}

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

        if(state == 8 || state == 9) 
            if (labels[button] == '0'){
            state = 0;
            menu(0);
            }
            if (labels [button] == '8'){
                state = 8;
            }        
            if (labels [button] == '9'){
                state = 9;
            }
}

void menu(int screen)
{

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
		return_home();
		printstr("Real-time:    B");
		putcustom(0x20);
		shift_line();
	}

	if (screen == 99){
		return_home();
		printstr("Real-time: !! B");
		putcustom(0x20);
	}
	
	if (screen == 2){
		clear_display();
		return_home();				//FIGURE OUT WHICH WAY ROUND YOU NEED THESE
		clear_display();
		printstr("C: Capture Next");
		shift_line();
		printstr("D: Set Trigger");
	}

	if (screen == 3){
		return_home();
		clear_display();
		printstr("Capturing...");
	}

	if (screen == 4){
		clear_display();
		return_home();
    }

	if (screen == 5){
		clear_display();
		return_home();
		printstr("A: Set Trigger");
        shift_line();
        printstr("B: Capture");
	}

    if (screen == 6){
        clear_display();
        return_home();
        printstr("Slot:");
    }
    
    if (screen == 7){
        clear_display();
        return_home();
        printstr("Waiting...");
    }

    if (screen == 8){
        clear_display();
        return_home();
        printstr("IC2 Mode.");    
    }
}

void packet_viewer(int action)
{
		/*
		 	< 0x10 | ^ 0x12 | > 0x20 
		*/
	
		char strbuf[18];

		putcustom(0x10);
		sprintf(strbuf, "%03d--x--x--%03d", packet_index,  packet_index+3); 
		printstr(strbuf);
		putcustom(0x20);
		shift_line();						//BELOW LINE contains a bodge (adding extra 1 to index to mitigate off by one)	
		sprintf(strbuf,"  %2x %2x %2x  %2x", packet[packet_index+1], packet[packet_index+2], packet[packet_index+3], packet[packet_index+4]);
		printstr(strbuf);
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
		//	pc.write("Terminating...\n\r");
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
		//		pc.write(strbuf);
			}else{
				sprintf(strbuf,"  - %#2.2x\n\r",cbuf[i]);
		//		pc.write(strbuf);
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
    int total = 0;
	while(1){

		if(j>512){
		//	pc.write("\n\rTerminating...\n\r");
			//pc.write("\n\r\n\r");
			return;
		}

		read_success = dmx.read(buf);
		if(read_success == 0){ continue;}
        
        //if (j<128){
//		    UART_Send((LPC_UART_TypeDef *)LPC_UART0,(uint8_t *)buf,read_success,NONE_BLOCKING);
       
        // }
//		pc.write((char*)buf);

		for(int i = 0; i<read_success; i++){	
			
//			if (packet[j+i] != buf[i]){				//CODE DETECTS CHANGES BETWEEN NEW AND OLD PACKET
//				sprintf(strbuf,"%d - %#2.2x \n\r", j+i, buf[i]);
//				pc.write(strbuf);
//			}

//			if(i==0){
//				sprintf(strbuf,"%d- %d - %#2.2x\n\r", j, read_success, buf[i]);
//				pc.write(strbuf);
				packet[j+i] = buf[i];					//THIS FILLS ENTIRE PACKET
//			}else{
	//			sprintf(strbuf,"     - %#2.2x\n\r",buf[i]);

//				sprintf(strbuf,"%2.2x",buf[i]);
//				pc.write(strbuf);
//				packet[j] = buf[i]; 
//			}
		}
		//		sprintf(strbuf,"%2.2x",buf);
		//		pc.write(strbuf);

			j += read_success;
	}
}

void capture_packet_trigger()
{
	//Reads and writes the next 512 bytes into packet char array

	int read_success;
	uint8_t buf[5]; 
    char temp_packet[512];
	int j=0;
    int found = 0;

	while(1){

        if(found == 1){
            for(int i = 0; i<512; i++){
                    packet[i] = temp_packet[i];
                }

            return;
        }

        wait_for_break();

        while(1){

    		if(j>511){
                if(temp_packet[trigger_slot] != packet[trigger_slot]){
                    found = 1;
                }
                j = 0;
	    	    break;
            }

    		read_success = dmx.read(buf);
	    	if(read_success == 0){ continue;}
        
		    for(int i = 0; i<read_success; i++){	
				temp_packet[j+i] = buf[i];
		}
			j += read_success;
	    }
    }
}

void print_packet()
{

	char strbuf[128];
	char strbuf2[5];	
	char message[20];

//	strbuf[0] = '\0';
//	strbuf2[0] = '\0';

//	sprintf(message,"Packet no. %d:\n\r\n\r", total_packets);
//	pc.write(message);

	for(int i=0; i<16; i++){
		for (int j=0; j<32;j++){
			sprintf(strbuf2, "%2.2x",packet[((i*16)+j)]);
			pc.write(strbuf2);
            //strcat(strbuf, strbuf2);
	//		pc.write(packet[(i*16)+j]);
		}
	//	pc.write(strbuf);
	//	strbuf[0] = '\0';
	}
//	pc.write("\n\r");
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

//	pc.write("Starting...\n\r");

	uint8_t buf[5];
	char strbuf[80];
    char strbuf2[10];
    int limit = 17;
	int read_success = 0;
    char newline[2] = {'\n','\r'};
	
    for(int i = 0; i<512; i++){
		packet[i] = 0;
	}

	menu(0);

	while (1)
	{
		
//		sprintf(strbuf,"state: %d\n\r", state);
//		pc.write(strbuf);
	
		if(state == 0){
        
            //Startup state

			keypad_check(myaction);
		}

		else if(state == 1){
	
			while (state == 1){

                //Realtime mode, captures prints first 4 bytes of packet to LCD.
                //Also is where I put code to test other stuff.

				keypad_check(myaction);
				return_home();
				shift_line();		
				wait_for_break();	
//				new_grab();
				pc.write(break_code);
				grab_packet();
               // print_packet();
	            triggermod++;
    	//		pc.write(newline);
	//			grab_four_bytes(cbuf);
//				pc.write(packet);
                //UART_Receive((LPC_UART_TypeDef *)LPC_UART1,(uint8_t*)packet,512, NONE_BLOCKING);
				for(int i=1; i<5;i++){
					sprintf(strbuf, "%3d ",packet[i]);
				//	pc.write(strbuf);
					printstr(strbuf);
				}
               // if (triggermod > 3){
                 //   triggermod = 0;	
                    packet[256] = '\0';
//                    UART_Send((LPC_UART_TypeDef *)LPC_UART0, (uint8_t *)packet, 16, NONE_BLOCKING);

                    for(int k=0; k<512; k++){
                        sprintf(strbuf, "%d", packet[k]);
                        pc.write(strbuf);
                    }

                 //   pc.write(packet);
                //    pc.write(strbuf); 
 //                   print_packet();
                    //for (int k=0; k<7; k++){
                    //UART_Send((LPC_UART_TypeDef *)LPC_UART0, (uint8_t *)packet[k*64], 64, NONE_BLOCKING);
                   // }
    		        //UART_Send((LPC_UART_TypeDef *)LPC_UART0,(uint8_t *)packet,512,NONE_BLOCKING);
                    //print_packet();   
                   // }
//                    pc.write(packet);
 
                pc.write(newline);
                strbuf[0] = '\0';
                strbuf2[0] = '\0';
				total_packets++;	
				keypad_check(myaction);
			}
		}	

		if(state == 2){

            //Menu state

			keypad_check(myaction);
		}

		while(state == 99){

            //Pause state for realtime mode

			keypad_check(myaction);
		}
	
		if(state == 3){
		
            //Capture full packet state, transitions to packet viewer (state 4)
	
			wait_for_break();
			pc.write(break_code);
			grab_packet();
			pc.write(packet);
			total_packets++;
		//	sprintf(strbuf, "Total packets: %d State: %d\n\r", total_packets, state);
		//	pc.write(strbuf);
		//	print_packet();	
			state = 4;
			packet_index = 0;
			menu(4);
			packet_viewer(0);
		}

		if(state == 4){

            //Packet Inspector state

			while(state == 4){
				keypad_check(myaction);
			}
		}
	
        if(state == 6){

            //Waiting for packet change in trigger slot state
            
            capture_packet_trigger();
            state = 4;
            packet_viewer(0);
        }

        while (state == 8 || state == 9){
   
            if (state == 9){
                limit = 512;
            }
            else{ limit = 17;}
 
            // IC2 GUI output state            

			wait_for_break();	
			grab_packet();

            strbuf[0] = '\0';
    
            //Increase k to 512 for Snapshot Mode        
            for(int k = 0; k<limit; k++){
                sprintf(strbuf2,"%d-",packet[k]);
                strcat(strbuf, strbuf2);
            }

            strcat(strbuf, newline);
            pc.write(strbuf); 
            strbuf[0] = '\0';
            strbuf2[0] = '\0';
            keypad_check(myaction);

        }
	}
}
