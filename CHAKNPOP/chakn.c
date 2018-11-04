/* game.c - xmain, prntr */

#include <conf.h>
#include <kernel.h>
#include <io.h>
#include <bios.h>
#include <stdio.h>
#include <math.h>
#include <butler.h>
#include <sleep.h>

#define ON (1)
#define OFF (0)
  /*------------------------------------------------
  ChangeSpeaker - Turn speaker on or off. */

void ChangeSpeaker(int status)
{
	int portval;
	//   portval = inportb( 0x61 );

	portval = 0;
	asm{
		PUSH AX
		MOV AL,61h
		MOV byte ptr portval,AL
		POP AX
	}

		if (status == ON)
			portval |= 0x03;
		else
			portval &= ~0x03;
	// outportb( 0x61, portval );
	asm{
		PUSH AX
		MOV AX,portval
		OUT 61h,AL
		POP AX
	} // asm

} /*--ChangeSpeaker( )----------*/
void Sound(int hertz)
{
	unsigned divisor = 1193180L / hertz;

	ChangeSpeaker(ON);

	//        outportb( 0x43, 0xB6 );
	asm{
		PUSH AX
		MOV AL,0B6h
		OUT 43h,AL
		POP AX
	} // asm


	  //       outportb( 0x42, divisor & 0xFF ) ;
		asm{
		PUSH AX
		MOV AX,divisor
		AND AX,0FFh
		OUT 42h,AL
		POP AX
	} // asm


	  //        outportb( 0x42, divisor >> 8 ) ;

		asm{
		PUSH AX
		MOV AX,divisor
		MOV AL,AH
		OUT 42h,AL
		POP AX
	} // asm

} /*--Sound( )-----*/

void NoSound(void)
{
	ChangeSpeaker(OFF);
} /*--NoSound( )------*/
void grenade_sound() {
	Sound(20);
	sleept(600);
	NoSound();
	sleept(70);

}
extern SYSCALL  sleept(int);
extern SYSCALL	resched();
extern struct intmap far *sys_imp;


/*------------------------------------------------------------------------
 *  Game parameters.
 *------------------------------------------------------------------------
 */
#define WALL_COLOR 40
#define EMPTY_SPACE 120
#define FINNISH_GATE 155
#define NUMOFLIFES 3
#define HEARTCOLLOR 65
#define MONSTER_COLOR 0x30
#define MAX_MONSTERS 10
#define MAX_ENEMIES 15

#define GRANADE_SMOKE_COLOR 70

/*
Current stage parameters:
	0 - menu, 
	1 - stage
	2 - stage
	3 - stage
*/
int current_stage = 0; 
int displayer_sem = 1; 
int receiver_pid;
int (*old9newisr)(int);
int uppid, dispid, recvpid, stage_manager_pid, stage_0_pid, stage_1_pid, stage_2_pid, stage_3_pid, platform_3_pid, platform_3_pid1,sound_id ;
volatile int global_flag;
volatile int global_timer =0 ;

void interrupt (*old0x70isr)(void);
void platform(int min,int max,int hight,int movment, int left);					//general platform function

void stage_3_platform();		// platform 3 proc
void stage_3_platform2();		// platform 3 proc 2

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
 *  Latch  --  for changing game speed.
 *------------------------------------------------------------------------
 */
void setLatch(int latch)
{
	asm{
		  CLI			// critial scanCode:
		  PUSH AX		// here were programin the timer (ch0)
		  MOV AL,036h	// to work in (1,193,180/1193) = 1096 hz	
		  OUT 43h,AL	//
		  MOV AX,latch	//
		  OUT 40h,AL	//
		  MOV AL,AH		//
		  OUT 40h,AL 	//
		  POP AX		//
		  STI
	}
}


/*------------------------------------------------------------------------
 *  new0x70isr  --  for controlling tiiming of the game.
 *------------------------------------------------------------------------
 */
void interrupt new0x70isr(void)
{
  global_flag = 1;
  global_timer++;
  asm {
   PUSH AX
   PUSH BX
   IN AL,70h   // Read existing port 70h
   MOV BX,AX

   MOV AL,0Ch  // Set up "Read status register C"
   OUT 70h,AL  //
   MOV AL,8Ch  //
   OUT 70h,AL  //
   IN AL,71h
   MOV AX,BX   //  Restore port 70h
   OUT 70h,AL  // 

   MOV AL,20h   // Set up "EOI" value  
   OUT 0A0h,AL  // Notify Secondary PIC
   OUT 020h,AL  // Notify Primary PIC

   POP BX
   POP AX

  } // asm */
} // new0x70isr





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
	else if (scan == 157)
		ctrl_pressed  = 0;
	else if ((scan == 46) && (ctrl_pressed == 1)) // Control-C?
    {		
		nosound();
		// Part1: Initialize the display adapter
		setvect(0x70,old0x70isr);	// restore old x70 ISR.
		asm{
			MOV              AH, 0// Select function = 'Set mode'
			MOV              AL, 2// restore the display mode
			INT              10h// Adapter initialized.Page 0 displayed
			INT 27
		}
    } // if
    else if ((scan == 2) && (ctrl_pressed == 1)) // Control-C?
		send(butlerpid, MSGPSNAP);
    else if ((scan == 3) && (ctrl_pressed == 1)) // Control-C?
		send(butlerpid, MSGTSNAP);
    else if ((scan == 4) && (ctrl_pressed == 1)) // Control-C?
        send(butlerpid, MSGDSNAP);

	send(uppid,scan);
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


