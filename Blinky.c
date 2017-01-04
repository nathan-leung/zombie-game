/*----------------------------------------------------------------------------
 * Name:    Blinky.c
 * Purpose: LED Flasher
 * Note(s): possible defines set in "options for target - C/C++ - Define"
 *            __USE_LCD   - enable Output on LCD
 *----------------------------------------------------------------------------
 * This file is part of the uVision/ARM development tools.
 * This software may only be used under the terms of a valid, current,
 * end user licence from KEIL for a compatible version of KEIL software
 * development tools. Nothing else gives you the right to use this software.
 *
 * This software is supplied "AS IS" without warranties of any kind.
 *
 * Copyright (c) 2008-2011 Keil - An ARM Company. All rights reserved.
 *----------------------------------------------------------------------------*/
#include <RTL.h>
#include <stdio.h>
#include "LPC17xx.H"                         /* LPC17xx definitions           */
#include "GLCD.h"
#include "Serial.h"
#include "LED.h"
#include "ADC.h"
#include "flags.h"
#include "JOYSTICK.h"
#include "uart.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include "INT0.h"



/****************** TO DO LIST **********************/
//Add mutex for all GLCD calling
//Add INT0 Button implementation
//Fix Zombies that aren't being erased when killed
//Make Zombies spawn in the corners
//Change zombie/human to stack memory instead of heap
//Check if mutexes are needed for assignments
//Switch Timer to RIT timer
//Check if its okay to unblock each task's semaphore individually (essentially making the program sequential)
//A semaphore array cannot have its address set.
//Instead of shifting the arrays, copy a zombie into a different
//Make a "draw task" dedicated to drawing everything? (Separates drawing from game logic)
//Score
//Game over screen
//draw floor
//zombie collision

/***************** BUGS ******************************/
//CRASHES AT > 4 zombies

//zombie clears rect goes over other
//double zombie spawn

// sometimes zombie picture doesnt get removed / can be fixed by clearing whole screen
// clock slowdown?

/***************** MACROS ************************/
#define __FI        1                       /* Font index 16x24               */

#define HUMAN_WIDTH		10
#define HUMAN_HEIGHT	10

#define GUN_WIDTH 5
#define GUN_HEIGHT 5

#define Z_ARM_WIDTH	5
#define Z_ARM_HEIGHT	5

#define Z_BODY_WIDTH	10
#define Z_BODY_HEIGHT	10

#define ZOMBIE_WIDTH	((Z_ARM_WIDTH) * 2 + (Z_BODY_WIDTH))
#define ZOMBIE_HEIGHT ((Z_ARM_HEIGHT) * 2 + (Z_BODY_HEIGHT))

#define PICKUP_WIDTH 	7
#define PICKUP_HEIGHT	7

#define HUMAN_AREA 	100
#define GUN_AREA 		25
#define CLEAR_AREA 	400
#define Z_BODY_AREA (Z_BODY_WIDTH)*(Z_BODY_HEIGHT)
#define Z_ARM_AREA (Z_BODY_WIDTH)*(Z_BODY_HEIGHT)
#define ZOMBIE_AREA 200
#define PICKUP_AREA 49

#define MAX_ZOMBIES 3
#define MAX_PICKUPS 10

#define BOMB_RANGE 50.0
#define BOMB_D_HEIGHT 10
#define BOMB_D_WIDTH 10
#define BOMB_D_AREA 100

#undef PRINT_ENABLE
#undef PRINT_ENABLE_LOOPS


/******************* STRUCTS *******************/

typedef struct {
	int x_pos;
	int y_pos;
	//int width;
	//int height;
	
	int speed;
	int lives;
	
} human_t;


typedef struct {
	uint16_t x_pos;
	uint16_t y_pos;
	//int height;
	
	float speed;
} zombie_t;


typedef struct {
	uint16_t x_pos;
	uint16_t y_pos;
	
	int type;

} pickup_t;

//TODO: Maybe a struct which holds the array of zombies AND num_zombies variable

/****************** GLOBAL VARIABLES *******************/

