/*
 * tui.cc - Standard text UI (tui) for most of the tool. Uses 2 columns and a status bar.
 * The status bar provides feedback over some available keys. The left column provides a
 * console for user input and status info. The right column is used for raw serial output.
 *
 *  Created on: Jan 1, 2013
 *      Author: Berend Dekens
 */

#include "tui.h"
#include "main.h"

WINDOW *serial_win          = NULL;		// Window for the serial communication to be shown in (shared window)
WINDOW *serial_border       = NULL;		// Window to hold the border for the serial communication
WINDOW *cmd_win             = NULL;		// Window to hold and show the commands for the program
WINDOW *overview_border     = NULL;		// Window to hold the border for the overview window (3 window mode)
WINDOW *overview_win        = NULL;		// Window to hold and show the overview window (3 window mode)

// Defined in the level_bed routines - TODO: convert this into a hook to switch status bars on the fly
//extern void print_status_bar(int row, int stepsize);
extern int stepsize;

// Hook to the function drawing the status bar (needed for resizing)
t_print_status_bar func_print_status_bar = NULL;

int three_wnd_mode = 0;

/**
 * Load curses and create all windows
 */
void tui_init(int _three_wnd_mode, t_print_status_bar func_ptr) {
	int col, row;
	three_wnd_mode = _three_wnd_mode;
	int o_rows = three_wnd_mode ? MESH_SIZE_Y + 4 : 0; // OVerview rows (incl border), only exists in 3 window mode

	// Store the hook to the function which is drawing the status bar
	func_print_status_bar = func_ptr;

	// Init ncurses
	initscr();					// Initialize the terminal for use with ncurses
	noecho();					// Do not echo the characters as they are typed
	keypad(stdscr, TRUE);		// The standard screen sends arrow keys as key strokes

	// Do the window init
	getmaxyx(stdscr,row,col);

	// To retain the borders on a window, we first define a window 2 columns and rows larger
	// than the window for the serial output and draw a box in it.
	serial_border = newwin(row-o_rows-1, col/2, o_rows, col/2); // height, width, y, x
	// Create a window for the serial output to be shown during operation
	serial_win = newwin(row-o_rows-3, col/2-2, o_rows+1, col/2+1); // height, width, y, x
	// When inserting a new line at the end of the window, move the cursor one line up
	// (instead of returning to the beginning of the last line)
	scrollok(serial_win, true);
	idlok(serial_win, true);

	// Create a status window for user interaction
	cmd_win = newwin(row-o_rows-1, col/2, o_rows, 0); // height, width, y, x
	// When inserting a new line at the end of the window, move the cursor one line up
	// (instead of returning to the beginning of the last line)
	scrollok(cmd_win, true);
	idlok(cmd_win, true);

	if(three_wnd_mode) {
		// To retain the borders on a window, we first define a window 2 columns and rows larger
		// than the window for the serial output and draw a box in it.
		overview_border = newwin(o_rows, col, 0, 0); // height, width, y, x
		// Create a window for the serial output to be shown during operation
		overview_win = newwin(o_rows-2, col-2, 1, 1); // height, width, y, x
		// When inserting a new line at the end of the window, move the cursor one line up
		// (instead of returning to the beginning of the last line)
		scrollok(overview_win, true);
		idlok(overview_win, true);
	}

	// Update the screen to generate the windows and draw some boxes in them
	refresh();
	box(serial_border, 0, 0);
	wrefresh(serial_border);
	if(three_wnd_mode) {
		box(overview_border, 0, 0);
		wrefresh(overview_border);
	}
}

/**
 * End curses and destroy all windows
 */
void tui_deinit() {

}

/**
 * Resize the current TUI - will move, resize and redraw all windows
 */
void tui_resize() {
	int col, row;
	// Resize terminal event - get the current screen size
	getmaxyx(stdscr, row, col);

	// Only resize the TUI if we have a sane size
	if(row < MIN_HEIGHT || col < MIN_WIDTH) return;

	wprintw(cmd_win, "Resize detected, new size: %i (rows) x %i (cols)\n", row, col);
	wrefresh(cmd_win);

	int o_rows = three_wnd_mode ? MESH_SIZE_Y + 4 : 0; // OVerview rows (incl border), only exists in 3 window mode
	int serstart = (col / 2) - 1;
	int cmdwidth = col / 2;
	int serwidth = col - cmdwidth;
	int serheight = row - o_rows - 1;		// -1 as the last line is key help

	// Re-init curses - otherwise the hooks on the terminal are for the wrong screen size
	endwin();
	refresh();

	// Resize all windows
	WRESIZE(serial_border, serheight, serwidth);
	WRESIZE(serial_win, serheight-2, serwidth-2);
	WRESIZE(cmd_win, serheight, cmdwidth);
	if (three_wnd_mode) {
		WRESIZE(overview_border, o_rows, col);
		WRESIZE(overview_win,    o_rows-2, col-2);
	}

	// Move the serial windows
	MVWIN(serial_border, o_rows,   serstart);
	MVWIN(serial_win,    o_rows+1, serstart+1);

	// Mark the content as dirty so it gets redrawn completely (boxed windows done later)
	touchwin(cmd_win);

	// At this point all windows should be within the physical border again;
	// if we update the geometry it should be valid (within the terminal).
	assert(wnoutrefresh(serial_border)!=ERR);
	assert(wnoutrefresh(serial_win)!=ERR);
	assert(wnoutrefresh(cmd_win)!=ERR);
	if (three_wnd_mode) {
		assert(wnoutrefresh(overview_border)!=ERR);
		assert(wnoutrefresh(overview_win)!=ERR);
	}

	// Draw the border around the serial window again
	assert(wclear(serial_border)!=ERR);
	assert(box(serial_border, 0, 0)!=ERR);
	assert(wnoutrefresh(serial_border)!=ERR);
	if (three_wnd_mode) {
		assert(wclear(overview_border)!=ERR);
		assert(box(overview_border, 0, 0)!=ERR);
		assert(wnoutrefresh(overview_border)!=ERR);
	}


	// Because serial_border and serial_win are stacked, we just clobbered serial_win
	// and we need to mark the whole window as dirty to force a redraw
	touchwin(serial_win);
	assert(wnoutrefresh(serial_win)!=ERR);
	if(three_wnd_mode) {
		touchwin(overview_win);
		assert(wnoutrefresh(overview_win)!=ERR);
	}

	// Done - blit the virtual buffer to screen
	doupdate();

	// Reprint the status bar
	//print_status_bar(row - 1, stepsize);
	(*func_print_status_bar)(row - 1, stepsize);
}


