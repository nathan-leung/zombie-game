//Joystick.h

#define KBD_MASK 0x79
#define BUTTON_MASK 0x1
#define POSITION_MASK 0xF


#define LEFT_POS 	0
#define UP_POS 		1
#define RIGHT_POS 2
#define DOWN_POS 	3

#define LEFT_BIT 	1 << 0
#define UP_BIT 		1 << 1
#define RIGHT_BIT 1 << 2
#define DOWN_BIT 	1 << 3

extern void JOYSTICK_Init(void);
extern uint8_t JOYSTICK_Status(void);

extern uint8_t JOYSTICK_Button_Read(void);
extern uint8_t JOYSTICK_Position_Read(void);

