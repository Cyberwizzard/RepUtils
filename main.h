/*
 * main.h - Master configuration for the various parts of the program and the 3D printer configuration
 *
 *  Created on: Jan 1, 2013
 *      Author: cyberwizzard
 */

#ifndef MAIN_H_
#define MAIN_H_

#define VERSION "0.5"		// Version number for the program

#define DEMO_MODE 0  		// As testing UI features without a printer is hard, provide a dummy mode
							// WARNING: With this enabled, the serial port and machine driver will pretend
							// to drive the printer.

// Safety features: define some limits to prevent sending insane commands to the printer
// Temperature
#define MAX_TEMP_HOTEND 250
#define MAX_TEMP_BED    100
#define PREHEAT_TEMP_HOTEND 125
#define PREHEAT_TEMP_BED    85
// Uncomment to automatically enable the fan when setting any temperature above 0 on the hotend
#define ENABLE_AUTOCOOL_HOTEND
#define AUTOCOOL_TEMP_THRESHOLD 40

// Dimensional limits
#define MAX_X 180.0f
#define MIN_X 0.0f
#define MAX_Y 180.0f
#define MIN_Y 0.0f
#define MAX_Z 100.0f
#define MIN_Z 0.0f

// How many mm can the toolhead be lowered by default below the Z end stop when leveling the bed?
// The program will ask to confirm this value on each start.
#define DEFAULT_BOUND_Z 2
// Absolute lower bound on how far the toolhead can move below the Z end stop. This essentially determines
// how far the toolhead will crash into the bed if you mess up during leveling.
#define LOWER_BOUND_Z   4
// How far should the toolhead be raised during moves between mesh points?
// The program will ask to confirm this value on each start.
#define DEFAULT_RAISE_Z 2

// Default speeds
#define MAX_SPEED_X 5000.0f
#define MAX_SPEED_Y 5000.0f
#define MAX_SPEED_Z 150.0f

// Mesh generation: define properties of the mesh
#define MESH_MIN_X    1.0f
#define MESH_MAX_X  179.0f
#define MESH_MIN_Y    1.0f
#define MESH_MAX_Y  179.0f
#define MESH_SIZE_X     5
#define MESH_SIZE_Y     5

// Size of the serial buffer allocated to parse command responses; has to be large enough for the biggest reply but too large means
// high memory consumption in the program for no reason.
// Default: 1024 bytes
#define SERIAL_REPLY_BUFFER_SIZE 2048



#endif /* MAIN_H_ */
