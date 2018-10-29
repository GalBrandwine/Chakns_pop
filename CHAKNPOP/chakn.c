/* game.c - xmain, prntr */

#include <conf.h>
#include <kernel.h>
#include <io.h>
#include <bios.h>
#include <stdio.h>
#include <math.h>
#include <butler.h>

extern SYSCALL  sleept(int);
extern struct intmap far *sys_imp;
#define WALL_COLOR 40
#define EMPTY_SPACE 120
#define NUMOFLIFES 3
#define HEARTCOLLOR 65

/*------------------------------------------------------------------------
 *  xmain  --  example of 2 processes executing the same code concurrently
 *------------------------------------------------------------------------
 */


int receiver_pid;
int (*old9newisr)(int);
 int uppid, dispid, recvpid, stage_1_pid;
INTPROC new_int9(int mdevno)
{
char result;
 int scan = 0;
 static int ctrl_pressed  = 0;

asm {
   PUSH AX
   IN AL,60h
   MOV BYTE PTR scan,AL
   POP AX
 } //asm

 if (scan == 29)
    ctrl_pressed  = 1;
 else
   if (scan == 157)
     ctrl_pressed  = 0;
   else  
     if ((scan == 46) && (ctrl_pressed == 1)) // Control-C?
     {
		 // Part1: Initialize the display adapter
		asm{
			MOV              AH, 0// Select function = 'Set mode'
			MOV              AL, 2// restore the display mode
			INT              10h// Adapter initialized.Page 0 displayed
			INT 27
		}
     } // if
     else
     if ((scan == 2) && (ctrl_pressed == 1)) // Control-C?
        send(butlerpid, MSGPSNAP);
     else
     if ((scan == 3) && (ctrl_pressed == 1)) // Control-C?
        send(butlerpid, MSGTSNAP);
     else
     if ((scan == 4) && (ctrl_pressed == 1)) // Control-C?
        send(butlerpid, MSGDSNAP);
	else
     if ((scan == 30)) // 'A' pressed
       moveChack('L');
	   else
     if ((scan == 32)) // 'D' pressed
       moveChack('R');
	    else
     if ((scan == 17)) // 'W' pressed
       moveChack('U');
	      else
     if ((scan == 31)) // 'S' pressed
       moveChack('D');
	   
 // old9newisr(mdevno);

return 0;
}

void set_new_int9_newisr()
{
  int i;
  for(i=0; i < 32; i++)
    if (sys_imp[i].ivec == 9)
    {
		old9newisr = sys_imp[i].newisr;
     sys_imp[i].newisr = new_int9;
     return;
    }

} // set_new_int9_newisr


typedef struct position
{
  int x;
  int y;

}  POSITION;


/*------------------------------------------------------------------------
/* Game characters 
 *------------------------------------------------------------------------
 */
typedef struct chack
{
	char *name;	// user can set Chack's name in MENU scren.
	int score;
	int life;	// Life must be above 0, initiates with  NUMOFLIFES
	int gravity; // 0 means standing on floor, 1 means standing on floor
	POSITION position;
} CHACK;

typedef struct chicken
{
	int level; // checken level will define how fast eggs are being layed.
	POSITION position;
} CHICKEN;

typedef struct monster
{
	int alive;	// alive is a boolean flag, it is set to true when moster is born out of egg, and set to FALSE when monster entered to granade smoke.
	int gravity; // 0 means standing on floor, 1 means standing on floor
	POSITION position;
}MONSTER;




char display[2001];			//char
char display_color[2001];	//Color
char display_background [25][80]; //level design
char display_background_color [25][80]; //level design color, for sampeling where the items are in the screen

char ch_arr[2048];
int front = -1;
int rear = -1;

int point_in_cycle;
int gcycle_length;
int gno_of_pids;

/*------------------------------------------------------------------------
 *  New Stuff	New Stuff	New Stuff	New Stuff	New Stuff
 *------------------------------------------------------------------------
 */
POSITION *chackPosition;
CHACK *chack;

void SetScreen ()
{
	asm{
		PUSH AX
		MOV AH,0 //Select function = 'SET MODE'
		MOV AL,3 //80 BY 25 Color image
		INT 10H  //Adapter initilizedD. Page 0 displayed
		POP AX
	}
} // SetScreen

