/*
 * machine.c
 *
 *  Created on: Dec 6, 2012
 *      Author: cyberwizzard
 */

#include <sys/select.h>
#include <stdio.h>
#include <curses.h>
#include <stdlib.h>

#include "main.h"
#include "serial.h"
#include "machine.h"
#include "mesh_builder.h"

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

/**
 * Request the position of the toolhead from the printer
 * TODO: parse this and return the values?
 */
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

/**
 * Disable the motor hold; this switches off the motors (which might cause the machine
 * to lose alignment!)
 * Firmware: Marlin, Teacup
 */
int disable_motor_hold() {
	return serial_cmd("M84\n", NULL);
}

/**
 * Enable or disable the software end-stops, to allow the toolhead to go outside the safe
 * bounds on the machine.
 * Firmware: Marlin
 */
int enable_soft_endstops(bool on) {
	if(on)
		return serial_cmd("M211 S1\n",NULL);
	return serial_cmd("M211 S0\n",NULL);
}

/**
 * Disable the UBL mesh compensation; required to calibrate the mesh points (because the
 * firmware will try to compensate as well - which would be bad).
 */
int mesh_disable() {
	return serial_cmd("G29 D\n", NULL);
}

/**
 * Load the UBL mesh points from a specific EEPROM save slot (or the currently loaded mesh)
 * @param slot Set to -1 to load the current mesh points and not load a mesh from EEPROM
 * @param mesh Pointer to the mesh memory to load with the mesh points from the printer
 * @param wnd Window handle from ncurses to print debug info into (when NULL no debug info is generated)
 */
