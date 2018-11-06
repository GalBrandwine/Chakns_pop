/* game.c - xmain, prntr */

#include <conf.h>
#include <kernel.h>
#include <io.h>
#include <bios.h>
#include <stdio.h>
#include <math.h>
#include <butler.h>
#include <sleep.h>


/*------------------------------------------------------------------------
*  Sound section.
*------------------------------------------------------------------------
*/

 
//ChangeSpeaker - Turn speaker on or off. 
#define ON (1)
#define OFF (0)

void ChangeSpeaker(int status){
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
#define CHACK_COLOR 55
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

int (*old9newisr)(int);
int uppid, dispid, recvpid, stage_manager_pid, stage_0_pid, stage_1_pid, stage_2_pid, stage_3_pid, platform_3_pid, platform_3_pid1,sound_id ;
volatile int global_flag;
volatile int global_timer =0 ;

void interrupt (*old0x70isr)(void);
void platform(int min,int max,int hight,int movment, int left);					//general platform function

void stage_3_platform();		// platform 3 proc
void stage_3_platform2();		// platform 3 proc 2


char display_background [25][80]; //level design
char display_background_color [25][80]; //level design color, for sampeling where the items are in the screen

int point_in_cycle;
int gcycle_length;
int gno_of_pids;

int new_stage =0;					//kill button for monsters and chickens
int chack_alive = 1;				//chack can die if its 1
int kill_world=0;					//shutdown
int kill_monster = 0;				//monster killed chak?
int chicken_pid;
int eggs_layed;
int max_enemies[MAX_ENEMIES];


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
		// Part1: Initialize the display adapter
		nosound();
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
	char oldAttribute;
	char oldChar;

}MONSTER;


POSITION *chackPosition;
CHACK chack;
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


/*------------------------------------------------------------------------
 * Simple coordination drawer.
 *------------------------------------------------------------------------
 */
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
	
	display_background[chack.position.y][chack.position.x] = '^';
	display_background_color[chack.position.y][chack.position.x] =CHACK_COLOR;
	send(dispid, 1);
}

reduce_life(){
	switch (chack.life){
		case 3: 
			write_string(0,1,652,"LIFE:2");
		break;
		case 2:
			write_string(0,1,652,"LIFE:1");
		break;
		case 1:
			write_string(0,1,652,"LIFE:0");
		break;
		//add back to menu
	}
	chack.life--;
}

void kill_chack() {
	if ((display_background_color[chack.position.y][chack.position.x] == GRANADE_SMOKE_COLOR || display_background_color[chack.position.y][chack.position.x + 1] == GRANADE_SMOKE_COLOR || display_background_color[chack.position.y][chack.position.x - 1] == GRANADE_SMOKE_COLOR || kill_monster == 1) && chack_alive == 1) {				//kill chack
		chack_alive = 0;
		display_background[chack.position.y][chack.position.x] = ' ';
		display_background_color[chack.position.y][chack.position.x] = EMPTY_SPACE;
		kill_monster = 0;					//cant be killed again by monster
		display_background[3][4] = '^';//draw new chack
		display_background_color[chack.position.y][chack.position.x] = CHACK_COLOR;
		chack.position.y = 3;	//save new cords
		chack.position.x = 4;
		reduce_life();
		send(dispid, 1);
		if (chack.life == 0) { //ggwp
			new_stage = 1;
			kill_world = 1;		//ggwp
			write_string(11, 25, 100,"________________________________");
			write_string(12, 25, 100, "GAME OVER RESTART AND TRY AGAIN");
			write_string(13, 25, 100,"________________________________");
			send(dispid, 1);		// do it for me imidiatly
		}//gameover
		//REDUCE LIFE+ CHECK IF GAME OVER
	}
}


