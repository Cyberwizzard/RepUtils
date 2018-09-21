/*
 * mesh_builder.c
 *
 *  Created on: Sept 14, 2018
 *      Author: cyberwizzard
 */

#include "mesh_builder.h"

#include <stdio.h>
#include <curses.h>

#include "main.h"
#include "machine.h"
#include "utility.h"
#include "tui.h"

// Mesh points
ty_meshpoint mesh [MESH_SIZE_Y][MESH_SIZE_X];

int mesh_builder_stepsize = 0;			// Step size for lowering or raising the head

void mesh_builder_print_mesh_status(WINDOW *wnd, int y, int x, int y_sel, int x_sel);


void mesh_builder_destroy_win(WINDOW *local_win)
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

void mesh_builder_print_status_bar(int row, int stepsize) {
	//const char *banner = "[AWSD] Move mesh point [F2] Fill Row [F3] Fill Column [F4] Fill All [Up/Down] Raise/lower head [Left/Right] Change step size: %s";
	const char *banner = "[F5] Download mesh [F6] Upload mesh [F10] Quit [AWSD] Move mesh point [Up/Down] Raise/lower head [Left/Right] Change step size: %s";
	const char *step0 = "[1mm] 0.1mm 0.01mm";
	const char *step1 = "1mm [0.1mm] 0.01mm";
	const char *step2 = "1mm 0.1mm [0.01mm]";
	const char *step = (stepsize==0?step0:((stepsize==1)?step1:step2));
	mvprintw(row, 0, banner, step);
	refresh();
}

