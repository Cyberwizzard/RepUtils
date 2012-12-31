/*
 * machine.h
 *
 *  Created on: Dec 6, 2012
 *      Author: cyberwizzard
 */

#ifndef MACHINE_H_
#define MACHINE_H_

// Safety features: define some limits to prevent sending insane commands to the printer
#define MAX_X 190.0f
#define MIN_X 0.0f
#define MAX_Y 190.0f
#define MIN_Y 0.0f
#define MAX_Z 140.0f
#define MIN_Z 0.0f
#define MAX_SPEED_X 4000.0f
#define MAX_SPEED_Y 4000.0f
#define MAX_SPEED_Z 200.0f

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
 * the serial port.
 * @param xval X coordinate
 * @param yval Y coordinate
 * @param zval Z coordinate
 * @param relative Marks the given coordinates as a relative move instead of an absolute one
 * @param speed for the move, always absolute
 * @return 0 for OK or a negative error code otherwise
 */
int set_position(float xval, float yval, float zval, int relative, float speed);

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

int set_speed(float val);

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

#endif /* MACHINE_H_ */