void moveChack(char side)
{
	int jumpCounter=0;
	display_background[chack.position.y][chack.position.x]= ' ';
	display_background_color[chack.position.y][chack.position.x] = EMPTY_SPACE;
	
	switch (side)
	{
	case 'R'://move  right
		
		 if ((display_background_color[chack.position.y][(chack.position.x + 1)] == FINNISH_GATE) || display_background_color[chack.position.y][(chack.position.x + 1)] == EMPTY_SPACE)//if its not green wall
		{
			display_background[chack.position.y][chack.position.x] = ' ';
			display_background_color[chack.position.y][chack.position.x] = EMPTY_SPACE;
			chack.position.x = (chack.position.x+1)%80;
		}
		else if ((display_background_color[chack.position.y - 1][(chack.position.x + 1)] == EMPTY_SPACE && (display_background_color[chack.position.y - 1][(chack.position.x)] == EMPTY_SPACE)) )//if he can climb the wall
		{
			display_background[chack.position.y][chack.position.x] = ' ';
			display_background_color[chack.position.y][chack.position.x] = EMPTY_SPACE;
			chack.position.y = (chack.position.y - 1) % 25;
			drawChack();
			sleept(1);
			display_background[chack.position.y][chack.position.x] = ' ';
			display_background_color[chack.position.y][chack.position.x] = EMPTY_SPACE;
			chack.position.x = (chack.position.x + 1) % 80;
			drawChack();
		}

		break;
	case 'L'://move left
		
		  if ((display_background_color[chack.position.y][(chack.position.x - 1)] == FINNISH_GATE) || display_background_color[chack.position.y][(chack.position.x - 1)] == EMPTY_SPACE)//if its not green wall
		{
			display_background[chack.position.y][chack.position.x] = ' ';
			display_background_color[chack.position.y][chack.position.x] = EMPTY_SPACE;
			chack.position.x = (chack.position.x-1)%80;
		}
		else
			if (display_background_color[chack.position.y - 1][(chack.position.x - 1)] != WALL_COLOR && (display_background_color[chack.position.y - 1][(chack.position.x)] != WALL_COLOR))//if he can climb the wall
			{
				display_background[chack.position.y][chack.position.x] = ' ';
				display_background_color[chack.position.y][chack.position.x] = EMPTY_SPACE;
				chack.position.y = (chack.position.y - 1) % 80;
				drawChack();
				sleept(1);
				display_background[chack.position.y][chack.position.x] = ' ';
				display_background_color[chack.position.y][chack.position.x] = EMPTY_SPACE;
				chack.position.x = (chack.position.x - 1) % 80;
				drawChack();
			}
			
		break;
	case 'U'://move up
		jumpCounter = 0;
		while (((display_background_color[chack.position.y - 1][(chack.position.x )] == FINNISH_GATE) || display_background_color[chack.position.y - 1][(chack.position.x)] == EMPTY_SPACE) && (jumpCounter<3))
		{
			display_background[chack.position.y][chack.position.x] = ' ';
			display_background_color[chack.position.y][chack.position.x] =EMPTY_SPACE;
			chack.position.y = (chack.position.y - 1) % 25;
			jumpCounter++;
			drawChack();
			sleept(100);
			
		}
		if(display_background_color[chack.position.y-1][(chack.position.x)] == WALL_COLOR){
			chack.gravity=0;
		}
		break;
	case 'D'://move down
		while ((display_background_color[chack.position.y + 1][(chack.position.x )] == FINNISH_GATE) || (display_background_color[chack.position.y + 1][(chack.position.x)] == EMPTY_SPACE))//chack is falling
		{
			display_background[chack.position.y][chack.position.x] = ' ';
			display_background_color[chack.position.y][chack.position.x] = EMPTY_SPACE;
			chack.position.y = (chack.position.y + 1);

			drawChack();
			sleept(100);
		}
		break;
	}
	if ((display_background_color[chack.position.y + 1][(chack.position.x)] != FINNISH_GATE) && (display_background_color[chack.position.y + 1][(chack.position.x)] != WALL_COLOR))
	{
		if( (display_background_color[chack.position.y-1][(chack.position.x)] == WALL_COLOR))
		{
		chack.gravity=0;
		}
		else
		{
		chack.gravity=1;
		}	
	
	}
	while ((chack.gravity == 1) && ((display_background_color[chack.position.y + 1][(chack.position.x )] != FINNISH_GATE) && display_background_color[chack.position.y + 1][(chack.position.x)] != WALL_COLOR  && display_background_color[chack.position.y + 1][(chack.position.x)] != HEARTCOLLOR))//chack is falling
	{
		display_background[chack.position.y][chack.position.x] = ' ';
		display_background_color[chack.position.y][chack.position.x] = EMPTY_SPACE;
		chack.position.y = (chack.position.y + 1);
		drawChack();
		sleept(100);
	}
	chack_alive=1; //cant die if im not alive

	kill_chack();
	if (chack.position.y==23 && chack.position.x==79 ){		//check if stage is over
		new_stage=1;											//kill monsters and chickens
		current_stage++;										//advance to the next stage
		send(stage_manager_pid, current_stage);
	}
	drawChack();
}


