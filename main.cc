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

	calibratez_heightloop();

	// Send a barrier command to the printer before shutting down
	set_dwell(100);
	// Close the serial port
	serial_close();
}


