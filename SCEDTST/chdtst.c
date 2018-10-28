/* game.c - xmain, prntr */

#include <conf.h>
#include <kernel.h>
#include <io.h>
#include <bios.h>

extern SYSCALL  sleept(int);
extern struct intmap far *sys_imp;
/*------------------------------------------------------------------------*/
#include "process1.h"
#include "process2.h"
/*------------------------------------------------------------------------*/
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>

volatile int moved_right_flag =0;
volatile char c;
volatile char scanCode = 'p';			// non_importance set value
volatile int latch = 1193;				// the higher to lower freq!
volatile int flag;

int location =0;
int location_row =0;
int color_segment =0x0B800;     //0B800h Segment address of memory on color adapter
int Resolution_Counter=0;

void interrupt (*int8save)(void);
void interrupt (*int9save)(void);
void Set_Cursor(int new_location);
void Paint_Arrow(int location);
void Paint_in_blue();

void interrupt newint8(void)
{
	asm{
		mov al,20h
		out 20h,al
	}
	asm{	// same as above, but with call to the original ISR
		PUSHF
		CALL DWORD PTR int8save
	}
	Resolution_Counter++;	// increase every 'tick', with latch is set to 1193
							// we increase it 1000 times in a 1 sec
} // newint8(void)

void interrupt newint9(void)
{
	asm{
		PUSHF
		CALL DWORD PTR int9save
	}
	asm{// here were only checking for input (not taking it yet from input buffer)
		CLI			// critial scanCode:
		PUSH AX
		MOV AH,1
		INT 16h
		PUSHF
		POP AX
		AND AX,64  // MASK to save only ZF bit from FlagsReg
		MOV flag,AX
		POP AX
		STI
	} /* check if button was presed */

	if( flag==0 )	// if ZF is DOWN, its mean that theres and char in input-buffer
	{
		asm{
			CLI
			PUSH AX
			MOV AH,0
			INT 16h
			MOV c,AL
			mov scanCode,ah
			POP AX
			STI
		}
	}
	

}// newint9(void)

void Set_Cursor(int new_location){
	int temp_location = new_location;
	asm{
		
		mov BX, temp_location
		MOV              DX,3D4H  // Point to 3D4H - 3D5H port pair
		MOV              AL,14    // Address of cursor register pos high byte
		MOV              AH,BH    // Get desired value of cursor pos high byte
		OUT              DX,AX    // Port(3D4h) = 14, Port(3D5h) = Value of BH
		//
		MOV              AL,15    // Address of cursor register pos low byte
		MOV              AH,BL    // Get desired value pf cursor pos low byte
		OUT              DX,AX    // Port(3D4h) = 15, Port(3D5h) = Value of BL
		
	}
}
void Set_Screen()
{
	asm{
	//Part1 : Initialize the display adapter
	//
		MOV              AH,0          // Select function = 'Set mode'
		MOV              AL,1          // 40 by 25 color image
		INT              10h           // Adapter initialized. Page 0 displayed
	// Part2 : Set cursor to be max
		MOV             DX,3D4h  		// Point TO 3D4h - 3D5h port pair
		MOV             AX,000Ah 		// Cursor start address (0Ah) - Value 0 (00h)
		OUT             DX,AX    		// Port(3D4h) = 0Ah, Port(3D5h) = 01h
		MOV             AX,0F0Bh 		// Cursor end address - Value 15 (0Eh)
		OUT             DX,AX    		// Port(3D4h) = 0Bh, Port(3D5h) = 0Eh	
	}
}
void Paint_in_blue()
{
	int cx;	
	int ES_temp;
	int SI_temp =0;
	//count =0;

	for (cx = 0; cx < 1000 ;cx++){// Loop all over screen pixles
		asm{
		
			MOV             AX,color_segment   	// Segment address of memory on color adapter
			MOV             ES,AX         		// 
			mov				SI,SI_temp			
			mov				BYTE PTR ES:[SI+1],0011110b 
			mov				BYTE PTR ES:[SI],0b /*
														set all attributes color = blue 
														(paint all screed to be blue
														 - turning on background blue bit
														 - turning on foreground bits to make yellow collor.
														 - turning all aschi's to 0
														*/
			INC				SI_temp
			INC				SI_temp
		
		}
		//count++;	// for debug
	}
}