int mesh_download(int slot, ty_meshpoint mesh[MESH_SIZE_Y][MESH_SIZE_X], WINDOW* wnd) {
	int err = 0, row = 0, col = 0, lpos = 0, only_valid = 1, has_comma = 0;
	char cmd_buf[100];
	char *res_buf = NULL;

	// See if a slot should be loaded first
	if(slot >= 0) {
		snprintf(cmd_buf,100,"G29 L%i\n", slot);
		if((err = serial_cmd(cmd_buf, NULL))) return err;
		if(wnd != NULL) { wprintw(wnd,"Slot load OK\n"); wrefresh(wnd); }
	}

	// Reset and invalidate all points in the mesh
	for(int y=0; y<MESH_SIZE_Y; y++) {
		for(int x=0; x<MESH_SIZE_X; x++) {
			mesh[y][x].z     = 0.0f;
			mesh[y][x].valid = 0;
		}
	}
	if(wnd != NULL) { wprintw(wnd,"Mesh reset OK\n"); wrefresh(wnd); }

	// Fetch all mesh points in CSV format
	if((err = serial_cmd("G29 T1\n", &res_buf))) return err;
	if(wnd != NULL) { wprintw(wnd,"G29T OK\n"); wrefresh(wnd); }

	// Scan the buffer until a line with at least one comma is found and only
	// numbers and dots (other characters indicate status lines)
	for (int p = 0; p < 4096; p++) {
		if(res_buf[p] == '\n') {
			// Check if the previous line was valid (only comma delimited numbers)
			if(only_valid && has_comma) break;

			// Previous line was not a CSV list of floats, continue scanning...

			// New line character - clear flags
			only_valid = 1;
			has_comma = 0;
			// Update the line start in the 'left pos' flag
			lpos = p+1;
		} else if(res_buf[p] == '\r') {
			// Carriage return - swallow this character silently
		} else if(res_buf[p] == ',') {
			has_comma = 1;
		} else if((res_buf[p] >= '0' && res_buf[p] <= '9') || res_buf[p] == '.' || res_buf[p] == '-') {
			// Numeric characters - silently swallow as these are allowed
		} else {
			// Unknown characters; mark line as invalid
			only_valid = 0;
		}
	}
	if(!only_valid || !has_comma) {
		// No valid lines found in the first 4kB of output...
		free(res_buf);
		return 1;
	}
	if(wnd != NULL) { wprintw(wnd,"CSV data start @ %i\n", lpos); wrefresh(wnd); }

	// lpos now points to the first line of CSV data describing the mesh

	// loop over the buffer to find either a comma or a newline
	for(int pos=lpos+1; pos<4096; pos++) {
		if(res_buf[pos] == 0) {
			// End of string - should never occur when parsing numbers...
			free(res_buf);
			return 1;
		} else if(res_buf[pos] == '\r') {
			// Carriage return is not needed, ignore it by replacing it with a space
			res_buf[pos] = ' ';
		} else if(res_buf[pos] == ',') {
			// Parse number string between lpos and pos into float
			// Note: change comma into null to 'end' the numeric string at the comma
			res_buf[pos] = 0;
			mesh[row][col].z = atof(&res_buf[lpos]);
			mesh[row][col].valid = 1;
			if(wnd != NULL) { wprintw(wnd,"Parsed CSV: (%i, %i) => %.03f\n", col, row, mesh[row][col].z); wrefresh(wnd); }
			// Revert to be able to print the whole buffer if we want to
			res_buf[pos] = ',';
			// Update left position
			lpos = pos+1;
			// Move column
			col++;
			// Sanity
			if(col >= MESH_SIZE_X) {
				// More mesh point columns in printer result than we allow!
				free(res_buf);
				return 3;
			}
		} else if(res_buf[pos] == '\n') {
			// Parse number string between lpos and pos into float
			// Note: change comma into null to 'end' the numeric string at the comma
			res_buf[pos] = 0;
			mesh[row][col].z = atof(&res_buf[lpos]);
			mesh[row][col].valid = 1;
			if(wnd != NULL) { wprintw(wnd,"Parsed CSV: (%i, %i) => %.03f\n", col, row, mesh[row][col].z); wrefresh(wnd); }
			// Revert to be able to print the whole buffer if we want to
			res_buf[pos] = '\n';
			// Update left position
			lpos = pos+1;
			// Reset column
			col = 0;
			// Move row
			row++;

			// End of mesh test
			if(row == MESH_SIZE_Y) {
				break;
			}

			// Sanity
			if(row >= MESH_SIZE_Y) {
				// More mesh point rows in printer result than we allow!
				free(res_buf);
				return 4;
			}
		}
	}

	if(wnd != NULL) { wprintw(wnd,"Done parsing mesh\n"); wrefresh(wnd); }

	// Parsing done, free buffer
	free(res_buf);

	// Flag success
	return 0;
}

/**
 * Upload the UBL mesh points into a specific EEPROM save slot (or the currently loaded mesh)
 * @param slot Set to -1 to only load the mesh into the printer RAM (and not EEPROM)
 * @param mesh Pointer to the mesh memory to upload with the mesh points for the printer
 * @param wnd Window handle from ncurses to print debug info into (when NULL no debug info is generated)
 */
int mesh_upload(int slot, ty_meshpoint mesh[MESH_SIZE_Y][MESH_SIZE_X], WINDOW* wnd) {
	int err = 0;
	char cmd_buf[100];

	// Reset and invalidate all points in the mesh
	for(int y=0; y<MESH_SIZE_Y; y++) {
		for(int x=0; x<MESH_SIZE_X; x++) {
			if(mesh[y][x].valid)
				snprintf(cmd_buf, 100, "M421 I%i J%i Z%.03f\n", x, y, mesh[y][x].z);
			else
				snprintf(cmd_buf, 100, "M421 I%i J%i N1\n", x, y);
			// Execute upload for this point in the mesh
			if((err = serial_cmd(cmd_buf, NULL))) return err;
		}
	}
	if(wnd != NULL) { wprintw(wnd,"Mesh upload OK\n"); wrefresh(wnd); }

	// See if a slot should be saved
	if(slot >= 0) {
		snprintf(cmd_buf,100,"G29 S%i\n", slot);
		if((err = serial_cmd(cmd_buf, NULL))) return err;
		if(wnd != NULL) { wprintw(wnd,"Slot save OK\n"); wrefresh(wnd); }
	}

	// Flag success
	return 0;
}

