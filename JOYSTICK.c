//Joystick.c
#include "LPC17xx.H"  
#include "JOYSTICK.h"

void JOYSTICK_Init (void) {
	LPC_PINCON->PINSEL3 &= ~((3<< 8)|(3<<14)|(3<<16)|(3<<18)|(3<<20)); 
	LPC_GPIO1->FIODIR &= ~((1<<20)|(1<<23)|(1<<24)|(1<<25)|(1<<26));
}

uint8_t JOYSTICK_Status(void){
	uint8_t kbd_val;
	kbd_val = (LPC_GPIO1->FIOPIN >> 20) & KBD_MASK;
	return kbd_val;
	
}

uint8_t JOYSTICK_Button_Read(void){
	uint8_t kbd_val;
	kbd_val = (LPC_GPIO1->FIOPIN >> 20) & BUTTON_MASK;
	return kbd_val;
	
}


uint8_t JOYSTICK_Position_Read(void){
		uint8_t kbd_val;
	kbd_val = (LPC_GPIO1->FIOPIN >> 23) & POSITION_MASK;
	return kbd_val;	
}