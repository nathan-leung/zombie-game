/*----------------------------------------------------------------------------
 * Name:    INT0.h
 * Purpose: button
 * Note(s):
 *----------------------------------------------------------------------------

 *----------------------------------------------------------------------------*/


#include <RTL.h>



#ifndef __INT0_H
#define __INT0_H

/* LED Definitions */
extern uint16_t INT0_val;
extern uint8_t INT0_done;
extern void INT0_Init(void);
extern void EINT3_IRQHandler(void);

extern volatile int button_pressed;

#endif