/*------------------------------------------------------------------------
 * Position sturct
 *------------------------------------------------------------------------
 */
typedef struct position
{
  int x;
  int y;

}  POSITION;


char randDirection(){
	int newDirection=rand()%4;
	switch(newDirection)
	{
		case 0:
		return  'U';
		break;
		case 1:
		return 'D';
		break;
		case 2:
		return 'L';
		break;
		case 3:
		return 'R';
		break;
	}
 }




/*------------------------------------------------------------------------
 * Print to screen:
 *	taken from: https://wiki.osdev.org/Printing_To_Screen
 *	modified by gal.
 *------------------------------------------------------------------------
 */
void write_string( int string_start_pos_y, int string_start_pos_x, int colour, char *string )
{
    int str_len = 0;

	while( *string != '\0' ){
	   display_background[string_start_pos_y][string_start_pos_x] = *string++;
	   display_background_color[string_start_pos_y][string_start_pos_x++] = colour;
	}
}

/*------------------------------------------------------------------------
/* Game characters 
 *------------------------------------------------------------------------
 */
typedef struct chack
{
	char *name;		// user can set Chack's name in MENU scren.
	int score;
	int life;		// Life must be above 0, initiates with  NUMOFLIFES
	int gravity;	// 0 means standing on floor, 1 means standing on floor
	POSITION position;
} CHACK;

typedef struct chicken
{
	int level; // checken level will define how fast eggs are being layed.
	POSITION position;
} CHICKEN;

typedef struct monster
{
	int alive;			// alive is a boolean flag, it is set to true when moster is born out of egg, and set to FALSE when monster entered to granade smoke.
	char direction;		//the direction of the monster(U,D,R,L)
	POSITION position;
	POSITION oldPosition;
	char oldAttribute[3];
	char oldChar[3];
	
}MONSTER;







POSITION *chackPosition;
CHACK *chack;
MONSTER monsters[MAX_MONSTERS];	// Array of monsters, for controlling theirs PID's


/*------------------------------------------------------------------------
 * Set screen to text mode.
 *------------------------------------------------------------------------
 */
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


/*------------------------------------------------------------------------
 *  Chuck section
 *------------------------------------------------------------------------
 */
void drawChack()
{
	display_background[chack->position.y][chack->position.x]= '^';
	display_background[chack->position.y][chack->position.x-1]= '(';
	display_background[chack->position.y][chack->position.x+1]= ')';
	send(dispid,1);
}