void Paint_Arrow(int location)
{
	int temp_SI = location*2;
	int temp_CX;
	
	Paint_in_blue();						// repaint all screen in blue before painting new Arrow location
	asm{
		
		MOV             AX,color_segment   	// Segment address of memory on color adapter
		MOV             ES,AX         		
		mov				SI,temp_SI
		mov				BYTE PTR ES:[SI],00001110b  // attributes: yellow foreground
													// because cursor is FULL then every thing  will be painted in YELLOW
		mov				BYTE PTR ES:[SI],' '	
	}
	
	if(location%40 - 1 > 0){
		asm{
			mov				SI,temp_SI
			mov				BYTE PTR ES:[SI-1],100100b 	// attributes: Green background, Red foreground
			mov				BYTE PTR ES:[SI-2],'>'		
		}
	}
	if(location%40 - 2 >= 0 ){
		asm{
			
			mov				SI,temp_SI
			mov				BYTE PTR ES:[SI-3],100100b 	// attributes: Green background, Red foreground
			mov				BYTE PTR ES:[SI-4],'-'	
		}
	}
	
	if(location%40 - 3 >= 0 ){
		asm{
			mov				SI,temp_SI
			mov				BYTE PTR ES:[SI-5],00100100b // attributes: Green background, Red foreground
			mov				BYTE PTR ES:[SI-6],'-'	
		}
	}
	
	if(location%40 - 4 >= 0 ){
		asm{
			mov				SI,temp_SI
			mov				BYTE PTR ES:[SI-7],100100b // attributes: Green background, Red foreground
			mov				BYTE PTR ES:[SI-8],'-'	
		}
	}
	
	if(location%40 - 5 >= 0){
		asm{
			
			mov				SI,temp_SI
			mov				BYTE PTR ES:[SI-9],100100b // attributes: Green background, Red foreground
			mov				BYTE PTR ES:[SI-10],'-'
		}
	}
	
	if(location%40 - 6 >= 0){
		asm{
			
			mov				SI,temp_SI
			mov				BYTE PTR ES:[SI-11],100100b // attributes: Green background, Red foreground
			mov				BYTE PTR ES:[SI-12],'-'			
		}
	}
	if(location%40 - 7 >= 0){
		asm{
			
			mov				SI,temp_SI
			mov				BYTE PTR ES:[SI-13],0011110b // attributes: Blue backgroun	(the back of the arrow
			mov				BYTE PTR ES:[SI-14],' '			
		}
	}
}

void Play_game()
{
	int slower = 10;
	while(c != 27 )
	{
		// here were adding Shelli's optional slowdown_LOOP
		// latch is a constant, and we use this slowing_down loop
		// increase Resolution_Counter avery int8, 
		// break loop (== continue with game) every time 
		// Resolution_Counter%slower reached a hole circle, 
		// meaning the game 'ticks' in resulotion: slower/1000
		while(Resolution_Counter%slower != 0); 
		
		if ( scanCode == 77){	// right arrow pressed
			moved_right_flag = 1 - moved_right_flag;	
			scanCode = 'p';	// reset scanCode
		}
		if (scanCode == 72 && slower > 3){	// up arrow pressed
			scanCode = 'p';	// reset scanCode
			slower--;		// increase slower, to change the break 
							// condition of slowe_down_loop in 1/1000 res
		}
		if ( scanCode == 80){	// down arrow pressed, prevent underflow
			scanCode = 'p';	// reset scanCode
			slower++; 		// 
		}
		if(moved_right_flag == 0)
		{
			if ((location+=80) >= 1000)
			{	// move one line down
				location_row =0; // reset to up-most
				location =0;			
				Paint_in_blue();
			}
		}
		else 
		{
			if ((location_row++) >=40)
			{	// move one line down
				location_row =0;	// reset to up-most
				location =0;	
				Paint_in_blue();
			}
		}
		Paint_Arrow(location + location_row);
		Set_Cursor(location + location_row);
	}
}
/*------------------------------------------------------------------------
 *  xmain  --  example of 2 processes executing the same code concurrently
 *------------------------------------------------------------------------
 */

#define ARROW_NUMBER 5
#define TARGET_NUMBER 4

int receiver_pid;


void kill_game(void){
	// Return screen to regular settings, textual graphic setting
	asm{
		MOV  AX,2
        INT  10h
	}
}

INTPROC new_int9(int mdevno)
{
 char result = 0;
 int scan = 0;
  int ascii = 0;

asm {
  MOV AH,1
  INT 16h
  JZ Skip1
  MOV AH,0
  INT 16h
  MOV BYTE PTR scan,AH
  MOV BYTE PTR ascii,AL
 } //asm
 if (scan == 75)
   result = 'a';
 else
   if (scan == 72)
     result = 'w';
   else
   if (scan == 77)
      result = 'd';
 if ((scan == 46)&& (ascii == 3)) // Ctrl-C?
	kill_game();
   asm INT 27; // terminate xinu

   send(receiver_pid, result); 

Skip1:

} // new_int9

void set_new_int9_newisr()
{
  int i;
  for(i=0; i < 32; i++)
    if (sys_imp[i].ivec == 9)
    {
     sys_imp[i].newisr = new_int9;
     return;
    }

} // set_new_int9_newisr


typedef struct position
{
  int x;
  int y;

}  POSITION;

char display[2001];

char ch_arr[2048];
int front = -1;
int rear = -1;

int point_in_cycle;
int gcycle_length;
int gno_of_pids;


