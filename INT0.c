/*----------------------------------------------------------------------------
 * Name:    INT0.c
 * Purpose: button
 * Note(s):
 *----------------------------------------------------------------------------

 *----------------------------------------------------------------------------*/

#include "LPC17xx.H"                         /* LPC17xx definitions           */
#include "INT0.h"
#include <RTL.h>  

/*----------------------------------------------------------------------------
  initialize LED Pins
 *----------------------------------------------------------------------------*/
volatile int button_pressed = 0;

void INT0_Init (void) {

	// P2.10 is related to the INT0 or the push button.
	// P2.10 is selected for the GPIO 
	LPC_PINCON->PINSEL4 &= ~(3<<20); 

	// P2.10 is an input port
	LPC_GPIO2->FIODIR   &= ~(1<<10); 

	// P2.10 reads the falling edges to generate the IRQ
	// - falling edge of P2.10
	LPC_GPIOINT->IO2IntEnF |= (1 << 10);

	// IRQ is enabled in NVIC. The name is reserved and defined in `startup_LPC17xx.s'.
	// The name is used to implemet the interrupt handler above,
	NVIC_EnableIRQ( EINT3_IRQn );
	
	

}
// INT0 interrupt handler

void EINT3_IRQHandler() {

	// Checks whether the interrupt is called on the falling edge
	if ( LPC_GPIOINT->IO2IntStatF && (0x01 << 10) ) 
	{
		LPC_GPIOINT->IO2IntClr |= (1 << 10); // clear interrupt condition
		
		//set flag

		button_pressed = 1;
		
	}
}