void moveChack(char side)
{
	int jumpCounter=0;
	display_background[chack->position.y][chack->position.x]= ' ';
	display_background[chack->position.y][chack->position.x-1]= ' ';
	display_background[chack->position.y][chack->position.x+1]= ' ';
	
	switch (side)
	{
		case 'R'://move  right
		if(display_background_color[chack->position.y][(chack->position.x+1)] != WALL_COLOR)//if its not green wall
		{
			chack->position.x = (chack->position.x+1)%80;
		}
		else if(display_background_color[chack->position.y-1][(chack->position.x+1)] != WALL_COLOR && (display_background_color[chack->position.y-1][(chack->position.x)] != WALL_COLOR))//if he can climb the wall
		{
			display_background[chack->position.y][chack->position.x]= ' ';
			display_background[chack->position.y][chack->position.x-1]= ' ';
			display_background[chack->position.y][chack->position.x+1]= ' ';
			chack->position.y = (chack->position.y-1)%80;
			drawChack();
			sleept(1);
			display_background[chack->position.y][chack->position.x]= ' ';
			display_background[chack->position.y][chack->position.x-1]= ' ';
			display_background[chack->position.y][chack->position.x+1]= ' ';
			chack->position.x = (chack->position.x+1)%80;
			drawChack();
		}
		else if (display_background_color[chack->position.y][(chack->position.x+1)] != FINNISH_GATE)// if hit exit door
		{
			// TODO: change stage, from current to the next stage, or menu if finnished.
		}

		break;
		case 'L'://move left
		if(display_background_color[chack->position.y][(chack->position.x-1)] != WALL_COLOR)//if its not green wall
		{
			chack->position.x = (chack->position.x-1)%80;
		}
		else
			if(display_background_color[chack->position.y-1][(chack->position.x-1)] != WALL_COLOR && (display_background_color[chack->position.y-1][(chack->position.x)] != WALL_COLOR))//if he can climb the wall
		{
			display_background[chack->position.y][chack->position.x]= ' ';
			display_background[chack->position.y][chack->position.x-1]= ' ';
			display_background[chack->position.y][chack->position.x+1]= ' ';
			chack->position.y = (chack->position.y-1)%80;
			drawChack();
			sleept(1);
			display_background[chack->position.y][chack->position.x]= ' ';
			display_background[chack->position.y][chack->position.x-1]= ' ';
			display_background[chack->position.y][chack->position.x+1]= ' ';
			chack->position.x = (chack->position.x-1)%80;
			drawChack();
		}
		break;
		case 'U'://move up
		jumpCounter =0;
		while((display_background_color[chack->position.y-1][(chack->position.x)] != WALL_COLOR) && (jumpCounter<3))
		{
			display_background[chack->position.y][chack->position.x]= ' ';
			display_background[chack->position.y][chack->position.x-1]= ' ';
			display_background[chack->position.y][chack->position.x+1]= ' ';
			chack->position.y = (chack->position.y-1)%25;
			jumpCounter++;
			drawChack();
			sleept(100);
			
		}
		if(display_background_color[chack->position.y-1][(chack->position.x)] == WALL_COLOR){
			chack->gravity=0;
		}
		break;
		case 'D'://move down
		while((display_background_color[chack->position.y+1][(chack->position.x)]!= WALL_COLOR))//chack is falling
	{
		display_background[chack->position.y][chack->position.x]= ' ';
		display_background[chack->position.y][chack->position.x-1]= ' ';
		display_background[chack->position.y][chack->position.x+1]= ' ';
		chack->position.y = (chack->position.y+1);
		
		drawChack();
		sleept(100);
	}
	break;
}
	if((display_background_color[chack->position.y+1][(chack->position.x)] != WALL_COLOR))
	{
		if( (display_background_color[chack->position.y-1][(chack->position.x)] == WALL_COLOR))
		{
		chack->gravity=0;
		}
		else
		{
		chack->gravity=1;
		}	
	
	}
	while((chack->gravity==1) && (display_background_color[chack->position.y+1][(chack->position.x)]!= WALL_COLOR))//chack is falling
	{
		display_background[chack->position.y][chack->position.x]= ' ';
		display_background[chack->position.y][chack->position.x-1]= ' ';
		display_background[chack->position.y][chack->position.x+1]= ' ';
		chack->position.y = (chack->position.y+1);
		drawChack();
		sleept(100);
	}
	drawChack();
}


/*------------------------------------------------------------------------
 *  Grenede section
 *------------------------------------------------------------------------
 */
 void free_heart(int y, int temp_y, int x, int temp_x){
	/*Funstion for animating frying hert.
	if a heart id has been freed, than it should go up to the ceiling and make a new gate.
	*/
	int hole_width = 5;
	int temp;

	for ( ; temp_y >= 0; temp_y--){
			if (display_background_color[temp_y][temp_x] != WALL_COLOR){
				display_background_color[temp_y - y][temp_x + x]= EMPTY_SPACE - 30;
			}
	}
	for (temp =0 ; temp <= hole_width; temp++){
		display_background_color[0][temp_x + temp]= FINNISH_GATE;	
	}
 }


void throw_granade(int direction){
	/* Function for drawing throwen granades.
		
	parameter: direction - 1 throw left, 0 throw right.
	*/

	// Fly 1 seconds, remember that latch suppose to be set to work at 1000hz, 
	// so flying time will be reduced once every 100 ms, hance a second
	int flying_time = 10;	
	int flying_tod = tod;
	int granade_explode_timer = tod; // sleep 3 sec before exploding;
	int temp_x = chack->position.x;
	int temp_y = chack->position.y;

	int y;
	int x;
	

	while (1){
		if (abs(tod - flying_tod) >= 100){	// print to tscreen granade every 0.1 sec
			
			display_background[temp_y][temp_x] = ' ';
			if( direction == 1){// move granade left
				temp_x--;
				display_background[temp_y][temp_x] = '-';
				display_background[temp_y][temp_x-1] = '<';
			}
			else {// move granade right.
				temp_x++;
				display_background[temp_y][temp_x] = '-';
				display_background[temp_y][temp_x+1] = '>';
			}


			flying_time--;		
			send(dispid,1);
			flying_tod = tod;
		}
		if (flying_time < 0) {
			break;
		}
	}

	// Fall until granade hit breen ground.
	while(display_background_color[temp_y+1][temp_x] != 40){
		display_background[temp_y][temp_x-1] = ' ';
		display_background[temp_y][temp_x+1] = ' ';
		display_background[temp_y][temp_x] = ' ';
		temp_y ++;
	}
	display_background[temp_y][temp_x] = '^';
	display_background[temp_y][temp_x+1] = '>';
	display_background[temp_y][temp_x-1] = '<';


	// Ugly busy/wait loop.
	while (abs(tod - granade_explode_timer) <= 3000){
	}
	
	for (y = 0; y <= 1; y++){
		for(x = -1; x <= 1; x++){
			if (display_background_color[temp_y - y][temp_x + x] == HEARTCOLLOR){
				free_heart(y, temp_y, x, temp_x);
			}
			display_background_color[temp_y - y][temp_x + x]= GRANADE_SMOKE_COLOR;
			
		}
	}
	if (display_background_color[temp_y][temp_x - 2] == HEARTCOLLOR || display_background_color[temp_y][temp_x + 2] == HEARTCOLLOR){
				free_heart(y, temp_y, x, temp_x);
	}
	display_background_color[temp_y][temp_x - 2]= GRANADE_SMOKE_COLOR;
	display_background_color[temp_y][temp_x + 2]= GRANADE_SMOKE_COLOR;
	display_background[temp_y][temp_x]= ' ';
	display_background[temp_y][temp_x+1] = ' ';
	display_background[temp_y][temp_x-1] = ' ';

	// Reset granade_explode_timer timer;

	granade_explode_timer = tod;
	grenade_sound();
	while (abs(tod - granade_explode_timer) <= 3000){
	}

	// Clear granade's smoke.
	for (y = 0; y <= 1; y++){
		for(x = -1; x <= 1; x++){
			display_background_color[temp_y - y][temp_x + x]= EMPTY_SPACE;
		}
	}
	display_background_color[temp_y][temp_x - 2]= EMPTY_SPACE;
	display_background_color[temp_y][temp_x + 2]= EMPTY_SPACE;
}