int mesh_builder() {
	int keepgoing = 1;			// Flag to terminate the input loop
	int x_pos = 0;				// Current position within the mesh
	int y_pos = 0;				// Current position within the mesh
	int x_sel = 0;				// Selection in the mesh to potentially activate to calibrate next
	int y_sel = 0;				// Selection in the mesh to potentially activate to calibrate next
	float xyspeed = 4000.0f;	// Speed when moving from point to point in the mesh
	int ch;						// Integer to hold the key scancode during the input loop
	float zraise = 0.0f;		// How far should the head be raised during moves from corner to corner?
	float z_offset = 7.0f;		// The offset of the Z axis using G92, this allows us to get below the optoflag

	// Input loop variables
	mesh_builder_stepsize = 1;	// 0 = 1mm, 1 = 0.1mm, 2 = 0.01mm
	float step = 0.1f;

	// Inititialize the mesh array so the X and Y coordinates of each mesh point are known
	for (int y=0; y<MESH_SIZE_Y; y++) {
		for (int x=0; x<MESH_SIZE_X; x++) {
			mesh[y][x].valid = 0;

			mesh[y][x].x = MESH_MIN_X + ((float)x * (MESH_MAX_X - MESH_MIN_X)) / (MESH_SIZE_X - 1);
			mesh[y][x].y = MESH_MIN_Y + ((float)y * (MESH_MAX_Y - MESH_MIN_Y)) / (MESH_SIZE_Y - 1);
			mesh[y][x].z = 0.0f;
		}
	}

	// Start curses and all windows
	tui_init(1, &mesh_builder_print_status_bar);

	// Print the overview of mesh points
	mesh_builder_print_mesh_status(overview_win, y_pos, x_pos, y_sel, x_sel);

	// Start by homing all axis on the machine
	wprintw(cmd_win,"Homing all axis\n");
	wrefresh(cmd_win);
	//ASSERT(home_xyz());
	// Home all axis by raising Z first to avoid scraping the tape
	{
		ASSERT(override_zpos(0.0f));
		ASSERT(set_z(4.0f,0,MAX_SPEED_Z));
		ASSERT(home_xy());
		ASSERT(home_z());
	}

	// How far can the head be lowered from the Z end stop?
	{
		int zoi = 0;
		// int utility_ask_int(WINDOW *wnd, string q, int *ans, int def, int min, int max, int step)
		if(!utility_ask_int(cmd_win, "How far can the toolhead be lowered below the Z end-stop in mm?", &zoi, 10, 0, 20, 1)) goto stop;
		z_offset = (float)zoi;
		wprintw(cmd_win,"Using %.2f mm as the absolute lowest position\n", -z_offset);
		wrefresh(cmd_win);
	}

	{
		int zoi = 0;
		if(!utility_ask_int(cmd_win, "During XY moves, how far should the toolhead be raised compared to the Z end-stop?", &zoi, 0, 0, 10, 1)) goto stop;
		zraise = (float)zoi;
		wprintw(cmd_win,"Using %.2f mm as the repositioning height\n", zraise);
		wrefresh(cmd_win);
	}

	// Disable the bed levelling logic so the machine reverts to linear motion
	ASSERT(mesh_disable());

	// Disable the software end-stops so the machine can lower the head below the Z end-stop
	ASSERT(enable_soft_endstops(false));
	// Apply the Z offset so we can lower the toolhead during leveling (0 is now -z_offset below the Z end-stop)
	ASSERT(override_zpos(z_offset));

	// Move Z up to whatever the movement height is
	ASSERT(set_z(zraise+z_offset));
	// Move to point (0,0) in the mesh
	ASSERT(set_position(mesh[0][0].x,mesh[0][0].y,get_z(),0,xyspeed));

	// Input loop
	// Print the status bar
	mesh_builder_print_status_bar(LINES-1, mesh_builder_stepsize);
	wrefresh(overview_win);
	while(keepgoing) {
		int update = 0; // Flag to trigger the mesh Z height to be updated and the mesh overview refreshed

		if(mesh_builder_stepsize < 0 || mesh_builder_stepsize > 2) {
			wprintw(cmd_win,"Invalid step size detected: %i\n", mesh_builder_stepsize);
			goto stop;
		}

		// Read a keyboard press
		ch = getch();
		// Correct the scan code for special keys like the arrow keys (consisting of 2 keystrokes)
		if ( ch == 0 || ch == 224 )
		ch = 256 + getch();

		// Handle the key press

		switch(ch) {
		case 'q': // Quit the control loop
			wprintw(cmd_win,"Hint: to quit, press F10 (instead of Q)\n");

			break;
		case KEY_F10:
			keepgoing = 0;

			// Raise the head
			set_z(zraise+z_offset);
			// Move to origin
			set_position(0,0,get_z(),0,xyspeed);

			break;
		case KEY_LEFT:
			if(mesh_builder_stepsize > 0) mesh_builder_stepsize--;
			switch(mesh_builder_stepsize) {
				case 0: step = 1.0f; break;
				case 1: step = 0.1f; break;
				case 2: step = 0.01f; break;
			}
			// Print the status bar
			mesh_builder_print_status_bar(LINES-1, mesh_builder_stepsize);
			break;
		case KEY_RIGHT:
			if(mesh_builder_stepsize < 2) mesh_builder_stepsize++;
			switch(mesh_builder_stepsize) {
				case 0: step = 1.0f; break;
				case 1: step = 0.1f; break;
				case 2: step = 0.01f; break;
			}
			// Print the status bar
			mesh_builder_print_status_bar(LINES-1, mesh_builder_stepsize);
			break;
		case KEY_DOWN: {
			float z = get_z();
			z -= step;
			if(z < 0) {
				wprintw(cmd_win,"Warning: could not lower toolhead further, switch to a smaller step size\n");
			} else {
				// Lower the head
				wprintw(cmd_win,"Setting Z to %.2f\n", z-z_offset);
				ASSERT(set_z(z,0,MAX_SPEED_Z));
			}}
			// Toolhead moved, mark this point now as valid
			if(!mesh[y_pos][x_pos].valid) mesh[y_pos][x_pos].valid = 1;
			update = 1; // Update the mesh state
			break;
		case KEY_UP: {
			float z = get_z();
			z += step;
			if(z > 50.0f) {
				wprintw(cmd_win,"Warning: could not raise toolhead further, switch to a smaller step size\n");
			} else {
				// Raise the head
				wprintw(cmd_win,"Setting Z to %.2f\n", z-z_offset);
				ASSERT(set_z(z,0,MAX_SPEED_Z));
			}}
			// Toolhead moved, mark this point now as valid
			if(!mesh[y_pos][x_pos].valid) mesh[y_pos][x_pos].valid = 1;
			update = 1; // Update the mesh state
			break;
		case 'a':
			// Move 'left' on the mesh selection
			if(x_sel > 0) {
				x_sel--;
				update = 1; // Update the mesh state
			}
			break;
		case 'd':
			// Move 'right' on the mesh selection
			if(x_sel < MESH_SIZE_X-1) {
				x_sel++;
				update = 1; // Update the mesh state
			}
			break;
		case 'w':
			// Move 'up' on the mesh selection
			if(y_sel < MESH_SIZE_Y-1) {
				y_sel++;
				update = 1; // Update the mesh state
			}
			break;
		case 's':
			// Move 'down' on the mesh selection
			if(y_sel > 0) {
				y_sel--;
				update = 1; // Update the mesh state
			}
			break;
		case 10:
			// Enter key - switch selection
			if(x_sel != x_pos || y_sel != y_pos) {
				// Selection changed - get the current Z and store it (relatively to the Z end-stop, which was shifted up by z_offset)
				mesh[y_pos][x_pos].z = get_z() - z_offset;

				// Update position to selection
				x_pos = x_sel;
				y_pos = y_sel;
				wprintw(cmd_win, "Moving to mesh point (%i,%i) @ (%.1f,%.1f)\n", x_pos, y_pos, mesh[y_pos][x_pos].x, mesh[y_pos][x_pos].y);
				// Raise Z and move to new position
				set_z(zraise+z_offset);
				set_position(mesh[y_pos][x_pos].x,mesh[y_pos][x_pos].y,get_z(),0,xyspeed);
				set_z(mesh[y_pos][x_pos].z+z_offset);

				// Update the overview
				mesh_builder_print_mesh_status(overview_win, y_pos, x_pos, y_sel, x_sel);
			}
			break;
		case KEY_F5:
			{
				int zoi = 0;
				int errcode = 0;
				// Load the mesh from the printer
				wprintw(cmd_win, "Preparing to download mesh from printer...\n");
				// int utility_ask_int(WINDOW *wnd, string q, int *ans, int def, int min, int max, int step)
				if(!utility_ask_int(cmd_win, "Which mesh should be loaded from printer EEPROM? Use -1 to use the currently active mesh instead.", &zoi, -1, -1, 20, 1)) goto stop;

				// Download mesh from printer
				if((errcode = mesh_download(zoi, mesh, cmd_win))) {
					if(errcode == 1) {
						wprintw(cmd_win, "ERROR: No valid CSV line found in first 4kB of mesh response!\n");
					} else if(errcode == 3) {
						wprintw(cmd_win, "ERROR: Incorrect number of columns (X points) found! Expected: %i\n", MESH_SIZE_X);
					} if(errcode == 4) {
						wprintw(cmd_win, "ERROR: Incorrect number of rows (Y points) found! Expected: %i\n", MESH_SIZE_Y);
					} else {
						wprintw(cmd_win, "ERROR: Unknown error during download: %i\n", errcode);
					}
				} else {
					wprintw(cmd_win, "Downloaded mesh successfully\n");
				}

				// Redraw the mesh overview
				update = 1;

				// Ask to align the toolhead to get started
				bool ans = false;
				if(!utility_ask_bool(cmd_win, "Mesh loaded; move toolhead to loaded Z position?", &ans, false)) goto stop;
				if(ans) {
					// Move to current Z height in mesh
					wprintw(cmd_win,"Setting Z to %.2f\n", mesh[y_pos][x_pos].z);
					ASSERT(set_z(mesh[y_pos][x_pos].z+z_offset));
				} else {
					wprintw(cmd_win,"Not moving current Z height; downloaded mesh point updated\n");
				}

			}
			break;
		case KEY_F6:
			{
				int zoi = 0;
				int errcode = 0;
				// Load the mesh from the printer
				wprintw(cmd_win, "Preparing to upload mesh to printer...\n");
				// int utility_ask_int(WINDOW *wnd, string q, int *ans, int def, int min, int max, int step)
				if(!utility_ask_int(cmd_win, "Which mesh slot should the mesh be saved into printer EEPROM? Use -1 to only upload.", &zoi, -1, -1, 20, 1)) goto stop;

				// Upload mesh from printer
				if((errcode = mesh_upload(zoi, mesh, cmd_win))) {
					wprintw(cmd_win, "ERROR: Unknown error during download: %i\n", errcode);
				} else {
					wprintw(cmd_win, "Uploaded mesh successfully\n");
				}
			}
			break;
		case 410:
			// Resize event
			tui_resize();
			break;
		default: // Unknown keypress
			wprintw(cmd_win,"Invalid key: %i\n", ch);
		}

		// Update the mesh if requested
		if (update) {
			// Get the current Z and store it in the mesh (so the view will update)
			mesh[y_pos][x_pos].z = get_z() - z_offset;
			// Re-print the overview
			mesh_builder_print_mesh_status(overview_win, y_pos, x_pos, y_sel, x_sel);
		}

		// When we are all done, update the screen
		wrefresh(cmd_win);
		wrefresh(overview_win);
	}

stop:
	// Remove the window for the serial output and its pointer
	mesh_builder_destroy_win(serial_border);
	mesh_builder_destroy_win(serial_win);
	serial_win = NULL;

	// Destroy the status panel
	mesh_builder_destroy_win(cmd_win);

	endwin();
	printf("Mesh Builder terminated\n");

	return 0;
}

