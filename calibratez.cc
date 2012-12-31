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

const float M4_pitch = 0.7f;	// Pitch of a M4 bolt

WINDOW *serial_win = NULL;		// Window for the serial communication to be shown in (shared window)

// Corner positions and z offset information - needed to split logic into multiple functions
float zpos[3] = {};			// Position of the toolhead at each corner
float zoffset = -7.0f;		// The offset of the Z axis using G92, this allows us to get beyond the optoflag

void print_instructions(WINDOW *wnd, int base_corner = 0);
void print_instructions_avg(WINDOW *wnd);

WINDOW *create_newwin(int height, int width, int starty, int startx) {
	WINDOW *local_win;
	local_win = newwin(height, width, starty, startx);
	box(local_win, 0 , 0);		/* 0, 0 gives default characters
					 * for the vertical and horizontal
					 * lines			*/
	wrefresh(local_win);		/* Show that box 		*/
	return local_win;
}

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
	const char *banner = "[F1-F4] Move to corner [Up/Down] Raise/lower head [Left/Right] Change step size: %s";
	const char *step0 = "[1mm] 0.1mm 0.01mm";
	const char *step1 = "1mm [0.1mm] 0.01mm";
	const char *step2 = "1mm 0.1mm [0.01mm]";
	const char *step = (stepsize==0?step0:((stepsize==1)?step1:step2));
	mvprintw(row,0, banner, step);
	refresh();
}

void curses_init(int *maxrows, int *maxcols, WINDOW **serial_border, WINDOW **serial_wnd, WINDOW **status_wnd) {

}