/*------------------------------------------------------------------------
 *  prntr  --  print a character indefinitely
 *------------------------------------------------------------------------
 */



void displayer( void )
{
	while (1)
         {
               receive();
               //sleept(18);
               printf(display);
         } //while
} // prntr

void receiver()
{
  while(1)
  {
    char temp;
    temp = receive();
    rear++;
    ch_arr[rear] = temp;
    if (front == -1)
       front = 0;
    //getc(CONSOLE);
  } // while

} //  receiver



char display_draft[25][80];
POSITION target_pos[TARGET_NUMBER];
POSITION arrow_pos[ARROW_NUMBER];


void updateter()
{

  int i,j;
  int gun_position;           
  int no_of_arrows;
  int target_disp = 80/TARGET_NUMBER;
  char ch;

  int no_of_targets;

  no_of_arrows = 0;

  no_of_targets = 4;

  gun_position = 39;

  target_pos[0].x = 3;
  target_pos[0].y = 0; 


  for(i=1; i < TARGET_NUMBER; i++)
  {
    target_pos[i].x = i*target_disp;
    target_pos[i].y = 0; 

  } // for
  for(i=0; i < ARROW_NUMBER; i++)
       arrow_pos[i].x =  arrow_pos[i].y = -1;

  while(1)
  {

   receive();

   while(front != -1)
   {
     ch = ch_arr[front];
     if(front != rear)
       front++;
     else
       front = rear = -1;

     if ( (ch == 'a') || (ch == 'A') )
       if (gun_position >= 2 )
              gun_position--;
       else;
     else if ( (ch == 'd') || (ch == 'D') )
       if (gun_position <= 78 )
         gun_position++;
       else;
     else if ( (ch == 'w') || (ch == 'W') )
       if (no_of_arrows < ARROW_NUMBER)
       {
         arrow_pos[no_of_arrows].x = gun_position;
         arrow_pos[no_of_arrows].y = 23;
         no_of_arrows++;

       } // if
   } // while(front != -1)

     ch = 0;
     for(i=0; i < 25; i++)
        for(j=0; j < 80; j++)
            display_draft[i][j] = ' ';  // blank

    display_draft[22][gun_position] = '^';
    display_draft[23][gun_position-1] = '/';
    display_draft[23][gun_position] = '|';
    display_draft[23][gun_position+1] = '\\';
    display_draft[24][gun_position] = '|';

    for(i=0; i < ARROW_NUMBER; i++)
       if (arrow_pos[i].x != -1)
       {
         if (arrow_pos[i].y > 0)
           arrow_pos[i].y--;
           display_draft[arrow_pos[i].y][arrow_pos[i].x] = '^';
           display_draft[arrow_pos[i].y+1][arrow_pos[i].x] = '|';

       } // if

    for(i=0; i < TARGET_NUMBER; i++)
       if (target_pos[i].x != -1)
        {
         if (target_pos[i].y < 22)
              target_pos[i].y++;
         display_draft[target_pos[i].y][target_pos[i].x] = '*';
        } // if

    for(i=0; i < 25; i++)
      for(j=0; j < 80; j++)
        display[i*80+j] = display_draft[i][j];
		//Paint_Arrow(i);
    display[2000] = '\0';

  } // while(1)

} // updater 

int sched_arr_pid[5] = {-1};
int sched_arr_int[5] = {-1};


SYSCALL schedule(int no_of_pids, int cycle_length, int pid1, ...)
{
  int i;
  int ps;
  int *iptr;

  disable(ps);

  gcycle_length = cycle_length;
  point_in_cycle = 0;
  gno_of_pids = no_of_pids;

  iptr = &pid1;
  for(i=0; i < no_of_pids; i++)
  {
    sched_arr_pid[i] = *iptr;
    iptr++;
    sched_arr_int[i] = *iptr;
    iptr++;
  } // for
  restore(ps);

} // schedule 

xmain()
{
        int uppid, dispid, recvpid;
		int procces1_pid, procces2_pid;


		// set screen to textual mode
		Set_Screen();  
        resume( dispid = create(displayer, INITSTK, INITPRIO, "DISPLAYER", 0) );
        resume( recvpid = create(receiver, INITSTK, INITPRIO+3, "RECIVEVER", 0) );
        resume( uppid = create(Play_game, INITSTK, INITPRIO, "UPDATER", 0) );
        receiver_pid =recvpid;  
		
		//resume( procces1_pid = create(process1, INITSTK, INITPRIO, "PROCESS1", 0) );
        //resume( procces2_pid = create(process2, INITSTK, INITPRIO, "PROCESS2", 0) );

		//int8save = getvect(8);
		//setvect(8,newint8);
		//int9save = getvect(9);
		//setvect(9,newint9);
        set_new_int9_newisr();
		schedule(2,57, dispid, 0,  uppid, 29);
} // xmain