//BITMAPS
unsigned short human_map[HUMAN_AREA];
unsigned short gun_map[GUN_AREA];
unsigned short clear_rect_map[CLEAR_AREA]; // change this to clear human/gun/zombie
unsigned short z_arm_map[Z_ARM_AREA];
unsigned short z_body_map[Z_BODY_AREA];
unsigned short zombie_map[ZOMBIE_AREA];
unsigned short pickup_map[PICKUP_AREA];
unsigned short bomb_map[] = {0,0,1,1,1,1,1,0,0,
														 0,1,1,2,2,2,1,1,0,
														 1,1,2,2,3,2,2,1,1,
														 1,2,2,3,3,3,2,2,1,
														 1,2,3,3,3,3,3,2,1,
														 1,2,2,3,3,3,2,2,1,
														 1,1,2,2,3,2,2,1,1,
														 0,1,1,2,2,2,1,1,0,
														 0,0,1,1,1,1,1,0,0};
unsigned short bomb_w_map[BOMB_D_AREA];
unsigned short bomb_r_map[BOMB_D_AREA];
unsigned short bomb_o_map[BOMB_D_AREA]; 
unsigned short bomb_y_map[BOMB_D_AREA]; 

//VARIABLES
//TODO make zombies_array and human NOT pointers with heap memory
volatile zombie_t zombies_array[MAX_ZOMBIES];
OS_TID zombie_tasks[MAX_ZOMBIES]; //Task IDs for each zombie_task
volatile human_t human;
volatile int num_zombies = 0;
volatile pickup_t pickups_array[MAX_PICKUPS]; 
int lives_left = 0;


//MUTEXES AND SEMAPHORES

	//Variable Mutexes
	OS_MUT human_mut; // 'human'
	OS_MUT zombies_mut; // 'zombies_array'
	OS_MUT num_zombies_mut; // 'num_zombies'
	
	//Peripheral Mutexes
	OS_MUT led_mut; 
	OS_MUT joystick_mut;
	OS_MUT GLCD_mut;
	
	//Semaphores to block tasks
	OS_SEM human_task_sem; // ' human_task'
	OS_SEM zombie_task_sem[MAX_ZOMBIES]; // 'zombie_task'
	OS_SEM button_sem;
	
	


/******************* FUNCTIONS *********************/

// Kills a specific zombie
void kill_zombie(int z_index){
	int i;
	#ifdef PRINT_ENABLE
	printf("Killing zombie #");
	for(i=0; i< z_index; i++){
		printf("1");
	}
	printf("\nout of ");
	for(i = 0; i< num_zombies; i++) {
		printf("1");
	}
	printf("\n");
	#endif
	
	if(z_index < num_zombies){
		zombie_t dead_zombie;
		
		os_mut_wait(&zombies_mut, 0xffff);
			dead_zombie = zombies_array[z_index];
		os_mut_release(&zombies_mut);
		
		os_mut_wait(&GLCD_mut, 0xffff);
		GLCD_Bitmap (dead_zombie.x_pos, dead_zombie.y_pos, ZOMBIE_WIDTH, ZOMBIE_HEIGHT, (unsigned char *)clear_rect_map);
		os_mut_release(&GLCD_mut);
		
		
		if(z_index != num_zombies-1){
			os_mut_wait(&zombies_mut, 0xffff);
				zombies_array[z_index] = zombies_array[num_zombies-1];
			os_mut_release(&zombies_mut);
			
			os_mut_wait(&num_zombies_mut, 0xffff);
				num_zombies--;
			os_mut_release(&num_zombies_mut);

			os_tsk_delete(zombie_tasks[num_zombies]);
			return;
		} else if (z_index != 0) {
			printf("Case 2");			os_mut_wait(&zombies_mut, 0xffff);
			os_mut_release(&zombies_mut);
			
			os_mut_wait(&num_zombies_mut, 0xffff);
				num_zombies--;
			os_mut_release(&num_zombies_mut);

			os_tsk_delete(zombie_tasks[num_zombies]);
			return;
		} else {
			int first_zombie_index;
			os_mut_wait(&num_zombies_mut, 0xffff);
				num_zombies--;
			os_mut_release(&num_zombies_mut);
			
			os_mut_wait(&GLCD_mut, 0xffff);
				GLCD_Bitmap (zombies_array[0].x_pos, zombies_array[0].y_pos, ZOMBIE_WIDTH, ZOMBIE_HEIGHT, (unsigned char *)clear_rect_map);
			os_mut_release(&GLCD_mut);
			
			
			//TODO replace this with a free to the zombie, deleting task, intializing new zombie, and restarting task?
			os_mut_wait(&zombies_mut, 0xffff);
				first_zombie_index = zombie_init();
			os_mut_release(&zombies_mut);
			
			if(first_zombie_index != 0){
				//ERROR!!
			}
			return;
			
		}
		
	}	
}

