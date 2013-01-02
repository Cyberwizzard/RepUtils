/*
 * main.h
 *
 *  Created on: Jan 1, 2013
 *      Author: cyberwizzard
 */

#ifndef MAIN_H_
#define MAIN_H_

#define VERSION "0.2"		// Version number for the program

#define DEMO_MODE 0			// As testing UI features without a printer is hard, provide a dummy mode
							// WARNING: With this enabled, the serial port and machine driver will pretend
							// to drive the printer.

// Safety features: define some limits to prevent sending insane commands to the printer
#define MAX_X 183.0f
#define MIN_X 0.0f
#define MAX_Y 180.0f
#define MIN_Y 0.0f
#define MAX_Z 140.0f
#define MIN_Z 0.0f
#define MAX_SPEED_X 4000.0f
#define MAX_SPEED_Y 4000.0f
#define MAX_SPEED_Z 200.0f

#endif /* MAIN_H_ */