/*------------------------------------------------------------------------
 *  Monsters section
 *------------------------------------------------------------------------
 */
void moveMonster( MONSTER *monster)
{
	
	int newDirection;
	sleept(1);
	switch (monster->direction)
	{
		case 'R'://move  right
		if((display_background_color[monster->position.y][(monster->position.x+2)] != WALL_COLOR))
		{
			monster->oldAttribute[0] = monster->oldAttribute[1];
			monster->oldAttribute[1] = monster->oldAttribute[2];
			monster->oldAttribute[2] = display_background_color[monster->position.y][monster->position.x + 2];
			monster->oldChar[0] = monster->oldChar[1];
			monster->oldChar[1] = monster->oldChar[2];
			monster->oldChar[2] = display_background[monster->position.y][monster->position.x + 2];


			monster->oldPosition.y = monster->position.y;
			monster->oldPosition.x = monster->position.x;
			display_background[monster->oldPosition.y][monster->oldPosition.x - 1] = monster->oldChar[0];
			display_background[monster->oldPosition.y][monster->oldPosition.x] = monster->oldChar[1];
			display_background[monster->oldPosition.y][monster->oldPosition.x + 1] = monster->oldChar[2];
			display_background_color[monster->oldPosition.y][monster->oldPosition.x - 1] = monster->oldAttribute[0];
			display_background_color[monster->oldPosition.y][monster->oldPosition.x] = monster->oldAttribute[1];
			display_background_color[monster->oldPosition.y][monster->oldPosition.x + 1] = monster->oldAttribute[2];
			monster->position.x = (monster->position.x+1)%80;
			sleept(50);
			
			
		}
		 else
		 {
			 monster->direction =randDirection();
		 }
		break;
		case 'L'://move left
		if((display_background_color[monster->position.y][(monster->position.x-2)] != WALL_COLOR)){
			monster->oldAttribute[0] = display_background_color[monster->position.y][monster->position.x - 2];
			monster->oldAttribute[1] = monster->oldAttribute[0];
			monster->oldAttribute[2] = monster->oldAttribute[1];
			monster->oldChar[0] = display_background[monster->position.y][monster->position.x - 2];
			monster->oldChar[1] = monster->oldChar[0];
			monster->oldChar[2] = monster->oldChar[1];


			monster->oldPosition.y = monster->position.y;
			monster->oldPosition.x = monster->position.x;
			display_background[monster->oldPosition.y][monster->oldPosition.x - 1] = monster->oldChar[0];
			display_background[monster->oldPosition.y][monster->oldPosition.x] = monster->oldChar[1];
			display_background[monster->oldPosition.y][monster->oldPosition.x + 1] = monster->oldChar[2];
			display_background_color[monster->oldPosition.y][monster->oldPosition.x - 1] = monster->oldAttribute[0];
			display_background_color[monster->oldPosition.y][monster->oldPosition.x] = monster->oldAttribute[1];
			display_background_color[monster->oldPosition.y][monster->oldPosition.x + 1] = monster->oldAttribute[2];
			monster->position.x = (monster->position.x-1)%80;
			sleept(50);
			
		}
		else
		 {
			 monster->direction =randDirection();
		 }
		break;
		case 'U'://move up
		if((display_background_color[monster->position.y-1][(monster->position.x)] != WALL_COLOR)){
			monster->oldAttribute[0] =EMPTY_SPACE;
			monster->oldAttribute[1] = EMPTY_SPACE;
			monster->oldAttribute[2] = EMPTY_SPACE;
			monster->oldChar[0] =' ';
			monster->oldChar[1] = ' ';
			monster->oldChar[2] = ' ';

			monster->oldPosition.y = monster->position.y;
			monster->oldPosition.x = monster->position.x;
			display_background[monster->oldPosition.y][monster->oldPosition.x - 1] = monster->oldChar[0];
			display_background[monster->oldPosition.y][monster->oldPosition.x] = monster->oldChar[1];
			display_background[monster->oldPosition.y][monster->oldPosition.x + 1] = monster->oldChar[2];
			display_background_color[monster->oldPosition.y][monster->oldPosition.x - 1] = monster->oldAttribute[0];
			display_background_color[monster->oldPosition.y][monster->oldPosition.x] = monster->oldAttribute[1];
			display_background_color[monster->oldPosition.y][monster->oldPosition.x + 1] = monster->oldAttribute[2];
			monster->position.y = (monster->position.y-1)%25;

			sleept(50);
		}
		else
		 {
			 monster->direction =randDirection();
		 }
		break;
		case 'D'://move down
			if((display_background_color[monster->position.y+1][(monster->position.x)] != WALL_COLOR)){
				monster->oldAttribute[0] = EMPTY_SPACE;
				monster->oldAttribute[1] = EMPTY_SPACE;
				monster->oldAttribute[2] = EMPTY_SPACE;
				monster->oldChar[0] = ' ';
				monster->oldChar[1] = ' ';
				monster->oldChar[2] = ' ';

				monster->oldPosition.y = monster->position.y;
				monster->oldPosition.x = monster->position.x;
				display_background[monster->oldPosition.y][monster->oldPosition.x - 1] = monster->oldChar[0];
				display_background[monster->oldPosition.y][monster->oldPosition.x] = monster->oldChar[1];
				display_background[monster->oldPosition.y][monster->oldPosition.x + 1] = monster->oldChar[2];
				display_background_color[monster->oldPosition.y][monster->oldPosition.x - 1] = monster->oldAttribute[0];
				display_background_color[monster->oldPosition.y][monster->oldPosition.x] = monster->oldAttribute[1];
				display_background_color[monster->oldPosition.y][monster->oldPosition.x + 1] = monster->oldAttribute[2];
				monster->position.y = (monster->position.y+1)%25;
			
				sleept(50);
			}
			else{
				monster->direction =randDirection();
			}
			break;
	}
}