void drawInPosL(int pos,char letter,char att) //draw on screen
{
	asm{
		PUSH AX
		PUSH CX
		PUSH BX
		MOV AX,0B800h
		MOV ES,AX
		MOV BX,pos
		MOV AL,att
		MOV BYTE PTR ES:([BX]+1),AL
		MOV AL,letter
		MOV BYTE PTR ES:([BX]),AL
		POP BX
		POP CX
		POP AX
	}
}
void drawChack()
{
	display_background[chack->position.y][chack->position.x]= '^';
	display_background[chack->position.y][chack->position.x-1]= '(';
	display_background[chack->position.y][chack->position.x+1]= ')';
	send(dispid,1);
}
moveChack(char side)
{
	
	display_background[chack->position.y][chack->position.x]= ' ';
	display_background[chack->position.y][chack->position.x-1]= ' ';
	display_background[chack->position.y][chack->position.x+1]= ' ';
	
	switch (side)
	{
		case 'R'://move  right
		if(display_background_color[chack->position.y][(chack->position.x+1)] != 40)//if its not green wall
		{
			chack->position.x = (chack->position.x+1)%80;
		}
		
		break;
		case 'L'://move left
		if(display_background_color[chack->position.y][(chack->position.x-1)] != 40)//if its not green wall
		{
			chack->position.x = (chack->position.x-1)%80;
		}
		break;
		case 'U'://move up
		if(display_background_color[chack->position.y-1][(chack->position.x)] != 77)//if its not red wall
		{
			chack->position.y = (chack->position.y-1)%25;
		}
		break;
		case 'D'://move down
		if(display_background_color[chack->position.y+1][(chack->position.x)] != 77)//if its not red wall
		{
			chack->position.y = (chack->position.y+1)%25;
		}
		break;
	}
	while((display_background_color[chack->position.y+1][(chack->position.x)]!= 77) && (display_background_color[chack->position.y+1][(chack->position.x)] != 40))
	{
		display_background[chack->position.y][chack->position.x]= ' ';
		display_background[chack->position.y][chack->position.x-1]= ' ';
		display_background[chack->position.y][chack->position.x+1]= ' ';
		chack->position.y = (chack->position.y+1);
		
		drawChack();
	}
	drawChack();
}
void move_chicken(CHICKEN *chicken_input){ // move chicken by stage setup.
	CHICKEN *chicken = chicken_input;
	static int direction = 0;	// 0 - move left, 1 - move right
	int temp_x;
	int temp_y;

	if(direction == 0 && display_background_color[chicken->position.y][(chicken->position.x+1)] != 40)//if its not green wall
	{
		chicken->position.x = (chicken->position.x+1)%80;
		if((chicken->position.x+1)%80 == 79)direction =1;	// change sirection
	}		
	if(direction == 1 && display_background_color[chicken->position.y][(chicken->position.x-1)] != 40)//if its not green wall
	{
		chicken->position.x = (chicken->position.x-1)%80;
		if((chicken->position.x) == 1)direction =0;	// change sirection
	}
	if(display_background_color[chicken->position.y-1][(chicken->position.x)] != 77)//if its not red wall
	{
		chicken->position.y = (chicken->position.y-1)%25;
	}
	if(display_background_color[chicken->position.y+1][(chicken->position.x)] != 77)//if its not red wall
	{
		chicken->position.y = (chicken->position.y+1)%25;
	}
}

