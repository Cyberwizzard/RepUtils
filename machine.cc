/*
 * machine.c
 *
 *  Created on: Dec 6, 2012
 *      Author: cyberwizzard
 */

#include <stdio.h>
#include <curses.h>
#include <stdlib.h>
#include "main.h"
#include "serial.h"
#include "machine.h"

float x = 0.0f,y = 0.0f,z = 0.0f,speed = 0.0f;
extern WINDOW *serial_win;

#define message(...) {if(serial_win!=NULL) wprintw(serial_win, __VA_ARGS__); \
						else printf(__VA_ARGS__); }

/**
 * Position the head of the machine in 3 dimensional space and with a given speed.
 * Note that only changed parameters are sent to the printer to reduce traffic over
 * the serial port.
 * @param xval X coordinate
 * @param yval Y coordinate
 * @param zval Z coordinate
 * @param relative Marks the given coordinates as a relative move instead of an absolute one
 * @param speed for the move, always absolute
 */
int set_position(float xval, float yval, float zval, int relative, float speedval, bool changes_only) {
	int res = 0;
	// Boundary checks
	if(relative) { 	SAFETY_LIMIT_TEST(X, (x+xval), res); }
	else {			SAFETY_LIMIT_TEST(X, (xval), res); }
	if(relative) { 	SAFETY_LIMIT_TEST(Y, (y+zval), res); }
	else {			SAFETY_LIMIT_TEST(Y, (yval), res); }
	if(relative) { 	SAFETY_LIMIT_TEST(Z, (z+zval), res); }
	else {			SAFETY_LIMIT_TEST(Z, (zval), res); }
	if(res != 0) { message("safety limit failed\n"); }
	//else { message("safety ok\n"); }
	if(res != 0) return res;

	// Calculate position for each axis, determine which axis have changed
	int cx = 0, cy = 0, cz = 0, cs = (speedval != speed);
	if(cs) speed = speedval;
	if(relative) {
		// Relative move
		cx = xval != 0.0f;
		cy = yval != 0.0f;
		cz = zval != 0.0f;
		x+=xval;
		y+=yval;
		z+=zval;
	} else {
		// Absolute move
		cx = (xval != x);
		cy = (yval != y);
		cz = (zval != z);
		x = xval;
		y = yval;
		z = zval;
	}

	// Test if something needs to be done
	if((cx+cy+cz+cs)==0 && changes_only) {
		message("Delta move\n");
		return 0;
	}

	char buf[100];
	int ptr = 0;
	ptr += snprintf(buf, 100-ptr, "G01 ");
	if(cx || !changes_only) ptr += snprintf(&buf[ptr], 100-ptr,"X%.2f ", x);
	if(cy || !changes_only) ptr += snprintf(&buf[ptr], 100-ptr,"Y%.2f ", y);
	if(cz || !changes_only) ptr += snprintf(&buf[ptr], 100-ptr,"Z%.2f ", z);
	if(cs || !changes_only) ptr += snprintf(&buf[ptr], 100-ptr,"F%.2f ", speed);
	ptr += snprintf(&buf[ptr], 100-ptr,"\n");
	return serial_cmd(buf, NULL);
}

// ================== Utility functions to drive one or more axis =================
int set_x(float val) {
	return set_position(val,y,z,0,speed);
}

int set_x(float val, int relative) {
	if(relative) return set_position(val,0,0,1,speed);
	else return set_position(val,y,z,0,speed);
}

int set_x(float val, int relative, float speedval) {
	if(relative) return set_position(val,0,0,1,speedval);
	else return set_position(val,y,z,0,speedval);
}

int set_y(float val) {
	return set_position(x,val,z,0,speed);
}

int set_y(float val, int relative) {
	if(relative) return set_position(0,val,0,1,speed);
	else return set_position(x,val,z,0,speed);
}

int set_y(float val, int relative, float speedval) {
	if(relative) return set_position(0,val,0,1,speedval);
	else return set_position(x,val,z,0,speedval);
}

int set_z(float val) {
	return set_position(x,y,val,0,speed);
}

int set_z(float val, int relative) {
	if(relative) return set_position(0,0,val,1,speed);
	else return set_position(x,y,val,0,speed);
}

int set_z(float val, int relative, float speedval) {
	if(relative) return set_position(0,0,val,1,speedval);
	else return set_position(x,y,val,0,speedval);
}

int set_speed(float val) {
	return set_position(x,y,z,0,val);
}


// =========================== Other positioning ======================

float get_x() {
	return x;
}

float get_y() {
	return y;
}

float get_z() {
	return z;
}

int home_xy() {
	int res = serial_cmd("G28 X0 Y0\n", NULL);
	if(res) return res;
	res = serial_cmd("G01 X0 Y0\n", NULL);	// Home can be at the end as well; go to 0
	if(res == 0) x = y = 0;
	return res;
}