//Detects if the human is touching one of the zombies
void detect_collision( void ){
	int i;
	human_t local_human;
	zombie_t local_zombie;
	
	os_mut_wait(&human_mut, 0xffff);
	local_human = human;
	os_mut_release(&human_mut);

	
	for(i=0; i<num_zombies; i++){
		int j;
		
		os_mut_wait(&zombies_mut, 0xffff);
		local_zombie = zombies_array[i];
		os_mut_release(&zombies_mut);

		if( local_zombie.x_pos < local_human.x_pos + HUMAN_WIDTH && local_zombie.x_pos > local_human.x_pos - ZOMBIE_WIDTH 
			&& local_zombie.y_pos > local_human.y_pos - ZOMBIE_HEIGHT && local_zombie.y_pos < local_human.y_pos + HUMAN_HEIGHT){	
			
				#ifdef PRINT_ENABLE
					printf("Collision Detected!\n");
				#endif
				
				if(local_human.lives > 0){
					//kill all zombies but 1
					//reset human and zombie coordinates						
					//OR
					//human explodes and loses a life
					//zombies continue
					
				#ifdef PRINT_ENABLE
					printf("Running Algorithm\n");
				#endif
					
					
					//Clears the human
					os_mut_wait(&GLCD_mut, 0xffff);
						GLCD_Bitmap (local_human.x_pos - GUN_WIDTH, local_human.y_pos-  GUN_WIDTH, 20, 20, (unsigned char *)clear_rect_map);
					os_mut_release(&GLCD_mut);
					 
					
					//Reset human coordinates
					os_mut_wait(&human_mut, 0xffff);
						human.x_pos = 10;
						human.y_pos = 10;
						--(human.lives);
					os_mut_release(&human_mut);
					
				#ifdef PRINT_ENABLE
					printf("Killing Zombies...\n");
				#endif
					//Kill all the zombies but the first
					for( j= num_zombies-1 ; j >= 0; j--){
						kill_zombie(j);
					}
					#ifdef PRINT_ENABLE
						printf("Done Killing\n");
					#endif
					
					//Clear the first zombie and rest its coordinates/stats
// 					os_mut_wait(&zombies_mut, 0xffff);
// 						#ifdef PRINT_ENABLE
// 							printf("Got the zombies mutex\n");
// 						#endif
// 					
// 					
// 						
// 						os_mut_wait(&GLCD_mut, 0xffff);
// 						GLCD_Bitmap (zombies_array[0].x_pos, zombies_array[0].y_pos, ZOMBIE_WIDTH, ZOMBIE_HEIGHT, (unsigned char *)clear_rect_map);
// 						os_mut_release(&GLCD_mut);
// 					
// 						#ifdef PRINT_ENABLE
// 							printf("Cleared bitmap\n");
// 						#endif	
// 						zombies_array[0].x_pos = 100;
// 						zombies_array[0].y_pos = 100;
// 						zombies_array[0].speed = 3.0;
// 					#ifdef PRINT_ENABLE
// 							printf("Set values\n");
// 						#endif	
// 					os_mut_release(&zombies_mut);
					
					#ifdef PRINT_ENABLE
						printf("Erased and reset first zombie\n");
					#endif
					
					//Turn off an LED to indicate a life lost
					LED_Off(local_human.lives-1);
				} else {
					//Game over
				}
				return;
			}
	}
	
}

//Initializes the human
void human_init(void){
		os_mut_wait(&human_mut, 0xffff);
		human.x_pos = 10;
		human.y_pos = 10;
		human.speed = 10;
		human.lives = 8;
		os_mut_release(&human_mut);
}




//returns quadrant that the human is in
// (320, 0)         (320,230)
// X----------------X
// |				|				|
// |				|				| 
// |		2		|		3		| 
// |				|				| 
// |________|_______| 
// |				|				| 
// |				|				| 
// |		1		|		4		| 
// |				|				| 
// |				|				| 
// X----------------X
// (0,0)            (0, 240)
int get_human_quadrant(void){
	human_t local_human;
	
	os_mut_wait(&human_mut, 0xffff);
	local_human = human;
	os_mut_release(&human_mut);
	
	if(local_human.x_pos <= 160)
		if(local_human.y_pos <= 120)
			return 1;
		else
			return 4;
	else
		if(local_human.y_pos <= 120)
			return 2;
		else
			return 3;
	
}