void draw_chicken(CHICKEN *chicken_input){
	CHICKEN *chicken = chicken_input;	// dave initiated chicken;
	while (1){
		sleept(1);	// replace with 70hexa flags
		display_background[chicken->position.y][chicken->position.x]= ' ';
		display_background[chicken->position.y][chicken->position.x-1]= ' ';
		display_background[chicken->position.y][chicken->position.x+1]= ' ';
		move_chicken(chicken);
		display_background[chicken->position.y][chicken->position.x]= '^';
		display_background[chicken->position.y][chicken->position.x-1]= '^';
		display_background[chicken->position.y][chicken->position.x+1]= '=';
		send(dispid,1);
		}
}
/*------------------------------------------------------------------------
 *  prntr  --  print a character indefinitely
 *------------------------------------------------------------------------
 */


 /*------------------------------------------------------------------------
 *  stage_1  --  print stage 1 hard_coded
 *------------------------------------------------------------------------
 */
 void print_stage_1(){
	int i,j,temp_j,pos;
	int hole_flag =	0;
	int edge_needed_left =1;
	int edge_needed_right =0;
	int hole_size = 5;
	int number_of_hearts = 3;
	
		for(i = 0; i < 25; i++ )
		{
			for(j = 0; j < 80; j++)
			{
				pos = 2*(i*80 + j);
				// print stage rounding square
				if( i ==0 || j == 0 || i ==24 || j==79)
				{
					display_background_color[i][j] = WALL_COLOR;
				}
				else if (i%4 == 0){		// print floors
					
					if (j < hole_size && edge_needed_left == 1 && edge_needed_right == 0){ // set flags for printing only left_hole
						display_background_color[i][j] = EMPTY_SPACE;
						
					}

					else if (j + hole_size > 80 && edge_needed_left == 0 && edge_needed_right == 1){ // set plags for printing right_hole
						display_background_color[i][j] = EMPTY_SPACE;
						
					}
					else{
						display_background_color[i][j] = WALL_COLOR;
					}
				}	
				else{
					display_background_color[i][j] = EMPTY_SPACE;
			}
				if (j == 79 && i%4 == 0){ // make stage 1 patterns (shti va erev)
					edge_needed_left = 1 - edge_needed_left;
					edge_needed_right = 1- edge_needed_right;
				}
			}

			// Print stage hearts.
			// by the function: pos = 2*(i*80 + j);
			// heart 1
			display_background[7][9] = '<';
			display_background_color[7][9] = HEARTCOLLOR;
			
			display_background[7][10] = 'B';
			display_background_color[7][10] = HEARTCOLLOR;

			// heart 2
			display_background[15][55] = '<';
			display_background_color[15][55] = HEARTCOLLOR;
			
			display_background[15][56] = 'B';
			display_background_color[15][56] = HEARTCOLLOR;
			
			// heart 3
			display_background[19][9] = '<';
			display_background_color[19][9] = HEARTCOLLOR;
			
			display_background[19][10] = 'B';
			display_background_color[19][10] = HEARTCOLLOR;
	
			
			drawChack();
		}
		send(dispid,1);
 }

  void stage_1(){
	// create a chicken.
	CHICKEN *chicken;
	POSITION *chickenPosition;
	int chicken_pid;
	
	print_stage_1();	// print stage on the screen
	 
	 // initiate chicken.
	chickenPosition=(POSITION *)malloc(sizeof(POSITION));
	chickenPosition->x=7;
	chickenPosition->y=2;
	chicken->position=*chackPosition;

	resume( chicken_pid = create(draw_chicken, INITSTK, INITPRIO, "CHICKEN_DRAWER", 1, chicken) );


 }

void displayer( void )
{	//This process display the matrix, receive message (and return to ready) with every change in the screen matrix
	int i,j,pos;
	while (1)
         {
               receive();
			   for(i=0;i<25;i++)
			   {
				   for(j=0;j<80;j++)
				   {
					   pos = 2*(i*80 + j);
					   drawInPosL(pos,display_background[i][j],display_background_color[i][j]);	// display the whole screen
				   }
			   }
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
  } // while
} //  receiver

/*
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
    display[2000] = '\0';

  } // while(1)

} // updater 
*/

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
       
		SetScreen();		//intiate screen mode
		//print();
        resume( dispid = create(displayer, INITSTK, INITPRIO, "DISPLAYER", 0) );
        resume( recvpid = create(receiver, INITSTK, INITPRIO+3, "RECIVEVER", 0) );
       // resume( uppid = create(updateter, INITSTK, INITPRIO, "UPDATER", 0) );
		resume( stage_1_pid = create(stage_1, INITSTK, INITPRIO, "STAGE1", 0) );
        receiver_pid =recvpid;  
        set_new_int9_newisr();
		
		chackPosition=(POSITION *)malloc(sizeof(POSITION));
		chackPosition->x=7;
		chackPosition->y=2;
		chack->position=*chackPosition;
		chack->life = NUMOFLIFES;

    schedule(2,57, dispid, 0,  uppid, 29,stage_1_pid,30);
} // xmain