int home_xyz() {
	int res = serial_cmd("G28 X0 Y0 Z0\n", NULL);
	if(res) return res;
	res = serial_cmd("G01 X0 Y0 Z0\n", NULL);	// Home can be at the end as well; go to 0
	if(res == 0) x = y = z = 0;
	return res;
}

int home_x() {
	int res = serial_cmd("G28 X0\n", NULL);
	if(res) return res;
	res = serial_cmd("G01 X0\n", NULL);	// Home can be at the end as well; go to 0
	if(res == 0) x = 0;
	return res;
}

int home_y() {
	int res = serial_cmd("G28 Y0\n", NULL);
	if(res) return res;
	res = serial_cmd("G01 Y0\n", NULL);	// Home can be at the end as well; go to 0
	if(res == 0) y = 0;
	return res;
}

int home_z() {
	int res = serial_cmd("G28 Z0\n", NULL);
	if(res) return res;
	res = serial_cmd("G01 Z0\n", NULL);	// Home can be at the end as well; go to 0
	if(res == 0) z = 0;
	return res;
}

int get_pos() {
	return serial_cmd("M114\n", NULL);
}

/**
 * Tell the machine to dwell for a number of microseconds. This is an unbuffered command
 * which can also be used as a barrier to wait for the command queue to flush between moves.
 * @param timeout in microseconds (1000 is one second)
 * @return 0 when OK or an error code otherwise
 */
int set_dwell(int timeout) {
	if(timeout < 0) timeout = 0;
	char buf[100];
	snprintf(buf,100,"G04 P%i\n", timeout);
	return serial_cmd(buf, NULL);
}

/**
 * Adjust the alignment of the Z axis by overriding the position of the head. This allows us to
 * move beyond the built-in boundaries; for example this can be used to home the Z axis into the
 * opt-flag and then lower it towards the bed (through the opto-flag barrier).
 * WARNING: Overriding the position of the toolhead can result in serious damage as the machine
 * will blindly accept the new coordinates!
 * @param val The value for the Z axis
 * @return 0 when OK or an error code otherwise
 */
int override_zpos(float val) {
	if(val < -MAX_Z || val > MAX_Z) {
		message("Error in override_zpos: Z axis position %f is not safe\n", val);
		return -1;
	}
	z = val;	// Override position
	char buf[100];
	snprintf(buf,100,"G92 Z%.2f\n", val);
	return serial_cmd(buf, NULL);
}

int disable_motor_hold() {
	return serial_cmd("M84\n", NULL);
}

// ================================= Temperature stuff ====================

int enable_fan(bool on) {
	if(on)
		return serial_cmd("M106 S255\n",NULL);
	return serial_cmd("M106 S0\n",NULL);
}

int get_temperature(const unsigned char heaterid, double *temp) {
	char *reply,buf[100];
	snprintf(buf,100,"M105 P%hhu\n", heaterid);
	if(serial_cmd(buf,&reply) != 0) return -1;

	// Find the location of the ':' which will be followed by the temperature
	int pos_s = 0, pos_e = 0;
	do {
		if (reply[pos_e] == ':') pos_s = pos_e;
		pos_e++;
	} while(reply[pos_e] != 0x0);
	// Check for weird or empty reply
	if(pos_e <= pos_s) return -1;

	// Convert the value after the double colon to a double
	*temp = atof(&reply[pos_s+1]);

	return 0;
}

int set_temperature(const unsigned char heaterid, const double temp) {
	char *reply,buf[100];
	snprintf(buf,100,"M104 P%hhu S%.0f\n", heaterid, temp);
	return serial_cmd(buf,&reply);
}

/**
 * Modify the proportional scaling value of the PID logic in the printer.
 * WARNING: The commands for this might differ from Teacup on other firmwares!
 */
int set_pid_p(const double p) {
	char buf[100];
	snprintf(buf,100,"M130 P0 S%.2f\n", p);
	return serial_cmd(buf, NULL);
}

/**
 * Modify the integral scaling value of the PID logic in the printer.
 * WARNING: The commands for this might differ from Teacup on other firmwares!
 */
int set_pid_i(const int32_t i) {
	char buf[100];
	snprintf(buf,100,"M131 P0 S%i\n", i);
	return serial_cmd(buf, NULL);
}

/**
 * Modify the differential scaling value of the PID logic in the printer.
 * WARNING: The commands for this might differ from Teacup on other firmwares!
 */
int set_pid_d(const int32_t d) {
	char buf[100];
	snprintf(buf,100,"M132 P0 S%i\n", d);
	return serial_cmd(buf, NULL);
}

int print_pid() {
	return serial_cmd("M136\n", NULL);
}