/*------------------------------------------------------------------------
 *  Grenede section
 *------------------------------------------------------------------------
 */
 void free_heart(int y, int temp_y, int x, int temp_x){
	/* Function for animating frying hert.
	
	if a heart_id has been freed, than it should go up to the ceiling and make a new gate.
	*/
	int hole_width = 5;
	int temp;

	if(current_stage == 1){	// first stage cheat, i had to do so, because we cant't go upward in first stage.
		for (temp =0 ; temp <= hole_width; temp++){
			display_background_color[23-temp][79]= FINNISH_GATE;	
		}
	}
	else{
		for ( ; temp_y >= 0; temp_y--){
				// Painting the purpul collumn
				if (display_background_color[temp_y][temp_x] != WALL_COLOR){
					display_background_color[temp_y][temp_x] = EMPTY_SPACE - 30;
				}
		}
		for (temp =0 ; temp <= hole_width; temp++){
			// Painting tha FINNISH_GATE
			display_background_color[0][temp_x + temp]= FINNISH_GATE;	
		}
	}
 }


void throw_granade(int direction){
	/* Function for drawing throwen granades.
		
	parameter: direction - 1 throw left, 0 throw right.
	*/

	// Fly 1 seconds, remember that latch suppose to be set to work at 1000hz, 
	// so flying time will be reduced once every 100 ms 10 times, hance a second
	int flying_time = 10;	
	int flying_tod = tod;
	int granade_explode_timer = tod; // sleep 3 sec before exploding;
	int temp_x = chack.position.x;
	int temp_y = chack.position.y;

	int y;
	int x;
	

	while (1){
		if (abs(tod - flying_tod) >= 100){	// print to tscreen granade every 0.1 sec
			
			display_background[temp_y][temp_x] = ' ';
			if( direction == 1 && display_background_color[temp_y][temp_x-3] != WALL_COLOR && display_background_color[temp_y][temp_x+3] != FINNISH_GATE){// move granade left
				temp_x--;
				display_background[temp_y][temp_x] = '-';
				display_background[temp_y][temp_x-1] = '<';
			}
			else if(direction == 0 && display_background_color[temp_y][temp_x+3] != WALL_COLOR && display_background_color[temp_y][temp_x+3] != FINNISH_GATE){// move granade right.
				temp_x++;
				display_background[temp_y][temp_x] = '-';
				display_background[temp_y][temp_x+1] = '>';
			}

			// Update flying time.
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
	// wait for 3 seconds before exploding.
	granade_explode_timer = tod;
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

	// Ugly busy/wait loop.
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
		if ((display_background_color[monster->position.y][(monster->position.x + 1)] != WALL_COLOR))
		{
			if(monster->position.x==chack.position.x && monster->position.y==chack.position.y){
				kill_monster=1;
				kill_chack();
				
			}
			
			monster->oldAttribute = display_background_color[monster->position.y][monster->position.x + 1];
			
			monster->oldChar = display_background[monster->position.y][monster->position.x + 1];


			monster->oldPosition.y = monster->position.y;
			monster->oldPosition.x = monster->position.x;
			display_background[monster->oldPosition.y][monster->oldPosition.x] = monster->oldChar;
			
			;
			display_background_color[monster->oldPosition.y][monster->oldPosition.x] = monster->oldAttribute;
			
			monster->position.x = (monster->position.x + 1) % 80;
			sleept(50);
			
			
		}
		 else
		 {
			 monster->direction =randDirection();
		 }
		break;
	case 'L'://move left
		if ((display_background_color[monster->position.y][(monster->position.x - 1)] != WALL_COLOR))
		{
			if(monster->position.x==chack.position.x && monster->position.y==chack.position.y){
				kill_monster=1;
				kill_chack();
				
			}
			monster->oldAttribute = display_background_color[monster->position.y][monster->position.x - 1];
			
			monster->oldChar = display_background[monster->position.y][monster->position.x - 1];
		


			monster->oldPosition.y = monster->position.y;
			monster->oldPosition.x = monster->position.x;
			
			display_background[monster->oldPosition.y][monster->oldPosition.x] = monster->oldChar;
			
			
			display_background_color[monster->oldPosition.y][monster->oldPosition.x] = monster->oldAttribute;
			
			monster->position.x = (monster->position.x - 1) % 80;
			sleept(50);
			
		}
		else
		 {
			 monster->direction =randDirection();
		 }
		break;
		case 'U'://move up
		if((display_background_color[monster->position.y-1][(monster->position.x)] != WALL_COLOR))
		{
			if(monster->position.x==chack.position.x && monster->position.y==chack.position.y){
				kill_monster=1;
				kill_chack();
				
			}
			monster->oldAttribute = display_background_color[monster->position.y - 1][monster->position.x ];
			
			monster->oldChar = display_background[monster->position.y - 1][monster->position.x];
			

			monster->oldPosition.y = monster->position.y;
			monster->oldPosition.x = monster->position.x;
		
			display_background[monster->oldPosition.y][monster->oldPosition.x] = monster->oldChar;
			
			
			display_background_color[monster->oldPosition.y][monster->oldPosition.x] = monster->oldAttribute;
			
			monster->position.y = (monster->position.y - 1) % 25;

			sleept(50);
		}
		else
		 {
			 monster->direction =randDirection();
		 }
		break;
		case 'D'://move down
			if((display_background_color[monster->position.y+1][(monster->position.x)] != WALL_COLOR))
		{
			if(monster->position.x==chack.position.x && monster->position.y==chack.position.y){
				kill_monster=1;
				kill_chack();
				
			}
			monster->oldAttribute = display_background_color[monster->position.y + 1][(monster->position.x)];
			
			
			monster->oldChar = display_background[monster->position.y + 1][(monster->position.x)];

			monster->oldPosition.y = monster->position.y;
			monster->oldPosition.x = monster->position.x;
			
			display_background[monster->oldPosition.y][monster->oldPosition.x] = monster->oldChar;
			
			
			display_background_color[monster->oldPosition.y][monster->oldPosition.x] = monster->oldAttribute;
			
			monster->position.y = (monster->position.y + 1) % 25;

			sleept(50);
		}
		else
		 {
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
		// kill a monster 
		if (display_background_color[monster->position.y][monster->position.x]== GRANADE_SMOKE_COLOR || display_background[monster->position.y][monster->position.x-1]== GRANADE_SMOKE_COLOR || display_background[monster->position.y][monster->position.x+1]== GRANADE_SMOKE_COLOR){
			kill(getpid());
		}
		if (new_stage == 1) {
			kill(getpid());
		}
		
		write_string(monster->position.y, monster->position.x, MONSTER_COLOR, "M");
		/*
		display_background[monster->position.y][monster->position.x] = 'M';
		display_background_color[monster->position.y][monster->position.x] = MONSTER_COLOR;
		*/

		send(dispid, 1);
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
		if(kill_world==1){						//kill eggs
			display_background[temp_y][temp_x] = ' ';
			break;
		}
			
		//display_background[init_egg_laying_position_y][init_egg_laying_position_x] = '*';

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
		if(kill_world==1){										//kill eggs
			display_background[temp_y][temp_x] = ' ';
			break;
		}
		display_background[temp_y][temp_x] = ' ';
		temp_y ++;
		display_background[temp_y][temp_x] = '*';
	}

	monsterPosition.x = temp_x;
	monsterPosition.y = temp_y;

	m.position = monsterPosition;
	m.direction = 'D';
	m.oldAttribute = EMPTY_SPACE;
	
	m.oldChar = ' ';
	
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
	int temp_eggs_layed = 0;
	temp_eggs_layed = eggs_layed;

	while (1){
		//if (tod%1000 <= 10){	// game run at 1000hz -> if global_timer%1000 is 0, then a second has passed.
		if (abs(tod - temp_tod) >= 100){	// game run at 1000hz -> if global_timer%1000 is 0, then a second has passed.
			
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
			// lay egg	
			lay_egg_flag = 0;	// dissable before laying egg.
			init_egg_laying_position_y = chicken->position.y ;
			init_egg_laying_position_x = chicken->position.x ;
			if(temp_eggs_layed > 0){	// dont lay more eggs than stage x allow.
				display_background[init_egg_laying_position_y+1][init_egg_laying_position_x] = '*';
				resume( max_enemies[temp_eggs_layed--] = create(lay_egg, INITSTK, INITPRIO, "layed_egg", 2, init_egg_laying_position_y,init_egg_laying_position_x) );
			}
			resched();
			egg_tod = tod;
		}
		else if (lay_egg_flag == 0){
			lay_egg_flag = 1 ;
		}
		send(dispid,1);

		if (new_stage==1 ){	//time to die chicken
			display_background[chicken->position.y][chicken->position.x]= ' ';
			display_background[chicken->position.y][chicken->position.x-1]= ' ';
			display_background[chicken->position.y][chicken->position.x+1]= ' ';
			send(dispid,1);
			kill(getpid());
		}
		
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
	char str[7];
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
		// heart 1
		write_string(7,9,HEARTCOLLOR,"<B");

		// heart 2
		write_string(15,55,HEARTCOLLOR,"<B");

		// heart 3
		write_string(19,9,HEARTCOLLOR,"<B");

		
		sprintf(str, "LIFE:%d", chack.life);
		write_string(0,1,652,str);
		write_string(0,8,652,chack.name);
			
			
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
	char str[7];

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
			// heart 1
			write_string(7,9,HEARTCOLLOR,"<B");


			// heart 2
			write_string(17,55,HEARTCOLLOR,"<B");


			// heart 3
			write_string(21,9,HEARTCOLLOR,"<B");

			sprintf(str, "LIFE:%d", chack.life);
			write_string(0,1,652,str);

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
	char str[7];

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
			// heart 1
			write_string(7,9,HEARTCOLLOR,"<B");

			// heart 2
			write_string(17,55,HEARTCOLLOR,"<B");

			// heart 3
			write_string(21,9,HEARTCOLLOR,"<B");

			sprintf(str, "LIFE:%d", chack.life);
			write_string(0,1,652,str);
			
			
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


void stage_0(){//menu stage
	chack.position.x=2;
	chack.position.y=2;
	print_stage_0();	// print stage on the screen
}


void stage_1(){
	// create a chicken.
	CHICKEN *chicken;
	POSITION *chickenPosition;
	int chicken_pid;
	MONSTER *m;
	POSITION *monsterPosition;
	chack.position.x=2;
	chack.position.y=2;
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
	chack.position.x=2;
	chack.position.y=2;
	print_stage_2();	// print stage on the screen
	 
	 // initiate chicken.
	chickenPosition=(POSITION *)malloc(sizeof(POSITION));
	chickenPosition->x=7;
	chickenPosition->y=2;
	chicken->position=*chickenPosition;
	chicken->level = 2;
	new_stage =0; //reset

	resume( chicken_pid = create(draw_chicken, INITSTK, INITPRIO, "CHICKEN_DRAWER", 1, chicken) );
	
	  
 }
void stage_3(){
	// create a chicken.
	CHICKEN *chicken;
	POSITION *chickenPosition;
	int chicken_pid;
	MONSTER *m;
	POSITION *monsterPosition;
	chack.position.x=2;
	chack.position.y=2;
	print_stage_3();	// print stage on the screen
	 
	 // initiate chicken.
	chickenPosition=(POSITION *)malloc(sizeof(POSITION));
	chickenPosition->x=7;
	chickenPosition->y=2;
	chicken->position=*chickenPosition;
	chicken->level = 3;

	resume( chicken_pid = create(draw_chicken, INITSTK, INITPRIO, "CHICKEN_DRAWER", 1, chicken) );
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

 
void displayer( void ){	
	//This process display the matrix, receive message (and return to ready) with every change in the screen matrix
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


void updateter()
{
	int pressed,scan;
	while(1)
	{
		pressed = receive();
		scan = pressed;
		if (kill_world ==1)	//kill the level gg
			kill(getpid());
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


/*------------------------------------------------------------------------
*  Music notes.
*------------------------------------------------------------------------
*/
#define a0          43388       //   27.5000 hz
#define ais0        40953       //   29.1353 hz
#define h0          38655       //   30.8677 hz
#define c1          36485       //   32.7032 hz
#define cis1        34437       //   34.6479 hz
#define d1          32505       //   36.7081 hz
#define dis1        30680       //   38.8909 hz
#define e1          28958       //   41.2035 hz
#define f1          27333       //   43.6536 hz
#define fis1        25799       //   46.2493 hz
#define g1          24351       //   48.9995 hz
#define gis1        22984       //   51.9130 hz
#define a1          21694       //   55.0000 hz
#define ais1        20477       //   58.2705 hz
#define h1          19327       //   61.7354 hz
#define c2          18243       //   65.4064 hz
#define cis2        17219       //   69.2957 hz
#define d2          16252       //   73.4162 hz
#define dis2        15340       //   77.7817 hz
#define e2          14479       //   82.4069 hz
#define f2          13666       //   87.3071 hz
#define fis2        12899       //   92.4986 hz
#define g2          12175       //   97.9989 hz
#define gis2        11492       //  103.8260 hz
#define a2          10847       //  110.0000 hz
#define ais2        10238       //  116.5410 hz
#define h2          9664        //  123.4710 hz
#define c3          9121        //  130.8130 hz
#define cis3        8609        //  138.5910 hz
#define d3          8126        //  146.8320 hz
#define dis3        7670        //  155.5630 hz
#define e3          7240        //  164.8140 hz
#define f3          6833        //  174.6140 hz
#define fis3        6450        //  184.9970 hz
#define g3          6088        //  195.9980 hz
#define gis3        5746        //  207.6520 hz
#define a3          5424        //  220.0000 hz
#define ais3        5119        //  233.0820 hz
#define h3          4832        //  246.9420 hz
#define c4          4561        //  261.6260 hz
#define cis4        4305        //  277.1830 hz
#define d4          4063        //  293.6650 hz
#define dis4        3835        //  311.1270 hz
#define e4          3620        //  329.6280 hz
#define f4          3417        //  349.2280 hz
#define fis4        3225        //  369.9940 hz
#define g4          3044        //  391.9950 hz
#define gis4        2873        //  415.3050 hz
#define a4          2712        //  440.0000 hz
#define ais4        2560        //  466.1640 hz
#define h4          2416        //  493.8830 hz
#define c5          2280        //  523.2510 hz
#define cis5        2152        //  554.3650 hz
#define d5          2032        //  587.3300 hz
#define dis5        1918        //  622.2540 hz
#define e5          1810        //  659.2550 hz
#define f5          1708        //  698.4560 hz
#define fis5        1612        //  739.9890 hz
#define g5          1522        //  783.9910 hz
#define gis5        1437        //  830.6090 hz
#define a5          1356        //  880.0000 hz
#define ais5        1280        //  932.3280 hz
#define h5          1208        //  987.7670 hz
#define c6          1140        // 1046.5000 hz
#define cis6        1076        // 1108.7300 hz
#define d6          1016        // 1174.6600 hz
#define dis6         959        // 1244.5100 hz
#define e6           905        // 1318.5100 hz
#define f6           854        // 1396.9100 hz
#define fis6         806        // 1479.9800 hz
#define g6           761        // 1567.9800 hz
#define gis6         718        // 1661.2200 hz
#define a6           678        // 1760.0000 hz
#define ais6         640        // 1864.6600 hz
#define h6           604        // 1975.5300 hz
#define c7           570        // 2093.0000 hz
#define cis7         538        // 2217.4600 hz
#define d7           508        // 2349.3200 hz
#define dis7         479        // 2489.0200 hz
#define e7           452        // 2637.0200 hz
#define f7           427        // 2793.8300 hz
#define fis7         403        // 2959.9600 hz
#define g7           380        // 3135.9600 hz
#define gis7         359        // 3322.4400 hz
#define a7           339        // 3520.0000 hz
#define ais7         320        // 3729.3100 hz
#define h7           302        // 3951.0700 hz
#define c8           285        // 4186.0100 hz

#define whole_note            1800
#define half_note_dot         whole_note/2 + whole_note/4
#define half_note             whole_note/2
#define quarter_note_dot      whole_note/4 + whole_note/8
#define quarter_note          whole_note/4
#define eighth_note           whole_note/8
#define pause                 30

void sound()
{
	int i;
	int chords[]={g3,quarter_note,c4,quarter_note_dot,h3,eighth_note,c4,quarter_note,e4,quarter_note,d4,quarter_note_dot,c4,eighth_note,d4,quarter_note,e4,eighth_note,d4,eighth_note,c4,quarter_note,c4,quarter_note,e4,quarter_note,g4,quarter_note,a4,half_note_dot,a4,quarter_note,g4,quarter_note_dot,e4,eighth_note,e4,quarter_note,c4,quarter_note,d4,quarter_note_dot,c4,eighth_note,d4,quarter_note,e4,eighth_note,d4,eighth_note,c4,quarter_note_dot,a3,eighth_note,a3,quarter_note,g3,quarter_note,c4,half_note_dot,a4,quarter_note,g4,quarter_note_dot,e4,eighth_note,e4,quarter_note,c4,quarter_note,d4,quarter_note_dot,c4,eighth_note,d4,quarter_note,a4,quarter_note,g4,quarter_note_dot,e4,eighth_note,e4,quarter_note,g4,quarter_note,a4,half_note_dot,a4,quarter_note,g4,quarter_note_dot,e4,eighth_note,e4,quarter_note,c4,quarter_note,d4,quarter_note_dot,c4,eighth_note,d4,quarter_note,e4,eighth_note,d4,eighth_note,c4,quarter_note_dot,a3,eighth_note,a3,quarter_note,g3,quarter_note,c4,half_note_dot};
	
	while (1){
		for(i=0;i<=110;i+=2){
		Sound(chords[i]/40);
		sleept(chords[i+1]);
		}
	}
	NoSound();
	return(0);
}

int sched_arr_pid[10] = {-1};
int sched_arr_int[10] = {-1};


xmain()
{
	char *test;// = "chack_name";


	setLatch(1193);		// for working in 1000hz +-
	SetScreen();		//intiate screen mode
	

    resume( dispid = create(displayer, INITSTK, INITPRIO, "DISPLAYER", 0) );
	resume( uppid = create(updateter, INITSTK, INITPRIO, "UPDATER", 0) );
	
	resume( stage_manager_pid = create(stage_manager, INITSTK, INITPRIO, "stage_manager", 0) );
	stage_0_pid = create(stage_0, INITSTK, INITPRIO, "MENU", 0);
	stage_1_pid = create(stage_1, INITSTK, INITPRIO, "STAGE1", 0);
	stage_2_pid = create(stage_2, INITSTK, INITPRIO, "STAGE2", 0);
	stage_3_pid = create(stage_3, INITSTK, INITPRIO, "STAGE3", 0);
    resume(sound_id = create(sound, INITSTK, INITPRIO, "sound", 0));
	 
    set_new_int9_newisr();
	
	send(stage_manager_pid,0);		// activate stage_manager;

	chackPosition=(POSITION *)malloc(sizeof(POSITION));
	chackPosition->x=7;
	chackPosition->y=2;


	test = malloc(strlen("chack_name")*sizeof(char)+1);
    strcpy(test, "chack_name");
 
	chack.name = test;
	chack.position=*chackPosition;
	chack.life = NUMOFLIFES;
	chack.gravity=1;
	free(test);		
    //schedule(7,100, dispid, 1,  uppid, 30, stage_manager_pid, 60, stage_0_pid, 90);//, stage_1_pid, 120, stage_2_pid, 150, stage_3_pid, 180);
} // xmain	