/**
 * Print the mesh status overview
 * @param wnd Window to print the question and feedback in
 * @param y active Y point in the mesh
 * @param x active X point in the mesh
 * @param y selected Y point in the mesh
 * @param x selected X point in the mesh
 */
void mesh_builder_print_mesh_status(WINDOW *wnd, int y, int x, int y_sel, int x_sel) {
	wprintw(wnd, "\nMesh size: %i (x) * %i (y)     - Mesh boundaries: (%0.2f, %0.2f) - (%0.2f, %0.2f)\n",
			MESH_SIZE_X, MESH_SIZE_Y, MESH_MIN_X, MESH_MIN_Y, MESH_MAX_X, MESH_MAX_Y);

	for (int yy = MESH_SIZE_Y - 1; yy >= 0; yy--) {
		for (int xx = 0; xx < MESH_SIZE_X; xx++) {
			if(mesh[yy][xx].valid) {
				if(x == xx && y == yy)
					wprintw(wnd, "[%6.2f] ", mesh[yy][xx].z);
				else if(x_sel == xx && y_sel == yy)
					wprintw(wnd, "<%6.2f> ", mesh[yy][xx].z);
				else
					wprintw(wnd, " %6.2f  ", mesh[yy][xx].z);
			} else {
				if(x == xx && y == yy)
					wprintw(wnd, "  [ * ]  ");
				else if(x_sel == xx && y_sel == yy)
					wprintw(wnd, "  < * >  ");
				else
					wprintw(wnd, "    *    ");
			}
		}
		wprintw(wnd, "\n");
	}
	wprintw(wnd, "Active point: [%i, %i] ", x, y);
	if (x != x_sel || y != y_sel)
		wprintw(wnd, "   -    Selected point: <%i, %i> (press enter to activate and move the head) ", x_sel, y_sel);
}