int calibratez_heightloop() {
	int keepgoing = 1;			// Flag to terminate the input loop
	int pos = 0;				// Current position of the toolhead, from [0..3] => [00,X0,XY,0Y]
	float xyspeed = 4000.0f;	// Speed when moving from corner to corner
	int row, col;				// Number of rows and columns on the screen as reported by ncurses
	int ch;						// Integer to hold the key scancode during the input loop

	// Input loop variables
	int stepsize = 1;			// 0 = 1mm, 1 = 0.1mm, 2 = 0.01mm
	float step = 0.1f;
	int nopower = 0;			// Status flag: when the user pressed F5 to get the alignment info,
								// the motors are shut down and all axis can move freely.
								// We require the user to hit F6 afterwards to home all axis and restart
								// the alignment test.

	// Init ncurses
	initscr();					// Initialize the terminal for use with ncurses
	noecho();					// Do not echo the characters as they are typed
	keypad(stdscr, TRUE);		// The standard screen sends arrow keys as key strokes

	// Do the window init
	getmaxyx(stdscr,row,col);

	// To retain the borders on a window, we first define a window 2 columns and rows larger
	// than the window for the serial output and draw a box in it.
	WINDOW *serial_border = newwin(row-1, col/2, 0, col/2);
	// Create a window for the serial output to be shown during operation
	serial_win = newwin(row-3, col/2-2, 1, col/2+1);
	// When inserting a new line at the end of the window, move the cursor one line up
	// (instead of returning to the beginning of the last line)
	scrollok(serial_win, true);
	idlok(serial_win, true);


	// Create a status window for user interaction
	WINDOW *wnd = create_newwin(row-1, col/2, 0, 0);
	// When inserting a new line at the end of the window, move the cursor one line up
	// (instead of returning to the beginning of the last line)
	scrollok(wnd, true);
	idlok(wnd, true);

	// Update the screen to generate the windows and draw some boxes in them
	refresh();
	box(serial_border, 0, 0);
	wrefresh(serial_border);
	//refresh();

	wprintw(wnd,"Homing all axis\n");
	refresh();
	ASSERT(home_xyz());

	// How far can the head be lowered from the Z end stop?
	{
		int zoi = 0;
		if(!utility_ask_int(wnd, "How far can the toolhead be lowered in mm?", &zoi, 10, 0, 20, 1)) goto stop;
		zoffset = (float)-zoi;
		wprintw(wnd,"Using %.2f mm as the absolute lowest position\n", zoffset);
		wrefresh(wnd);
	}

	// Apply the Z offset so we can lower the toolhead during leveling
	ASSERT(override_zpos(-zoffset));

	// Set all starting positions to the original zero
	zpos[0] = zpos[1] = zpos[2] = zpos[3] = -zoffset;

	// Input loop
	// Print the status bar
	print_status_bar(row-1, stepsize);
	while(keepgoing) {
		if(stepsize < 0 || stepsize > 2) {
			wprintw(wnd,"Invalid step size detected: %i\n", stepsize);
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
				print_status_bar(row-1, stepsize);
				break;
			case KEY_RIGHT:
				if(stepsize < 2) stepsize++;
				switch(stepsize) {
					case 0: step = 1.0f; break;
					case 1: step = 0.1f; break;
					case 2: step = 0.01f; break;
				}
				// Print the status bar
				print_status_bar(row-1, stepsize);
				break;
			case KEY_DOWN: {
				float z = get_z();
				z -= step;
				if(z < 0) {
					wprintw(wnd,"Warning: could not lower toolhead further, switch to a smaller step size\n");
				} else {
					// Lower the head
					wprintw(wnd,"Setting Z to %.2f\n", z+zoffset);
					ASSERT(set_z(z,0,100.0f));
				}}
				break;
			case KEY_UP: {
				float z = get_z();
				z += step;
				if(z > 50.0f) {
					wprintw(wnd,"Warning: could not raise toolhead further, switch to a smaller step size\n");
				} else {
					// Raise the head
					wprintw(wnd,"Setting Z to %.2f\n", z+zoffset);
					ASSERT(set_z(z,0,100.0f));
				}}
				break;
			case '1':
			case KEY_F1:
				// Only move if we are not already at that corner
				if(pos == 0) break;
				wprintw(wnd, "Moving to corner 1 @ (%.1f,%.1f)\n", MIN_X, MIN_Y);

				// Store the current position of the Z axis
				zpos[pos] = get_z();
				wprintw(wnd, "Debug: %.2f %.2f %.2f %.2f\n", zpos[0], zpos[1], zpos[2], zpos[3]); wrefresh(wnd);
				// Set the current position to this one
				pos = 0;
				// Move the toolhead up to 0
				set_z(-zoffset);
				// Move to corner 1
				set_position(MIN_X,MIN_Y,-zoffset,0,xyspeed);
				// Lower toolhead to previous setting
				set_z(zpos[pos]);
				wprintw(wnd, "Ready @ Z=%.1f\n", zpos[pos]+zoffset);

				break;
			case '2':
			case KEY_F2:
				// Only move if we are not already at that corner
				if(pos == 1) break;
				wprintw(wnd, "Moving to corner 2 @ (%.1f,%.1f)\n", MAX_X, MIN_Y);

				// Store the current position of the Z axis
				zpos[pos] = get_z();
				wprintw(wnd, "Debug: %.2f %.2f %.2f %.2f\n", zpos[0], zpos[1], zpos[2], zpos[3]); wrefresh(wnd);
				// Set the current position to this one
				pos = 1;
				// Move the toolhead up to 0
				set_z(-zoffset);
				// Move to corner 1
				set_position(MAX_X,MIN_Y,-zoffset,0,xyspeed);
				// Lower toolhead to previous setting
				set_z(zpos[pos]);
				wprintw(wnd, "Ready @ Z=%.1f\n", zpos[pos]+zoffset);

				break;
			case '3':
			case KEY_F3:
				// Only move if we are not already at that corner
				if(pos == 2) break;
				wprintw(wnd, "Moving to corner 3 @ (%.1f,%.1f)\n", MAX_X, MAX_Y);

				// Store the current position of the Z axis
				zpos[pos] = get_z();
				wprintw(wnd, "Debug: %.2f %.2f %.2f %.2f\n", zpos[0], zpos[1], zpos[2], zpos[3]); wrefresh(wnd);
				// Set the current position to this one
				pos = 2;
				// Move the toolhead up to 0
				set_z(-zoffset);
				// Move to corner 1
				set_position(MAX_X,MAX_Y,-zoffset,0,xyspeed);
				// Lower toolhead to previous setting
				set_z(zpos[pos]);
				wprintw(wnd, "Ready @ Z=%.1f\n", zpos[pos]+zoffset);

				break;
			case '4':
			case KEY_F4:
				// Only move if we are not already at that corner
				if(pos == 3) break;
				wprintw(wnd, "Moving to corner 4 @ (%.1f,%.1f)\n", MIN_X, MAX_Y);

				// Store the current position of the Z axis
				zpos[pos] = get_z();
				// Set the current position to this one
				pos = 3;
				// Move the toolhead up to 0
				set_z(-zoffset);
				// Move to corner 1
				set_position(MIN_X,MAX_Y,-zoffset,0,xyspeed);
				// Lower toolhead to previous setting
				set_z(zpos[pos]);
				wprintw(wnd, "Ready @ Z=%.1f\n", zpos[pos]+zoffset);

				break;
			case KEY_F5:
				// Store current position
				zpos[pos] = get_z();

				wprintw(wnd, "Positions of each corner:\n");
				wprintw(wnd, "(%.1f,%.1f) => %.2f\n", MIN_X, MIN_Y, zoffset+zpos[0]);
				wprintw(wnd, "(%.1f,%.1f) => %.2f\n", MAX_X, MIN_Y, zoffset+zpos[1]);
				wprintw(wnd, "(%.1f,%.1f) => %.2f\n", MAX_X, MAX_Y, zoffset+zpos[2]);
				wprintw(wnd, "(%.1f,%.1f) => %.2f\n", MIN_X, MAX_Y, zoffset+zpos[3]);

				{
					int bc = 0;
					if(!utility_ask_int(wnd, "Which corner should be stationary? 1 to 4, 5 for average?", &bc, 1, 1, 5, 1)) {
						// Abort the calculation
						break;
					}

					if(bc >= 1 && bc <= 4)
						// Use one corner as a fixed point
						print_instructions(wnd, bc-1);
					else
						// Use the average between all corners
						print_instructions_avg(wnd);
				}

				// Erase the z positioning as the user will attempt to modify the alignment
				for(int i=0;i<4;i++) zpos[i] = -zoffset;
				// Move the head and present the bed
				set_z(-zoffset);
				set_position(0,0,-zoffset,0,xyspeed);
				// Barrier: wait for the move to complete before turning off the motors
				set_dwell(0);

				// Disable the motor hold
				disable_motor_hold();
				wprintw(wnd, "Motor hold disabled, press F6 to power on and home all axis\n");

				// Set the flag to force the user to press F6 or quit
				nopower = 1;
				break;
			case 410:
				// TODO: this is not working properly - fix when we figure out how to resize properly
				// Resize terminal event - get the current screen size
//				getmaxyx(stdscr,row,col);
//
//				// Resize the windows
//				wresize(serial_border, row-1, col/2);
//				wresize(serial_win, row-3, col/2-2);
//				wresize(wnd, row-1, col/2);
//				// Relocate the windows
//				wmove(serial_border, 0, col/2);
//				wmove(serial_win, 1, col/2+1);
				// Update the current screen layout
				refresh();
				// Redraw the box around the serial output
				//box(serial_border,0,0);
				//wrefresh(serial_border);

				break;
			default: // Unknown keypress
				wprintw(wnd,"Invalid key: %i\n", ch);
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
					wprintw(wnd, "Motor hold disabled, press F6 to power on and home all axis\n");
					break;
				case KEY_F6:
					// Disable the flag to return to normal alignment mode
					nopower = 0;
					// Tell the user what we are doing
					wprintw(wnd, "Homing all axis\n");
					home_xyz();
					// Override the Z offset again
					override_zpos(-zoffset);
					// Ready for another round
					wprintw(wnd, "Ready\n");
				default: // Unknown keypress
					wprintw(wnd,"Invalid key: %i\n", ch);
			}
		}

		// When we are all done, update the screen
		wrefresh(wnd);
	}

stop:
	// Remove the window for the serial output and its pointer
	destroy_win(serial_border);
	destroy_win(serial_win);
	serial_win = NULL;

	// Destroy the status panel
	destroy_win(wnd);

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


