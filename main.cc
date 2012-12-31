/*
 * main.c
 *
 *  Created on: Dec 5, 2012
 *      Author: cyberwizzard
 */
#include <stdio.h>
#include <unistd.h>

#include "serial.h"
#include "machine.h"
#include "calibratez.h"

#define _(x) ASSERT(x)

int main(void) {
	printf("RepRap Bed Level Tool by Berend Dekens\n");
	if(serial_open() < 0) return -1;
	printf("Opened serial port\n");
//
//	printf("Homing all axis\n");
//	home_xyz();
//	//home_z();
//	printf("Homing complete\n");
//	set_speed(4000);
//
//	for(int i=0;i<3;i++) {
//		printf("Moving Z\n");
//		_(set_z(10,1));
//		_(set_dwell(0));
//		printf("Doing square\n");
//		_(set_x(100));
//		_(set_y(100));
//		_(set_x(0));
//		_(set_y(0));
//		_(set_dwell(0));
//		printf("Move complete\n");
//		sleep(5);
//	}
//	home_xyz();

	calibratez_heightloop();
	sleep(10);
	serial_close();
}


