/*
 * main.h
 *
 *  Created on: Jan 1, 2013
 *      Author: cyberwizzard
 */

#ifndef MAIN_H_
#define MAIN_H_

#define VERSION "0.3"		// Version number for the program

#define DEMO_MODE 1  		// As testing UI features without a printer is hard, provide a dummy mode
							// WARNING: With this enabled, the serial port and machine driver will pretend
							// to drive the printer.

// Safety features: define some limits to prevent sending insane commands to the printer
#define MAX_X 180.0f
#define MIN_X 0.0f
#define MAX_Y 180.0f
#define MIN_Y 0.0f
#define MAX_Z 100.0f
#define MIN_Z 0.0f
#define MAX_SPEED_X 5000.0f
#define MAX_SPEED_Y 5000.0f
#define MAX_SPEED_Z 600.0f

// Mesh generation: define properties of the mesh
#define MESH_MIN_X    1.0f
#define MESH_MAX_X  179.0f
#define MESH_MIN_Y    1.0f
#define MESH_MAX_Y  179.0f
#define MESH_SIZE_X     8
#define MESH_SIZE_Y     8

#endif /* MAIN_H_ */
