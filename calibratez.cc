/*
 * calibratez.c
 *
 *  Created on: Dec 6, 2012
 *      Author: cyberwizzard
 */

#include <stdio.h>
#include <curses.h>

#include "machine.h"
#include "calibratez.h"
#include "utility.h"
#include "tui.h"

const float M4_pitch = 0.7f;	// Pitch of a M4 bolt

int stepsize = 0;				// Step size for lowering or raising the head

// Corner positions and z offset information - needed to split logic into multiple functions
float zpos[3] = {};			// Position of the toolhead at each corner
float zoffset = -7.0f;		// The offset of the Z axis using G92, this allows us to get beyond the optoflag

void print_instructions(WINDOW *wnd, int base_corner = 0);
void print_instructions_avg(WINDOW *wnd);
void print_instructions_3corners(WINDOW *wnd);


void destroy_win(WINDOW *local_win)
{
	/* box(local_win, ' ', ' '); : This won't produce the desired
	 * result of erasing the window. It will leave it's four corners
	 * and so an ugly remnant of window.
	 */
	wborder(local_win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
	/* The parameters taken are
	 * 1. win: the window on which to operate
	 * 2. ls: character to be used for the left side of the window
	 * 3. rs: character to be used for the right side of the window
	 * 4. ts: character to be used for the top side of the window
	 * 5. bs: character to be used for the bottom side of the window
	 * 6. tl: character to be used for the top left corner of the window
	 * 7. tr: character to be used for the top right corner of the window
	 * 8. bl: character to be used for the bottom left corner of the window
	 * 9. br: character to be used for the bottom right corner of the window
	 */
	wrefresh(local_win);
	delwin(local_win);
}

void print_status_bar(int row, int stepsize) {
	const char *banner = "[F1-F4] Move to corner [F5] Do calibration [Up/Down] Raise/lower head [Left/Right] Change step size: %s";
	const char *step0 = "[1mm] 0.1mm 0.01mm";
	const char *step1 = "1mm [0.1mm] 0.01mm";
	const char *step2 = "1mm 0.1mm [0.01mm]";
	const char *step = (stepsize==0?step0:((stepsize==1)?step1:step2));
	mvprintw(row, 0, banner, step);
	refresh();
}

int calibratez_heightloop() {
	int keepgoing = 1;			// Flag to terminate the input loop
	int pos = 0;				// Current position of the toolhead, from [0..3] => [00,X0,XY,0Y]
	float xyspeed = 4000.0f;	// Speed when moving from corner to corner
	int ch;						// Integer to hold the key scancode during the input loop
	float zraise = 0.0f;		// How far should the head be raised during moves from corner to corner?

	// Input loop variables
	stepsize = 1;				// 0 = 1mm, 1 = 0.1mm, 2 = 0.01mm
	float step = 0.1f;
	int nopower = 0;			// Status flag: when the user pressed F5 to get the alignment info,
								// the motors are shut down and all axis can move freely.
								// We require the user to hit F6 afterwards to home all axis and restart
								// the alignment test.

	// Start curses and all windows
	tui_init();

	// Start by homing all axis on the machine
	wprintw(cmd_win,"Homing all axis\n");
	wrefresh(cmd_win);
	//ASSERT(home_xyz());
	// Home all axis by raising Z first to avoid scraping the tape
	{
		ASSERT(override_zpos(0.0f));
		ASSERT(set_z(1.0f,0,MAX_SPEED_Z));
		ASSERT(home_xy());
		ASSERT(home_z());
	}

	// How far can the head be lowered from the Z end stop?
	{
		int zoi = 0;
		if(!utility_ask_int(cmd_win, "How far can the toolhead be lowered in mm?", &zoi, 10, 0, 20, 1)) goto stop;
		zoffset = (float)-zoi;
		wprintw(cmd_win,"Using %.2f mm as the absolute lowest position\n", zoffset);
		wrefresh(cmd_win);
	}

	{
		int zoi = 0;
		if(!utility_ask_int(cmd_win, "During XY moves, how far should the toolhead be raised?", &zoi, 0, 0, 10, 1)) goto stop;
		zraise = (float)zoi;
		wprintw(cmd_win,"Using %.2f mm as the repositioning height\n", zraise);
		wrefresh(cmd_win);
	}

	// Apply the Z offset so we can lower the toolhead during leveling
	ASSERT(override_zpos(-zoffset));

	// Set all starting positions to the original zero
	zpos[0] = zpos[1] = zpos[2] = zpos[3] = -zoffset;

	// Input loop
	// Print the status bar
	print_status_bar(LINES-1, stepsize);
	while(keepgoing) {
		if(stepsize < 0 || stepsize > 2) {
			wprintw(cmd_win,"Invalid step size detected: %i\n", stepsize);
			goto stop;
		}

		// Read a keyboard press
		ch = getch();
		// Correct the scan code for special keys like the arrow keys (consisting of 2 keystrokes)
		if ( ch == 0 || ch == 224 )
		ch = 256 + getch();

		// Handle the key press
		if(nopower == 0) {
			switch(ch) {
			case 'q': // Quit the control loop
				keepgoing = 0;

				// Raise the head
				set_z(-zoffset);
				// Move to origin
				set_position(0,0,get_z(),0,xyspeed);

				break;
			case KEY_LEFT:
				if(stepsize > 0) stepsize--;
				switch(stepsize) {
					case 0: step = 1.0f; break;
					case 1: step = 0.1f; break;
					case 2: step = 0.01f; break;
				}
				// Print the status bar
				print_status_bar(LINES-1, stepsize);
				break;
			case KEY_RIGHT:
				if(stepsize < 2) stepsize++;
				switch(stepsize) {
					case 0: step = 1.0f; break;
					case 1: step = 0.1f; break;
					case 2: step = 0.01f; break;
				}
				// Print the status bar
				print_status_bar(LINES-1, stepsize);
				break;
			case KEY_DOWN: {
				float z = get_z();
				z -= step;
				if(z < 0) {
					wprintw(cmd_win,"Warning: could not lower toolhead further, switch to a smaller step size\n");
				} else {
					// Lower the head
					wprintw(cmd_win,"Setting Z to %.2f\n", z+zoffset);
					ASSERT(set_z(z,0,MAX_SPEED_Z));
				}}
				break;
			case KEY_UP: {
				float z = get_z();
				z += step;
				if(z > 50.0f) {
					wprintw(cmd_win,"Warning: could not raise toolhead further, switch to a smaller step size\n");
				} else {
					// Raise the head
					wprintw(cmd_win,"Setting Z to %.2f\n", z+zoffset);
					ASSERT(set_z(z,0,MAX_SPEED_Z));
				}}
				break;
			case '1':
			case KEY_F1:
				// Only move if we are not already at that corner
				if(pos == 0) break;
				wprintw(cmd_win, "Moving to corner 1 @ (%.1f,%.1f)\n", MIN_X, MIN_Y);

				// Store the current position of the Z axis
				zpos[pos] = get_z();
				// Set the current position to this one
				pos = 0;
				// Move the toolhead up to the correct height to move the toolhead
				set_z(-zoffset + zraise);
				// Move to corner 1
				set_position(MIN_X,MIN_Y,-zoffset + zraise,0,xyspeed);
				// Lower toolhead to previous setting
				set_z(zpos[pos]);
				wprintw(cmd_win, "Ready @ Z=%.1f\n", zpos[pos]+zoffset);

				break;
			case '2':
			case KEY_F2:
				// Only move if we are not already at that corner
				if(pos == 1) break;
				wprintw(cmd_win, "Moving to corner 2 @ (%.1f,%.1f)\n", MAX_X, MIN_Y);

				// Store the current position of the Z axis
				zpos[pos] = get_z();
				// Set the current position to this one
				pos = 1;
				// Move the toolhead up to the correct height to move the toolhead
				set_z(-zoffset + zraise);
				// Move to corner 1
				set_position(MAX_X,MIN_Y,-zoffset + zraise,0,xyspeed);
				// Lower toolhead to previous setting
				set_z(zpos[pos]);
				wprintw(cmd_win, "Ready @ Z=%.1f\n", zpos[pos]+zoffset);

				break;
			case '3':
			case KEY_F3:
				// Only move if we are not already at that corner
				if(pos == 2) break;
				wprintw(cmd_win, "Moving to corner 3 @ (%.1f,%.1f)\n", MAX_X, MAX_Y);

				// Store the current position of the Z axis
				zpos[pos] = get_z();
				// Set the current position to this one
				pos = 2;
				// Move the toolhead up to the correct height to move the toolhead
				set_z(-zoffset + zraise);
				// Move to corner 1
				set_position(MAX_X,MAX_Y,-zoffset + zraise,0,xyspeed);
				// Lower toolhead to previous setting
				set_z(zpos[pos]);
				wprintw(cmd_win, "Ready @ Z=%.1f\n", zpos[pos]+zoffset);

				break;
			case '4':
			case KEY_F4:
				// Only move if we are not already at that corner
				if(pos == 3) break;
				wprintw(cmd_win, "Moving to corner 4 @ (%.1f,%.1f)\n", MIN_X, MAX_Y);

				// Store the current position of the Z axis
				zpos[pos] = get_z();
				// Set the current position to this one
				pos = 3;
				// Move the toolhead up to the correct height to move the toolhead
				set_z(-zoffset + zraise);
				// Move to corner 1
				set_position(MIN_X,MAX_Y,-zoffset + zraise,0,xyspeed);
				// Lower toolhead to previous setting
				set_z(zpos[pos]);
				wprintw(cmd_win, "Ready @ Z=%.1f\n", zpos[pos]+zoffset);

				break;
			case KEY_F5:
				// Store current position
				zpos[pos] = get_z();

				wprintw(cmd_win, "Positions of each corner:\n");
				wprintw(cmd_win, "[1] (%.1f,%.1f) => %.2f\n", MIN_X, MIN_Y, zoffset+zpos[0]);
				wprintw(cmd_win, "[2] (%.1f,%.1f) => %.2f\n", MAX_X, MIN_Y, zoffset+zpos[1]);
				wprintw(cmd_win, "[3] (%.1f,%.1f) => %.2f\n", MAX_X, MAX_Y, zoffset+zpos[2]);
				wprintw(cmd_win, "[4] (%.1f,%.1f) => %.2f\n", MIN_X, MAX_Y, zoffset+zpos[3]);

				{
					int bc = 0;
					if(!utility_ask_int(cmd_win, "Which corner should be stationary?\n1 to 4, 5 for average or 6 for three corners? (Esc to cancel)", &bc, 1, 1, 6, 1)) {
						// Abort the calculation
						break;
					}

					if(bc >= 1 && bc <= 4)
						// Use one corner as a fixed point
						print_instructions(cmd_win, bc-1);
					else if(bc == 5)
						// Use the average between all corners
						print_instructions_avg(cmd_win);
					else
						// Level the bed using 3 screws and one floating side
						print_instructions_3corners(cmd_win);
				}

				// Move the head and present the bed
				set_z(-zoffset);
				set_position(MIN_X,MAX_Y,-zoffset,0,xyspeed);
				// Barrier: wait for the move to complete before turning off the motors
				set_dwell(0);

				// Disable the motor hold
				disable_motor_hold();
				wprintw(cmd_win, "Motor hold disabled, press F6 to power on and home all axis\n");

				// Set the flag to force the user to press F6 or quit
				nopower = 1;
				break;
			case 410:
				// Resize event
				tui_resize();
				break;
			default: // Unknown keypress
				wprintw(cmd_win,"Invalid key: %i\n", ch);
			}
		} else {
			// Motors are powered off: only allow 'q'uit or F6 to power on and home al axis
			switch(ch) {
				case 'q': // Quit the control loop
					keepgoing = 0;

					// Raise the head
					set_z(-zoffset);
					// Move to origin
					set_position(0,0,get_z(),0,xyspeed);

					break;
				case KEY_LEFT:
				case KEY_RIGHT:
				case KEY_DOWN:
				case KEY_UP:
				case '1':
				case KEY_F1:
				case '2':
				case KEY_F2:
				case '3':
				case KEY_F3:
				case '4':
				case KEY_F4:
				case KEY_F5:
					wprintw(cmd_win, "Motor hold disabled, press F6 to restart and home all axis or F7 to home and retain each corner\n");
					break;
				case KEY_F6:
					// Erase the Z positioning to go back to the Z home point
					for(int i=0;i<4;i++) zpos[i] = -zoffset;
					/* no break */
				case KEY_F7:
					// Disable the flag to return to normal alignment mode
					nopower = 0;
					// Tell the user what we are doing
					wprintw(cmd_win, "Homing all axis\n");
					home_xyz();
					// Override the Z offset again
					override_zpos(-zoffset);
					// We are back at position 1
					pos = 0;
					// Ready for another round
					wprintw(cmd_win, "Ready\n");
				default: // Unknown keypress
					wprintw(cmd_win,"Invalid key: %i\n", ch);
			}
		}

		// When we are all done, update the screen
		wrefresh(cmd_win);
	}

stop:
	// Remove the window for the serial output and its pointer
	destroy_win(serial_border);
	destroy_win(serial_win);
	serial_win = NULL;

	// Destroy the status panel
	destroy_win(cmd_win);

	endwin();
	printf("Bed Height Calibration terminated\n");

	return 0;
}

/**
 * Print instructions to level the bed, based on a specific corner.
 * We make one corner stationary and take the 3 other corners to screw up or down.
 * @param wnd Window to print the question and feedback in
 * @param base_corner Corner to hold stationary: 1 to 4, clock-wise starting at left-back.
 */
void print_instructions(WINDOW *wnd, int base_corner) {
	assert(base_corner >= 0 && base_corner <= 3);
	const char *left = "left";
	const char *right = "right";

	// Use pointers to the corner positions to quickly switch between base modes
	float zpos0,zpos1,zpos2,zpos3;
	int z_number[4];

	// Generate a list of corner numbers, omit the base corner
	z_number[0] = base_corner;
	for(int i=1;i<4;i++) z_number[i] = base_corner < i ? i : i - 1;

	// Use the corner list to remap the corners, keeping the base at 0 and the rest at 1 to 3
	zpos0 = zpos[base_corner];
	zpos1 = zpos[z_number[1]];
	zpos2 = zpos[z_number[2]];
	zpos3 = zpos[z_number[3]];

	wprintw(wnd, "Using corner %i as base:\n", z_number[0]+1);
	{
		const char *dir = (zpos1 < zpos0) ? left : right;
		float angle = ((zpos1 - zpos0) / M4_pitch);
		if(angle < 0) angle = -angle;
		wprintw(wnd, "Corner %i: delta %.2f mm - turn screw %.2f times %s\n", z_number[1]+1, zpos1-zpos0, angle, dir);
	}
	{
		const char *dir = (zpos2 < zpos0) ? left : right;
		float angle = ((zpos2 - zpos0) / M4_pitch);
		if(angle < 0) angle = -angle;
		wprintw(wnd, "Corner %i: delta %.2f mm - turn screw %.2f times %s\n", z_number[2]+1, zpos2-zpos0, angle, dir);
	}
	{
		const char *dir = (zpos3 < zpos0) ? left : right;
		float angle = ((zpos3 - zpos0) / M4_pitch);
		if(angle < 0) angle = -angle;
		wprintw(wnd, "Corner %i: delta %.2f mm - turn screw %.2f times %s\n", z_number[3]+1, zpos3-zpos0, angle, dir);
	}
}

/**
 * Print the instructions to level the bed by adjusting all corners to obtain a level bed
 * between the height of all corners.
 * @param wnd Window to print the question and feedback in
 */
void print_instructions_avg(WINDOW *wnd) {
	// Find the maximum and minimum of all 4 corners
	const char *left = "left";
	const char *right = "right";
	float min = zpos[0], max = zpos[3], avg;
	for(int i=1;i<4;i++) {
		if(min > zpos[i]) min = zpos[i];
		if(max < zpos[i]) max = zpos[i];
	}
	// Take the average
	avg = (max+min) * 0.5f;

	wprintw(wnd, "Taking the average of all corners\n");
	{
		const char *dir = (zpos[0] < avg) ? left : right;
		float angle = ((zpos[0] - avg) / M4_pitch);
		if(angle < 0) angle = -angle;
		wprintw(wnd, "Corner 1: delta %.2f mm - turn screw %.2f times %s\n", zpos[0]-avg, angle, dir);
	}
	{
		const char *dir = (zpos[1] < avg) ? left : right;
		float angle = ((zpos[1] - avg) / M4_pitch);
		if(angle < 0) angle = -angle;
		wprintw(wnd, "Corner 2: delta %.2f mm - turn screw %.2f times %s\n", zpos[1]-avg, angle, dir);
	}
	{
		const char *dir = (zpos[2] < avg) ? left : right;
		float angle = ((zpos[2] - avg) / M4_pitch);
		if(angle < 0) angle = -angle;
		wprintw(wnd, "Corner 3: delta %.2f mm - turn screw %.2f times %s\n", zpos[2]-avg, angle, dir);
	}
	{
		const char *dir = (zpos[3] < avg) ? left : right;
		float angle = ((zpos[3] - avg) / M4_pitch);
		if(angle < 0) angle = -angle;
		wprintw(wnd, "Corner 4: delta %.2f mm - turn screw %.2f times %s\n", zpos[3]-avg, angle, dir);
	}
}

/**
 * Special alignment mode: 3 corners (4th has no screw).
 * We align this by averaging the Z between the orthogonal diagonal corners and
 * raising the 3rd point to this Z as well. The 4th corner should follow suit.
 */
void print_instructions_3corners(WINDOW *wnd) {
	// Find the maximum and minimum of all 4 corners
	const char *left = "left";
	const char *right = "right";
	float avg = 0.0f, p1 = 0.0f, p2 = 0.0f;

	// Which corner is floating
	int open = 0, mode = 3;
	if(!utility_ask_int(wnd, "Which corner is floating?", &open, 2, 1, 4, 1)) {
		// Abort the instructions
		return;
	}
	open--;		// We index from 0
	if(!utility_ask_int(wnd, "Optimize diagonal using 1:lowest point, 2:average, 3:highest point?", &mode, 2, 1, 3, 1)) {
		// Abort the instructions
		return;
	}

	wprintw(wnd, "Taking corner %d as floating\n", open+1);

	// Determine the average
	switch(open) {
	case 0:
		// Corner 1, use 2 and 4
	case 2:
		// Corner 3, use 2 and 4
		p1 = zpos[1];
		p2 = zpos[3];
		break;
	case 1:
		// Corner 2, use 1 and 3
	default:
		// Corner 4, use 1 and 3
		p1 = zpos[0];
		p2 = zpos[2];
		break;
	}

	// Determine the target height for all 3 (4) corners
	switch(mode) {
	case 1:
		wprintw(wnd, "Using the lowest point on the diagonal\n");
		if(p1 < p2) avg = p1; else avg = p2;
		break;
	case 2:
		wprintw(wnd, "Using the average between both points on the diagonal\n");
		avg = (p1 + p2) * 0.5f;
		break;
	case 3:
	default:
		wprintw(wnd, "Using the highest point on the diagonal\n");
		if(p1 > p2) avg = p1; else avg = p2;
		break;
	}

	if(open!=0){
		const char *dir = (zpos[0] < avg) ? left : right;
		float angle = ((zpos[0] - avg) / M4_pitch);
		if(angle < 0) angle = -angle;
		wprintw(wnd, "Corner 1: delta %.2f mm - turn screw %.2f times %s\n", zpos[0]-avg, angle, dir);
	}
	if(open!=1){
		const char *dir = (zpos[1] < avg) ? left : right;
		float angle = ((zpos[1] - avg) / M4_pitch);
		if(angle < 0) angle = -angle;
		wprintw(wnd, "Corner 2: delta %.2f mm - turn screw %.2f times %s\n", zpos[1]-avg, angle, dir);
	}
	if(open!=2){
		const char *dir = (zpos[2] < avg) ? left : right;
		float angle = ((zpos[2] - avg) / M4_pitch);
		if(angle < 0) angle = -angle;
		wprintw(wnd, "Corner 3: delta %.2f mm - turn screw %.2f times %s\n", zpos[2]-avg, angle, dir);
	}
	if(open!=3){
		const char *dir = (zpos[3] < avg) ? left : right;
		float angle = ((zpos[3] - avg) / M4_pitch);
		if(angle < 0) angle = -angle;
		wprintw(wnd, "Corner 4: delta %.2f mm - turn screw %.2f times %s\n", zpos[3]-avg, angle, dir);
	}
}