void drawMonster(MONSTER *monster)
{
	while(1)
	{
	
		moveMonster(monster);
	
		display_background[monster->position.y][monster->position.x-1]= '<';
		display_background[monster->position.y][monster->position.x]= '=';
		display_background[monster->position.y][monster->position.x+1]= 'D';
		display_background_color[monster->position.y][monster->position.x-1]= MONSTER_COLOR;
		display_background_color[monster->position.y][monster->position.x]= MONSTER_COLOR;
		display_background_color[monster->position.y][monster->position.x+1]= MONSTER_COLOR;
	
		send(dispid,1);

		sleept(25);
	
	}
}

void lay_egg(int init_egg_laying_position_y, int init_egg_laying_position_x){
	/* Function for laying eggs.
		
	each egg is  a procces that will bocome a monster.
	*/

	int fall_time = tod%10;
	int fall_tod = tod;
	int falling_speed = 1000;
	int speed_tod = tod;
	int temp_x = init_egg_laying_position_x;
	int temp_y = init_egg_laying_position_y;
	MONSTER m;
	POSITION monsterPosition;

	while (1){
		if (abs(tod - speed_tod) >= 1000){
			display_background[temp_y][temp_x] = ' ';
			temp_y ++;									// update falling position.
			display_background[temp_y][temp_x] = '*';
			fall_time--;		
			send(dispid,1);
			speed_tod = tod;
		}
		if (fall_time < 0) {
			// fall time is over, egg should lay on the floor (GREEN) for x secons and become a monster.
			break;
		}
	}
	// fall until egg hit breen ground.
	while(display_background_color[temp_y+1][temp_x] != 40){
		display_background[temp_y][temp_x] = ' ';
		temp_y ++;
		display_background[temp_y][temp_x] = '*';
	}
	
	
	monsterPosition.x= temp_x;
	monsterPosition.y= temp_y;
	
	m.position=monsterPosition;
	m.direction='D';
	m.oldAttribute[0] = EMPTY_SPACE;
	m.oldAttribute[1] = EMPTY_SPACE;
	m.oldAttribute[2] = EMPTY_SPACE;
	m.oldChar[0] = ' ';
	m.oldChar[1] = ' ';
	m.oldChar[2] = ' ';
	//resume(create(drawMonster, INITSTK, INITPRIO, "drawMonster",1,m));
	drawMonster(&m);
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
	CHICKEN *chicken = chicken_input;	// initiated chicken;
	// TODO: make that array ina the size of (chicken->level)*3
	int eggs_layed = chicken->level * 3;
	int lay_egg_flag = 1;				// laying egg flag;
	int stage_level = 250;				// lay egg every 2.5 sec
	int init_egg_laying_position_y = chicken->position.y + 1  ;
	int init_egg_laying_position_x = chicken->position.x  ;
	int max_enemies[MAX_ENEMIES];
	int fall_time;
	int temp_tod = tod;
	int egg_tod = tod;

	while (1){
		if (abs(tod - temp_tod) >= 100){	// game run at 1000hz -> if term is True, then a mili-second has passed.
			
			display_background[chicken->position.y][chicken->position.x]= ' ';
			display_background[chicken->position.y][chicken->position.x-1]= ' ';
			display_background[chicken->position.y][chicken->position.x+1]= ' ';
			move_chicken(chicken);
			display_background[chicken->position.y][chicken->position.x]= '^';
			display_background[chicken->position.y][chicken->position.x-1]= '^';
			display_background[chicken->position.y][chicken->position.x+1]= '=';
			temp_tod = tod;
		}
		
		if (lay_egg_flag == 1 && abs(tod - egg_tod) >= 2500){	// every stage has its allowed num of monsters/eggs
			//lay_egg(chicken);
			lay_egg_flag = 0;	// dissable before laying egg.
			init_egg_laying_position_y = chicken->position.y ;
			init_egg_laying_position_x = chicken->position.x ;
			if(eggs_layed > 0){	// dont lay more eggs than stage x allow.
				display_background[init_egg_laying_position_y+1][init_egg_laying_position_x] = '*';
				resume( max_enemies[eggs_layed--] = create(lay_egg, INITSTK, INITPRIO, "layed_egg", 2, init_egg_laying_position_y,init_egg_laying_position_x) );
			}
			resched();
			egg_tod = tod;
		}
		else if (lay_egg_flag == 0){
			lay_egg_flag = 1 ;
		}
		send(dispid,1);
		
	}
}

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
			display_background_color[7][9] = WALL_COLOR;
			display_background[7][10] = 'B';
			display_background_color[7][10] = WALL_COLOR;

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

 /*------------------------------------------------------------------------
 *  stage_2  --  print stage 2 hard_coded
 *------------------------------------------------------------------------
 */
 void print_stage_2(){
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
					continue;
				}
				else{ 
					if(i == 5 && ((j <=50) || (j>60))){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if(i == 4 && j>67){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if(i == 3 && j>70){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if(i >5 && i<13){
						if (i==12 && j<10){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if (i==11 && j<9){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if (i==10 && j<8){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if (i==9 && j<7){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if (i==8 && (j<6 || j>45)){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if (i==7 && j>50){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if (i==6 && j>55){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						
					}
					if(i == 10 && (j >=30)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if (i== 13 || i==14 || i==15){
						if(j<40 || j>60){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if(i==13 && j>=45){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
					}

					if ((i== 15) && (j>=40 && j<55)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if ((i==16 && j>59) ||(i==17 && j>58)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}

					if ((i== 18) && (j<20 || j>50)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if ((i== 19) && (j>35 && j<43)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if ((i== 20) && (j>30 && j<55)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if ((i== 21) && (j>20 && j<60)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					
					if (i> 21) {
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
				}

					display_background_color[i][j] = EMPTY_SPACE;
			}

			// Print stage hearts.
			// by the function: pos = 2*(i*80 + j);
			// heart 1
			display_background[7][9] = '<';
			display_background_color[7][9] = WALL_COLOR;
			display_background[7][10] = 'B';
			display_background_color[7][10] = WALL_COLOR;

			// heart 2
			display_background[17][55] = '<';
			display_background_color[17][55] = HEARTCOLLOR;
			
			display_background[17][56] = 'B';
			display_background_color[17][56] = HEARTCOLLOR;
			
			// heart 3
			display_background[21][9] = '<';
			display_background_color[21][9] = HEARTCOLLOR;
			
			display_background[21][10] = 'B';
			display_background_color[21][10] = HEARTCOLLOR;
	
			
			drawChack();
		}
		send(dispid,1);
 }
 /*------------------------------------------------------------------------
 *  stage_3  --  print stage 3 hard_coded
 *------------------------------------------------------------------------
 */
 void print_stage_3(){
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
					continue;
				}
				else{ 
					if(i == 5 && ((j <=50) || (j>60))){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if(i == 4 && j>67){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if(i == 3 && j>70){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if(i >5 && i<13){
						if (i==12 && j<10){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if (i==11 && j<9){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if (i==10 && j<8){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if (i==9 && j<7){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if (i==8 && (j<6 || j>45)){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if (i==7 && j>50){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if (i==6 && j>55){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						
					}
					if(i == 10 && (j >=30)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if (i== 13 || i==14 || i==15){
						if(j<40 || j>60){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
						if(i==13 && j>=45){
						display_background_color[i][j] = WALL_COLOR;
						continue;
						}
					}

					if ((i== 15) && (j>=40 && j<55)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if ((i==16 && j>59) ||(i==17 && j>58)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}

					if ((i== 18) && (j<20 || j>50)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if ((i== 19) && (j>35 && j<43)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if ((i== 20) && (j>30 && j<55)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					if ((i== 21) && (j>20 && j<60)){
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
					
					if (i> 21) {
					display_background_color[i][j] = WALL_COLOR;
					continue;
					}
				}

					display_background_color[i][j] = EMPTY_SPACE;
			}

			// Print stage hearts.
			// by the function: pos = 2*(i*80 + j);
			// heart 1
			display_background[7][9] = '<';
			display_background_color[7][9] = WALL_COLOR;
			display_background[7][10] = 'B';
			display_background_color[7][10] = WALL_COLOR;

			// heart 2
			display_background[17][55] = '<';
			display_background_color[17][55] = HEARTCOLLOR;
			
			display_background[17][56] = 'B';
			display_background_color[17][56] = HEARTCOLLOR;
			
			// heart 3
			display_background[21][9] = '<';
			display_background_color[21][9] = HEARTCOLLOR;
			
			display_background[21][10] = 'B';
			display_background_color[21][10] = HEARTCOLLOR;
	
			
			drawChack();
		}

		send(dispid,1);
		resume( platform_3_pid = create(stage_3_platform, INITSTK, INITPRIO, "STAGE3PLAT1", 0) );
		resume( platform_3_pid1 = create(stage_3_platform2, INITSTK, INITPRIO, "STAGE3PLAT2", 0) );
 }
 void platform(int min,int max,int hight,int movment,int left){	//min==1 max==20
	int plat_tod;
	int go_back=left;
	int max_mov=0;
	plat_tod=tod;

	if (left==1) max_mov = movment;


	while (1){
		if (abs(tod-plat_tod) >= 1000){
			
			if (go_back==0){
				display_background_color[hight][min] = EMPTY_SPACE;
				display_background_color[hight][max] = WALL_COLOR;
				min++;
				max++;
				max_mov++;
			}
			else{
				display_background_color[hight][min] = WALL_COLOR;
				display_background_color[hight][max] = EMPTY_SPACE;
				min--;
				max--;
				max_mov--;
			}
			if(max_mov==movment){
				go_back = 1;
				min--;
				max--;
			}
			if(max_mov==0){
				go_back = 0;
				min++;
				max++;
			}	
			plat_tod = tod;
			send(dispid,1);	
		}
	} 
 }
 void stage_3_platform(){	//min==1 max==19 hight 18 movment 5
		int min =1;
		int max = 20;
		int hight = 18;
		int movment = 5;
		int left = 0 ; 
		platform(min,max,hight,movment,left);
	}
void stage_3_platform2(){	
		int min =30;
		int max = 78;
		int hight = 10;
		int movment = 7;
		int left = 1;
		platform(min,max,hight,movment,left);
	}


  void stage_1(){
	// create a chicken.
	CHICKEN *chicken;
	POSITION *chickenPosition;
	int chicken_pid;
	MONSTER *m;
	POSITION *monsterPosition;
	
	print_stage_1();	// print stage on the screen
	 
	 // initiate chicken.
	chickenPosition=(POSITION *)malloc(sizeof(POSITION));
	chickenPosition->x=7;
	chickenPosition->y=2;
	chicken->position=*chickenPosition;
	chicken->level = 1;

	resume( chicken_pid = create(draw_chicken, INITSTK, INITPRIO, "CHICKEN_DRAWER", 1, chicken) );
	
	  
 }
void stage_2(){
	// create a chicken.
	CHICKEN *chicken;
	POSITION *chickenPosition;
	int chicken_pid;
	MONSTER *m;
	POSITION *monsterPosition;
	
	print_stage_2();	// print stage on the screen
	 
	 // initiate chicken.
	chickenPosition=(POSITION *)malloc(sizeof(POSITION));
	chickenPosition->x=7;
	chickenPosition->y=2;
	chicken->position=*chickenPosition;
	chicken->level = 1;

	resume( chicken_pid = create(draw_chicken, INITSTK, INITPRIO, "CHICKEN_DRAWER", 1, chicken) );
	
	  
 }
void stage_3(){
	// create a chicken.
	CHICKEN *chicken;
	POSITION *chickenPosition;
	int chicken_pid;
	MONSTER *m;
	POSITION *monsterPosition;
	
	print_stage_3();	// print stage on the screen
	 
	 // initiate chicken.
	chickenPosition=(POSITION *)malloc(sizeof(POSITION));
	chickenPosition->x=7;
	chickenPosition->y=2;
	chicken->position=*chickenPosition;
	chicken->level = 1;

	resume( chicken_pid = create(draw_chicken, INITSTK, INITPRIO, "CHICKEN_DRAWER", 1, chicken) );	  
 }


/*------------------------------------------------------------------------
*  stage_0 MENU  --  print stage 0 hard_coded
*------------------------------------------------------------------------
*/
 void print_stage_0(){
	int i,j,temp_j,pos;
	int hole_flag =	0;
	
	for(i = 0; i < 25; i++ )
	{
		for(j = 0; j < 80; j++)
		{
			pos = 2*(i*80 + j);
			// print stage rounding square
			display_background_color[i][j] = EMPTY_SPACE;

			if( i ==0 || j == 0 || i ==24 || j==79){
				display_background_color[i][j] = WALL_COLOR;
			}
		}
	}
	write_string(10, 30, 185,"Chack   POP!");
	write_string(13, 30, 20,"Press enter to begin...");
	send(dispid,1);
 }


void stage_0(){
	print_stage_0();	// print stage on the screen
}

void stage_manager(){
 	int stage_number;
	while(1){
		stage_number = receive();
		switch (stage_number){
		case 0://	activate stage 0, menu
			resume( stage_0_pid);
			break;
		case 1://	activate stage 1
			resume( stage_1_pid);
			break;	
		case 2://	activate stage 2
			resume( stage_2_pid);
			break;
		case 3://	activate stage 3
			resume( stage_3_pid);
			break;
		}
	}
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
			   signal(displayer_sem);
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


void updateter()
{
	int pressed,scan;
	while(1)
	{
		pressed = receive();
		scan = pressed;
		if ((scan == 30)) // 'A' pressed
			moveChack('L');
		else if ((scan == 32)) // 'D' pressed
			moveChack('R');
		else if ((scan == 17)) // 'W' pressed
			moveChack('U');
		else if ((scan == 31)) // 'S' pressed
		{
			moveChack('D');
		}
		else if (scan == 18)	// 'E' pressed
		{
			resume( create(throw_granade, INITSTK, INITPRIO, "Granade", 1,0) ); // Throw granade right.
		}
		else if (scan == 16)	// 'Q' pressed
		{
			resume( create(throw_granade, INITSTK, INITPRIO, "Granade", 1,1) ); // Throw granade left.
		}
		else if (scan ==0x1C && current_stage == 0){
			current_stage = 1;
			send(stage_manager_pid, current_stage);
		}
	}
} // updater 

void sound()
{
	
	while (1)
	{
		Sound(800);
		sleept(500);
		Sound(700);
		sleept(500);
		Sound(650);
		sleept(500);
		Sound(600);
		sleept(500);
		Sound(400);
		sleept(500);
		Sound(500);
		sleept(350);
		Sound(600);
		sleept(600);
		Sound(500);
		sleept(700);
		Sound(400);
		sleept(450);
		Sound(300);
		sleept(300);
		Sound(400);
		sleept(100);
		
		
		/*good for something*/
		//Sound(1100);
		//sleept(50);
		
		/*
		Sound(120);
		sleept(500);
		Sound(77);
		sleept(500);
		Sound(90);
		sleept(500);
		Sound(110);
		sleept(500);
		Sound(120);
		sleept(500);
		Sound(500);
		sleept(500);
		Sound(77);
		sleept(800);
		Sound(110);
		sleept(500);
		Sound(120);
		sleept(800);
		Sound(500);
		sleept(500);
		Sound(77);
		sleept(800);
		Sound(110);
		sleept(500);
		Sound(120);
		sleept(500);
		Sound(70);
		sleept(500);
		Sound(77);
		sleept(800);
		*/
	}
	NoSound();
	return(0);
}


int sched_arr_pid[10] = {-1};
int sched_arr_int[10] = {-1};


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
	char *test = strdup("chack_name");
	setLatch(1193);		// for working in 1000hz +-
	SetScreen();		//intiate screen mode
	

    resume( dispid = create(displayer, INITSTK, INITPRIO, "DISPLAYER", 0) );
    resume( recvpid = create(receiver, INITSTK, INITPRIO+3, "RECIVEVER", 0) );
	resume( uppid = create(updateter, INITSTK, INITPRIO, "UPDATER", 0) );
	
	// Init stages, and stages_manager.
	resume( stage_manager_pid = create(stage_manager, INITSTK, INITPRIO, "stage_manager", 0) );
	stage_0_pid = create(stage_0, INITSTK, INITPRIO, "MENU", 0);
	stage_1_pid = create(stage_1, INITSTK, INITPRIO, "STAGE1", 0);
	stage_2_pid = create(stage_2, INITSTK, INITPRIO, "STAGE2", 0);
	stage_3_pid = create(stage_3, INITSTK, INITPRIO, "STAGE3", 0);
    resume(sound_id = create(sound, INITSTK, INITPRIO, "sound", 0));
	
	receiver_pid =recvpid;  
    set_new_int9_newisr();
	
	send(stage_manager_pid,0);		// activate stage_manager;

	chackPosition=(POSITION *)malloc(sizeof(POSITION));
	chackPosition->x=7;
	chackPosition->y=2;

	chack->name = test;
	chack->position=*chackPosition;
	chack->life = NUMOFLIFES;
	chack->gravity=1;
		
    schedule(7,57, dispid, 0,  uppid, 29, stage_manager_pid, 30, stage_0_pid, 32, stage_1_pid, 34, stage_2_pid, 36,stage_3_pid, 38);
} // xmain	