// ================================= Temperature stuff ====================

int enable_fan(bool on) {
	if(on)
		return serial_cmd("M106 S255\n",NULL);
	return serial_cmd("M106 S0\n",NULL);
}

/**
 * Get the printer temperatures
 * Firmware: Marlin
 *
 * @param hotend_temp pointer to a double to store the hotend temperature in
 * @param bed_temp pointer to a double to store the bed temperature in
 */
int get_temperature(double *hotend_temp, double* bed_temp) {
	char *reply = NULL;
	if(serial_cmd("M105\n",&reply) != 0) return -1;

	// Find the location of the ':' which will be followed by the temperature
	int pos_s = -1; // Separator position
	int pos_e = 0; // End of temperature string
	bool got_bed = false, got_hotend = false;
	do {
		if (reply[pos_e] == ':') pos_s = pos_e;
		// Detect end of temperature string (there is always a space after the temp
		else if(pos_s != -1 && reply[pos_e] == ' ') {
			// Get the character before the separator
			char type = reply[pos_s-1];
			// Convert the space into a NULL to terminate the string for atof()
			reply[pos_e] = 0;

			// Determine where to store the temp
			if(type == 'T') {
				if(hotend_temp != NULL)
					*hotend_temp = strtod(&reply[pos_s+1], NULL);
				got_hotend = true;
			} else if(type == 'B') {
				if(bed_temp != NULL)
					*bed_temp = strtod(&reply[pos_s+1], NULL);
				got_bed = true;
			}
			//else - if none of the above silently ignore it

			// Restore space
			reply[pos_e] = ' ';
			// Reset separator index
			pos_s = -1;
		}

		pos_e++;
	} while(reply[pos_e] != 0x0);

	return (got_bed && got_hotend) ? 0 : 1;
}

/**
 * Set the hotend temperature - this command does not wait until the target temperature is reached
 *
 * Note: Teacup allows setting the bed temperature using ID 1
 * @param temp The temperature to set the printer bed to
 * @param heaterid Hotend ID to set the temperature on - if the machine only has one hotend, its usually ID 0
 */
int set_hotend_temperature(double temp, const unsigned char heaterid) {
	char buf[100];
	if(temp < 0) temp = 0;
	if(temp > MAX_TEMP_HOTEND)
		temp = MAX_TEMP_HOTEND;

	// Auto-cooling: when enabling the hotend, turn on the fan as well for temperatures above AUTOCOOL_TEMP_THRESHOLD degrees
	// Note: when switching off the hotend, the fan is left running unless the temperature is back at room level
#ifdef ENABLE_AUTOCOOL_HOTEND
	if(temp >= AUTOCOOL_TEMP_THRESHOLD) {
		// Hot-end enabled, enable fan as well
		enable_fan(true);
	} else {
		// Disabling hot-end; test if the temperature is low enough to switch off the fan
		double t_hotend = 0;
		get_temperature(&t_hotend, NULL);
		if(t_hotend <= AUTOCOOL_TEMP_THRESHOLD) {
			// Hotend cool enough, switch off fan as well
			enable_fan(false);
		}
	}
#endif

	snprintf(buf,100,"M104 P%hhu S%.0f\n", heaterid, temp);
	return serial_cmd(buf,NULL);
}

/**
 * Set the bed temperature - this command does not wait until the target temperature is reached
 * @param temp The temperature to set the printer bed to
 */
int set_bed_temperature(double temp) {
	char buf[100];
	if(temp < 0) temp = 0;
	if(temp > MAX_TEMP_BED)
		temp = MAX_TEMP_BED;

	snprintf(buf,100,"M140 S%.0f\n", temp);
	return serial_cmd(buf,NULL);
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
