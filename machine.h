/*
 * machine.h
 *
 *  Created on: Dec 6, 2012
 *      Author: cyberwizzard
 */

#ifndef MACHINE_H_
#define MACHINE_H_

#include "main.h"		// Also contains the machine boundaries
#include "mesh_builder.h"

#include <ncurses.h>

#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
//#define SAFETY_LIMIT_TEST(axis, value, result) if(value < TOKENPASTE(MIN_,axis) || value > TOKENPASTE(MAX_,axis)) { result = -1; printf("Axis position out of bounds: " axis "=%.3f\n", value); }
#define SAFETY_LIMIT_TEST(axis, value, result) { if(value < TOKENPASTE2(MIN_,axis) || value > TOKENPASTE2(MAX_,axis)) { \
	result = -1; \
	printf("Axis position out of bounds: " #axis "=%.3f\n", value); \
	}}

#define ASSERT(x) {int _return_code = x; if(_return_code!=0) { printf("Command assert failed in " __FILE__":%i with code %i\n", __LINE__, _return_code); return _return_code; }}

/**
 * Position the head of the machine in 3 dimensional space and with a given speed.
 * Note that only changed parameters are sent to the printer to reduce traffic over
 * the serial port when changes_only is true.
 * @param xval X coordinate
 * @param yval Y coordinate
 * @param zval Z coordinate
 * @param relative Marks the given coordinates as a relative move instead of an absolute one
 * @param speed for the move, always absolute
 * @return 0 for OK or a negative error code otherwise
 */
int set_position(float xval, float yval, float zval, int relative, float speed, bool changes_only = true);

// Wrapper functions around set_position for ease of use
int home_x();
int set_x(float val);
int set_x(float val, int relative);
int set_x(float val, int relative, float speedval);
float get_x();

int home_y();
int set_y(float val);
int set_y(float val, int relative);
int set_y(float val, int relative, float speedval);
float get_y();

int home_z();
int set_z(float val);
int set_z(float val, int relative);
int set_z(float val, int relative, float speedval);
float get_z();

int home_xy();
int home_xyz();

int get_pos();

int set_speed(float val);

/**
 * Load the UBL mesh points from a specific EEPROM save slot (or the currently loaded mesh)
 * @param slot Set to -1 to load the current mesh points and not load a mesh from EEPROM
 * @param mesh Pointer to the mesh memory to load with the mesh points from the printer
 */
int mesh_download(int slot = -1, ty_meshpoint mesh[MESH_SIZE_Y][MESH_SIZE_X] = NULL, WINDOW* wnd = NULL);

/**
 * Upload the UBL mesh points into a specific EEPROM save slot (or the currently loaded mesh)
 * @param slot Set to -1 to only load the mesh into the printer RAM (and not EEPROM)
 * @param mesh Pointer to the mesh memory to upload with the mesh points for the printer
 * @param wnd Window handle from ncurses to print debug info into (when NULL no debug info is generated)
 */
int mesh_upload(int slot, ty_meshpoint mesh[MESH_SIZE_Y][MESH_SIZE_X], WINDOW* wnd);

/**
 * Tell the machine to dwell for a number of microseconds. This is an unbuffered command
 * which can also be used as a barrier to wait for the command queue to flush between moves.
 * @param timeout in microseconds (1000 is one second)
 * @return 0 when OK or an error code otherwise
 */
int set_dwell(int timeout);

/**
 * Adjust the alignment of the Z axis by overriding the position of the head. This allows us to
 * move beyond the built-in boundaries; for example this can be used to home the Z axis into the
 * opt-flag and then lower it towards the bed (through the opto-flag barrier).
 * WARNING: Overriding the position of the toolhead can result in serious damage as the machine
 * will blindly accept the new coordinates!
 * @param val The value for the Z axis
 * @return 0 when OK or an error code otherwise
 */
int override_zpos(float val);

int disable_motor_hold();

int enable_soft_endstops(bool on);

int mesh_disable();

int enable_fan(bool on);

int get_temperature(const unsigned char heaterid, double *temp);
int set_temperature(const unsigned char heaterid, const double temp);

int set_pid_p(const double p);
int set_pid_i(const int i);
int set_pid_d(const int d);
int print_pid();


#endif /* MACHINE_H_ */