//Initializes a zombie
//Returns the index of the new zombie, or -1 if the maximum number of zombies are on screen
signed int zombie_init(void){
		int local_num_zombies;
		local_num_zombies = num_zombies;
		if(num_zombies < MAX_ZOMBIES){
			int quadrant = get_human_quadrant();
			
			os_mut_wait(&zombies_mut, 0xffff);
				//zombies_array[num_zombies] = (zombie_t *) malloc(sizeof(zombie_t));
				//TODO change x and y positions so that zombies spawn randomly in one of the four corners
				//make sure that the player is not in one of the corners that they will spawn
				if(quadrant == 1 || quadrant == 4)
					zombies_array[num_zombies].x_pos = 310;
				else
					zombies_array[num_zombies].x_pos = 10;
				if(quadrant == 1 || quadrant == 2)
					zombies_array[num_zombies].y_pos = 230;
				else
					zombies_array[num_zombies].y_pos = 10;
				
				zombies_array[num_zombies].speed = 3.0;
			os_mut_release(&zombies_mut);
			
			os_mut_wait(&num_zombies_mut, 0xffff);
				num_zombies++;
			os_mut_release(&num_zombies_mut);
		
		return local_num_zombies;
	}
	return -1; //No more zombies can spawn
	
}

/************************** TASKS **************************/


//Human task
__task void human_task( void* void_ptr ) {
	unsigned int joy_status;
	human_t prev_human;
	int x, y;
	
	printf("Human Task\n");
	#ifdef PRINT_ENABLE
		printf("Human Task\n");
	#endif
	

	while(1){
		#ifdef PRINT_ENABLE_LOOPS
			printf("Human Task is WAITING!\n");
		#endif
		
		os_sem_wait(&human_task_sem, 0xffff);
		#ifdef PRINT_ENABLE_LOOPS
			printf("Human Task!\n");
		#endif
		
		//Update human position
		
		
		os_mut_wait(&human_mut, 0xffff);
			prev_human = human;
		os_mut_release(&human_mut);
		
		//Clear current human position
		os_mut_wait(&GLCD_mut, 0xffff);
			GLCD_Bitmap (prev_human.x_pos - GUN_WIDTH, prev_human.y_pos-  GUN_WIDTH, 20, 20, (unsigned char *)clear_rect_map); 
		os_mut_release(&GLCD_mut);
		
		//Get the joystick status
		os_mut_wait(&joystick_mut, 0xffff);
			joy_status = JOYSTICK_Position_Read();
		os_mut_release(&joystick_mut);
		
		//Update position with in accordance with the joystick position
		os_mut_wait(&human_mut, 0xffff);
		if( !((joy_status >> DOWN_POS) & 1) && prev_human.x_pos > 10) human.x_pos -= human.speed;
		if( !((joy_status >> UP_POS) & 1) && prev_human.x_pos < 310) human.x_pos += human.speed;
		if( !((joy_status >> LEFT_POS) & 1) && prev_human.y_pos > 10) human.y_pos -= human.speed;
		if( !((joy_status >> RIGHT_POS) & 1) && prev_human.y_pos < 240) human.y_pos += human.speed;
		x = human.x_pos;
		y = human.y_pos;
		os_mut_release(&human_mut);
		
		//Draw the new human
		os_mut_wait(&GLCD_mut, 0xffff);
		GLCD_Bitmap (x, y, HUMAN_WIDTH, HUMAN_HEIGHT, (unsigned char *)human_map);
		os_mut_release(&GLCD_mut);
		
		
		//Draw the gun
		os_mut_wait(&GLCD_mut, 0xffff);
		if (y > prev_human.y_pos){
				if (x > prev_human.x_pos){
					GLCD_Bitmap (x+HUMAN_WIDTH, y+HUMAN_HEIGHT, GUN_WIDTH, GUN_HEIGHT, (unsigned char *)gun_map); 
				}
				else if (x < prev_human.x_pos){
					GLCD_Bitmap (x-GUN_WIDTH, y+HUMAN_HEIGHT, GUN_WIDTH, GUN_HEIGHT, (unsigned char *)gun_map);
				}
				else {
					GLCD_Bitmap (x + GUN_WIDTH/2, y+HUMAN_HEIGHT, GUN_WIDTH, GUN_HEIGHT, (unsigned char *)gun_map);
				}
			}
			else if (y < prev_human.y_pos){
				if (x > prev_human.x_pos){
					GLCD_Bitmap (x+HUMAN_WIDTH, y - GUN_HEIGHT, GUN_WIDTH, GUN_HEIGHT, (unsigned char *)gun_map); 
				}
				else if (x < prev_human.x_pos){
					GLCD_Bitmap (x - GUN_WIDTH, y - GUN_HEIGHT, GUN_WIDTH, GUN_HEIGHT, (unsigned char *)gun_map);
				}
				else {
					GLCD_Bitmap (x + GUN_WIDTH/2, y - GUN_HEIGHT, GUN_WIDTH, GUN_HEIGHT, (unsigned char *)gun_map);
				}
			}
			else {
				if (x > prev_human.x_pos){
					GLCD_Bitmap (x+HUMAN_WIDTH, y+GUN_HEIGHT/2, GUN_WIDTH, GUN_HEIGHT, (unsigned char *)gun_map); 
				}
				else if (x < prev_human.x_pos){
					GLCD_Bitmap (x-GUN_WIDTH, y + GUN_HEIGHT/2, GUN_WIDTH, GUN_HEIGHT, (unsigned char *)gun_map);
				}
				else {
					GLCD_Bitmap (x-GUN_WIDTH, y-GUN_WIDTH, GUN_WIDTH, GUN_HEIGHT, (unsigned char *)gun_map);
				}
			}
			os_mut_release(&GLCD_mut);			
			
	}
	
}


