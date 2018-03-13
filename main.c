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
int packet_index = 1;
int trigger_slot = 0;
int packetmod = 0;

int inputmod = 0;
int input_hun = 0;
int input_ten = 0;

char packet[512];	

char strbuf[20];

char break_code[3] = {0x42,0x84,0xc6};

void packet_viewer(int action);
void wait_for_break();

void myaction(int button)
{

 	//From state 0 to {1,2}	
	if (state == 0){
		if (labels[button] == 'A'){
			state = 1;
			menu(1);
		}

		if (labels[button] == 'B'){
			state = 2;	
			menu(2);
		}
        if (labels[button] == '*'){
            state = 8;
            menu(8); 
            return;
        }

        if(labels[button] == '#'){
            state = 12;
            menu(12);
            return;
        }
	}
	
	// From state 1 (real) to 2 (packet inspector)
	if (state == 1){
		if ((labels[button] == 'B') || (labels[button] == 'C') || (labels[button] == 'D')){
			state = 2;
			menu(2);
		}

		if(labels[button] == '0'){
			state = 99;
			menu(99);
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
		}

		if (labels[button] == 'C'){
			state = 3;
			menu(3);
		}
		
		if (labels[button] == 'D'){
			state = 5;
			menu(5);
		}

        if (labels[button] == '0'){
            state = 0;
            menu(0);
        }
	}

    if (state == 5){

            if (labels[button] == 'A'){
                state = 11;
                menu(6);

            return;
            }
            if (labels[button] == 'B'){
                state = 6;
                menu(7);
            }
    }

    if (state == 11){

        printchar(labels[button]);

         if((button < 3) || (button == 4) || (button == 5) || (button == 6) || (button == 8)
               || (button == 9) || (button == 10) || (button == 13))
            {

                char s = labels[button];

                int in = atoi(&s);

                if (inputmod == 0){
                    input_hun = in * 100;
                }
                if (inputmod == 1){
                    input_ten = in * 10;
                }

                if (inputmod == 2){
                    int slot = input_hun + input_ten + in; 
                    if ((slot < 0) || (slot > 512)){
                        return_home();
                        clear_display();
                        printstr("Error.");
                        shift_line();
                        printstr("Must be 0 - 512.");
                        delay(7000000);
                        inputmod = (inputmod+1) % 3;
                        menu(6);
                        return;
                    }
                    trigger_slot = slot;
                    return_home();
                    clear_display();
                    printstr("Slot updated!");
                    shift_line();
                    printstr("   ---");
                    sprintf(strbuf, "%d", trigger_slot);
                    printstr(strbuf);
                    printstr("---");
                    sprintf(strbuf, "%d\n\r", trigger_slot);
                    delay(7000000);
                    state = 5;
                    menu(5);
                }
        
                inputmod = (inputmod+1) % 3;
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

        if(state == 8 || state == 9){ 
            if (labels[button] == '0'){
                state = 0;
                menu(0);
            }
            if (labels [button] == '8'){
                state = 8;
                menu(8);
            }        
            if (labels [button] == '9'){
                state = 9;
                menu(9);
            }
        }

    if (state == 12 || state == 13){

       if(labels[button] == '#'){
            if(state == 12){  
                state = 13;
                 menu(13);
            }
            else if(state == 13){
                state = 12;
                menu(12);
            }
        }

        if(labels[button] == '0'){
            state = 0;
            menu(0);
        }
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
		return_home();
		clear_display();
		printstr("C: Capture Next");
		shift_line();
		printstr("D: Trigger Mode");
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
		return_home();
		clear_display();
		printstr("A: Set Trigger");
        shift_line();
        printstr("B: Capture");
	}

    if (screen == 6){
        return_home();
        clear_display();
        printstr("Slot:");
    }
    
    if (screen == 7){
        return_home();
        clear_display();
        printstr("Waiting...");
    }

    if (screen == 8){
        clear_display();
        return_home();
        printstr("IC2 Mode.");
        shift_line();
        printstr("(Real-time)");    
    }
    
    if (screen == 9){
        clear_display();
        return_home();
        printstr("IC2 Mode");
        shift_line();
        printstr("(Snapshot)");
    }

    if (screen == 12){
        return_home();
        clear_display();
        printstr("PC Mode.");
        shift_line();
        printstr("(Full)");
    }

    if (screen == 13){
        return_home();
        clear_display();
        printstr("PC Mode.");
        shift_line();
        printstr("(Raw)");
    }
}

void packet_viewer(int action)
{
		/*
		 	< 0x10 | ^ 0x12 | > 0x20 
		*/
	
		char strbuf[18];

        return_home();
        clear_display();

		putcustom(0x10);
		sprintf(strbuf, "%03d--x--x--%03d", packet_index,  packet_index+3); 
		printstr(strbuf);
		putcustom(0x20);
		shift_line();						//BELOW LINE contains a bodge (adding extra 1 to index to mitigate off by one)	
		sprintf(strbuf,"  %2x %2x %2x  %2x", packet[packet_index+1], packet[packet_index+2], packet[packet_index+3], packet[packet_index+4]);
		printstr(strbuf);
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
			return;
		}

		read_success = dmx.read(buf);
		if(read_success == 0){ continue;}
        
		for(int i = 0; i<read_success; i++){	
			
				packet[j+i] = buf[i];					//THIS FILLS ENTIRE PACKET
		 }

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

    char strbuf[40];

	while(1){
        if(temp_packet[trigger_slot] != packet[trigger_slot]){
            for(int i = 0; i<512; i++){
                    packet[i] = temp_packet[i];
                }

            return;
        }

        wait_for_break();

        while(1){
    		if(j>511){
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

	uint8_t buf[5];
	char strbuf[80];
    char strbuf2[10];
    char mstrstrbuf[600];
    int IC2limit = 17;
	int read_success = 0;
    char newline[2] = {'\n','\r'};
	
    for(int i = 0; i<512; i++){
		packet[i] = 0;
	}

	menu(0);

	while (1)
	{
		
	
		if(state == 0){
        
            //Startup state

			keypad_check(myaction);
		}

		else if(state == 1){
	
			while (state == 1){

                //Realtime mode, captures prints first 4 bytes of packet to LCD.

				keypad_check(myaction);
				return_home();
				shift_line();		
				wait_for_break();	
				grab_packet();
				for(int i=1; i<5;i++){
					sprintf(strbuf, "%3d ",packet[i]);
					printstr(strbuf);
				}
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

        while(state == 5 || state == 11){
            keypad_check(myaction);
        }
	
        if(state == 6){

            //Waiting for packet change in trigger slot state
            pc.write("Attempting packet capture.\n\r"); 
            capture_packet_trigger();
            pc.write("Packet captured!\n\r");
            state = 4;
            packet_index = 1;
            
            packet_viewer(0);
        }

        while (state == 12 || state == 13){
        
            // State for PC monitor

			wait_for_break();	
           	pc.write(break_code);
			
            grab_packet();
            

            char temp[16];
            for (int k = 0; k<32; k++){
                for (int m = 0; m<16; m++){
                    temp[m] = packet[m];
                }
                int success = UART_Send((LPC_UART_TypeDef *)LPC_UART0, (uint8_t *)temp, 16, BLOCKING);
            }
            
            strbuf[0] = '\0';
            strbuf2[0] = '\0';
            keypad_check(myaction);
            packetmod++;
        }

        while (state == 8 || state == 9){
   
            // IC2 GUI output state            
            
            if (state == 9){
                IC2limit = 500;
            }
            else{ IC2limit = 17;}

			wait_for_break();	
			grab_packet();

            strbuf[0] = '\0';
    
            //Increase k to 512 for Snapshot Mode        
            for(int k = 0; k<IC2limit; k++){
                sprintf(strbuf2,"%d-",packet[k]);
                pc.write(strbuf2);
            }

            strcat(mstrstrbuf, newline);
            pc.write(mstrstrbuf); 
            mstrstrbuf[0] = '\0';
            strbuf2[0] = '\0';
            keypad_check(myaction);

        }
	}
}
