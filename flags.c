#include "flags.h"

int sys_tick_flag = 0;
void setSysFlag(void){
	sys_tick_flag = 1;
}

void clearSysFlag(void){
	sys_tick_flag = 0;
}