//Zombie Task
__task void zombie_task( void* void_ptr ){
	
	signed int x_dist;
	signed int y_dist;
	float xy_ratio;
	int delta_y;
	int delta_x;
	int x_old, y_old;
	human_t current_human;
	zombie_t prev_zombie;
	signed int zombie_index = *((signed int *)void_ptr);
	
	#ifdef PRINT_ENABLE
		printf("Zombie Init.\n");
	#endif
	
	// get the index for the parameters
	while(1){
		
		#ifdef PRINT_ENABLE_LOOPS
			printf("Zombie Task is WAITING!\n");
		#endif
		
		
			os_sem_wait(&zombie_task_sem[zombie_index], 0xffff); 
		#ifdef PRINT_ENABLE_LOOPS
			printf("Zombie Task!\n");
		#endif
		

			//Check: Do I need mutexes here?
			os_mut_wait(&zombies_mut, 0xffff);
			prev_zombie = zombies_array[zombie_index];
			os_mut_release(&zombies_mut);
		
			os_mut_wait(&human_mut, 0xffff);
			current_human = human;
			os_mut_release(&human_mut);
			
			os_mut_wait(&GLCD_mut, 0xffff);
 			GLCD_Bitmap (prev_zombie.x_pos, prev_zombie.y_pos, ZOMBIE_WIDTH, ZOMBIE_HEIGHT, (unsigned char *)clear_rect_map);
			os_mut_release(&GLCD_mut);
		
			x_dist = current_human.x_pos - prev_zombie.x_pos;
			y_dist = current_human.y_pos - prev_zombie.y_pos;
			
			x_old = prev_zombie.x_pos;
			y_old = prev_zombie.y_pos;
			if(y_dist!=0){
				xy_ratio = abs(x_dist/y_dist);
				
				delta_y = (int) ((int) prev_zombie.speed )/sqrt(1+xy_ratio*xy_ratio);
				if(y_dist > 0) prev_zombie.y_pos += delta_y;
				else prev_zombie.y_pos -= delta_y;
				
				if(delta_y) delta_x = (int) delta_y*xy_ratio;
				else delta_x = ((int) prev_zombie.speed );
				if(x_dist > 0) prev_zombie.x_pos += delta_x;
				else prev_zombie.x_pos -= delta_x;
			}
			else {
				if(x_dist > 0) prev_zombie.x_pos += ((int) prev_zombie.speed );
				else prev_zombie.x_pos -= ((int) prev_zombie.speed );
			}
			

			os_mut_wait(&GLCD_mut, 0xffff);
			GLCD_Bitmap (prev_zombie.x_pos+Z_ARM_WIDTH, prev_zombie.y_pos+Z_ARM_WIDTH, Z_BODY_WIDTH, Z_BODY_HEIGHT, (unsigned char *)z_body_map); 
		//	GLCD_Bitmap (prev_zombie.x_pos+ZOMBIE_WIDTH, prev_zombie.y_pos+ZOMBIE_HEIGHT, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
			os_mut_release(&GLCD_mut);
			
		os_mut_wait(&GLCD_mut, 0xffff);
		 if (y_old < prev_zombie.y_pos){
				if (x_old > prev_zombie.x_pos){
					GLCD_Bitmap (prev_zombie.x_pos , prev_zombie.y_pos+Z_ARM_HEIGHT+Z_BODY_HEIGHT/4, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
					GLCD_Bitmap (prev_zombie.x_pos+Z_ARM_WIDTH+Z_BODY_WIDTH/4 , prev_zombie.y_pos+Z_ARM_HEIGHT+Z_BODY_HEIGHT, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
				}
				else if (x_old < prev_zombie.x_pos){
					GLCD_Bitmap (prev_zombie.x_pos+Z_ARM_WIDTH+Z_BODY_WIDTH , prev_zombie.y_pos+Z_ARM_HEIGHT+Z_BODY_HEIGHT/4, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
					GLCD_Bitmap (prev_zombie.x_pos+Z_ARM_WIDTH+Z_BODY_WIDTH/4 , prev_zombie.y_pos+Z_ARM_HEIGHT+Z_BODY_HEIGHT, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
				}
				else {
					GLCD_Bitmap (prev_zombie.x_pos+Z_ARM_WIDTH+Z_BODY_WIDTH , prev_zombie.y_pos+Z_ARM_HEIGHT+Z_BODY_HEIGHT, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
					GLCD_Bitmap (prev_zombie.x_pos , prev_zombie.y_pos+Z_ARM_HEIGHT+Z_BODY_HEIGHT, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
				}
			}
			else if (y_old > prev_zombie.y_pos){
				if (x_old > prev_zombie.x_pos){
					GLCD_Bitmap (prev_zombie.x_pos+Z_ARM_WIDTH+Z_BODY_WIDTH/4 , prev_zombie.y_pos, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
					GLCD_Bitmap (prev_zombie.x_pos , prev_zombie.y_pos+Z_ARM_HEIGHT+Z_BODY_HEIGHT/4, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
				}
				else if (x_old < prev_zombie.x_pos){
					GLCD_Bitmap (prev_zombie.x_pos+Z_ARM_WIDTH+Z_BODY_WIDTH/4 , prev_zombie.y_pos, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
					GLCD_Bitmap (prev_zombie.x_pos+Z_ARM_WIDTH+Z_BODY_WIDTH , prev_zombie.y_pos+Z_ARM_HEIGHT+Z_BODY_HEIGHT/4, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
				}
				else {
					GLCD_Bitmap (prev_zombie.x_pos+Z_ARM_WIDTH+Z_BODY_WIDTH , prev_zombie.y_pos, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
					GLCD_Bitmap (prev_zombie.x_pos , prev_zombie.y_pos, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
				}
			}
			else {
				if (x_old > prev_zombie.x_pos){
					GLCD_Bitmap (prev_zombie.x_pos , prev_zombie.y_pos, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
					GLCD_Bitmap (prev_zombie.x_pos , prev_zombie.y_pos+Z_ARM_HEIGHT+Z_BODY_HEIGHT, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
				}
				else if (x_old < prev_zombie.x_pos){
					GLCD_Bitmap (prev_zombie.x_pos+Z_ARM_WIDTH+Z_BODY_WIDTH , prev_zombie.y_pos, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
					GLCD_Bitmap (prev_zombie.x_pos+Z_ARM_WIDTH+Z_BODY_WIDTH , prev_zombie.y_pos+Z_ARM_HEIGHT+Z_BODY_HEIGHT, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
				}
				else {
					GLCD_Bitmap (prev_zombie.x_pos , prev_zombie.y_pos, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
					GLCD_Bitmap (prev_zombie.x_pos , prev_zombie.y_pos+Z_ARM_HEIGHT+Z_BODY_HEIGHT, Z_ARM_WIDTH, Z_ARM_HEIGHT, (unsigned char *)z_arm_map);
				}
			}
			os_mut_release(&GLCD_mut);
			
			os_mut_wait(&zombies_mut, 0xffff);
				zombies_array[zombie_index].x_pos = prev_zombie.x_pos;
				zombies_array[zombie_index].y_pos = prev_zombie.y_pos;
				zombies_array[zombie_index].speed += 0.02 ;
			os_mut_release(&zombies_mut);

	}
		
}

//Controls Button action
__task void button_task( void *void_ptr){
	human_t local_human;
	int i,j,k;
	float total_dist;
	int x_dist;
	int y_dist;
	#ifdef PRINT_ENABLE
			printf("Button Initialized\n");
	#endif
	
	
	
	while(1){
		#ifdef PRINT_ENABLE_LOOPS
			printf("Button Task is WAITING!\n");
		#endif
		
		os_sem_wait(&button_sem, 0xffff);
		
		#ifdef PRINT_ENABLE
			printf("Bomb detonated!\n");
		#endif
		
		os_mut_wait(&human_mut, 0xffff);
		local_human = human;
		os_mut_release(&human_mut);
	
		//Animate explosion
		for (i = 0; i < 81 ; i++){
			if (bomb_map[i] == 0){
				GLCD_Bitmap(local_human.x_pos-BOMB_RANGE+ BOMB_D_WIDTH*(i%9+1) , local_human.y_pos-BOMB_RANGE+BOMB_D_HEIGHT*(floor(i/9)+1) , BOMB_D_WIDTH, BOMB_D_HEIGHT, (unsigned char *)bomb_w_map);
			}
			else if (bomb_map[i] == 1){
				GLCD_Bitmap (local_human.x_pos-BOMB_RANGE+ BOMB_D_WIDTH*(i%9+1) , local_human.y_pos-BOMB_RANGE+BOMB_D_HEIGHT*(floor(i/9)+1) , BOMB_D_WIDTH, BOMB_D_HEIGHT, (unsigned char *)bomb_r_map);
			}
			else if (bomb_map[i] == 2){
				GLCD_Bitmap (local_human.x_pos-BOMB_RANGE+ BOMB_D_WIDTH*(i%9+1) , local_human.y_pos-BOMB_RANGE+BOMB_D_HEIGHT*(floor(i/9)+1) , BOMB_D_WIDTH, BOMB_D_HEIGHT, (unsigned char *)bomb_o_map);				 
			}
			else if (bomb_map[i] == 3){
				GLCD_Bitmap (local_human.x_pos-BOMB_RANGE+ BOMB_D_WIDTH*(i%9+1) , local_human.y_pos-BOMB_RANGE+BOMB_D_HEIGHT*(floor(i/9)+1) , BOMB_D_WIDTH, BOMB_D_HEIGHT, (unsigned char *)bomb_y_map);
			}
		}
				
				
		#ifdef PRINT_ENABLE
			printf("Got human\n");
		#endif	
		
		for( i = num_zombies-1 ; i >= 0 ; i-- ){
			#ifdef PRINT_ENABLE
				printf("Calculating Distance\n");
			#endif	
			
			x_dist = (local_human.x_pos + HUMAN_WIDTH/2) - (zombies_array[i].x_pos + ZOMBIE_WIDTH/2);
			y_dist = (local_human.y_pos + HUMAN_HEIGHT/2) - (zombies_array[i].y_pos + ZOMBIE_HEIGHT/2);
			total_dist = sqrt( x_dist*x_dist + y_dist*y_dist );
			
			if( total_dist < BOMB_RANGE){
					#ifdef PRINT_ENABLE
							printf("Killing In-Range Zombie\n");
					#endif	
					kill_zombie(i);
			}
			
		}
			os_dly_wait(10);
			for (i = 0; i < 81 ; i++){
					GLCD_Bitmap (local_human.x_pos-BOMB_RANGE+ BOMB_D_WIDTH*(i%9+1) , local_human.y_pos-BOMB_RANGE+BOMB_D_HEIGHT*(floor(i/9)+1) , BOMB_D_WIDTH, BOMB_D_HEIGHT, (unsigned char *)bomb_w_map);				 
			}
		

	}
	
	
	
}


//Base task
__task void base_task( void ) {
		int i;
		signed int zombie_index;
		unsigned int zombie_counter = 0;
		unsigned int pickup_counter = 0;
		//initialize all variables

		os_tsk_prio_self( 11 );
	
		#ifdef PRINT_ENABLE
			printf("Initializing...\n");
		#endif
	
		//Initialize semaphores
	  os_sem_init(&human_task_sem, 0);
		os_mut_init(&human_mut);
		os_mut_init(&GLCD_mut);
		os_mut_init(&joystick_mut);
		os_sem_init(&button_sem, 0);
	
		for(i=0; i< MAX_ZOMBIES; i++){
			os_sem_init(&zombie_task_sem[i], 0);
		}
	
		//Initialize Variables
	
	  //Initialize human
		human_init();
	
		
	  //Initialize First Zombie
		zombie_index = zombie_init(); //this will be zero
		
		// start other tasks
		os_tsk_create_ex( human_task, 11, &human );
		zombie_tasks[zombie_index] = os_tsk_create_ex( zombie_task, 11, (void *) &zombie_index );
		os_tsk_create_ex( button_task, 11, &human );
		
		while(1){
			unsigned long int i;
			unsigned long int seed;
			
		#ifdef PRINT_ENABLE_LOOPS
			printf("In Base Loop!\n");
			printf("--\n");
			printf("---\n");
		#endif
			

			
			for(i=0;i < 2000000; i++); //timer
			if(zombie_counter < 50) zombie_counter++;
			else {
				zombie_counter = 0;
				if(num_zombies < MAX_ZOMBIES){
					
					zombie_index = zombie_init(); //num_zombies is incremented in the function
					
					if(zombie_index != -1){
						zombie_tasks[zombie_index] = os_tsk_create_ex( zombie_task, 11, (void *) &zombie_index );
					}
					
				}
			}
			
			if(pickup_counter < 100) pickup_counter++;
			else {
				pickup_counter = 0;
				if(num_zombies < MAX_ZOMBIES){

				}
			}
			//Button Interrupt bomb: Each zombie checks if it is within a certain distance of the human.
			// If they are within the distance, kill themselves.
			
			
			//Post to update position and stats
			os_sem_send(&human_task_sem);
			for(i=0;i< num_zombies; i++){
				#ifdef PRINT_ENABLE_LOOPS
					printf("Sending Zombie Semaphore!\n");
				#endif
				os_sem_send(&zombie_task_sem[i]);
			}
			
			//detect collision between human and zombies
			detect_collision();
			
			if(bomb_detonated){
				#ifdef PRINT_ENABLE_LOOPS
					printf("Sending Bomb Semaphore!\n");
				#endif
				bomb_detonated = 0;
				os_sem_send(&button_sem);
			}
			
			
			
		}
	
	
}


/*----------------------------------------------------------------------------
  Main Program
 *----------------------------------------------------------------------------*/
int main (void) {
	int i;
	int j;
	printf("The peripherals only work if this statement is here.\n");

	
	SystemInit();
	SystemCoreClockUpdate();
	
	LED_Init();                  
  SER_Init();                               
  ADC_Init();
	INT0_Init();
	GLCD_Init(); /* Initialize graphical LCD      */

  GLCD_Clear(White);                         /* Clear graphical LCD display   */
  GLCD_SetBackColor(White);
  GLCD_SetTextColor(Blue);
	
	
	//Initialize shapes
// 	for (i =0;i < 10; i++){
// 		for(j=0; j <20 ; j++){
// 			if(i == 0 || i == 9 || j == 0 || j == 19)
// 			zombie_map[i*j] = Black;
// 			else
// 				zombie_map[i*j] = Red;
// 		}

	for (i = 0; i < 100; i++){
		gun_map[i] = Black;
		human_map[i] = Green;
		bomb_w_map[i] = White;
		bomb_r_map[i] = Red;
		bomb_o_map[i] = 0xFC60;
		bomb_y_map[i] = Yellow;
	}

	for (i =0;i < 10; i++){
		for(j=0; j <10 ; j++){
			human_map[i*j] = DarkGreen;
			if(i == 1)
			z_body_map[i*j] = Black;
			else
				z_body_map[i*j] = DarkGreen;
		}
	}
	
	for (i = 0; i < 5; i++){
		for(j=0 ; j < 5 ; j++){
			if(i == 1)
				z_arm_map[i*j] = Black;
			else
				z_arm_map[i*j] = DarkGreen;
		}
	}


	for (i = 0; i < 10; i++){
		for(j=0 ; j < 10 ; j++){
				human_map[i*j] = DarkGreen;
		}
	}
	for (i = 0; i < 400; i++){
		clear_rect_map[i] = White;
	}
	
	for (i = 0; i < PICKUP_AREA; i++){
		pickup_map[i] = Purple;
	}
	

	
	//Initialize all LEDs to on
	for (i = 0; i < 8; i++){
		LED_On(i);
	}
	#ifdef PRINT_ENABLE
	printf("test");
	#endif
	
	os_sys_init( base_task );

	while ( 1 ) {
		// Endless loop
	}
//   SysTick_Config(SystemCoreClock/10);       /* Generate interrupt each 1 ms */	
}
