/*
 * tui.cc - Standard text UI (tui) for most of the tool. Uses 2 columns and a status bar.
 * The status bar provides feedback over some available keys. The left column provides a
 * console for user input and status info. The right column is used for raw serial output.
 *
 *  Created on: Jan 1, 2013
 *      Author: Berend Dekens
 */

#include "tui.h"

WINDOW *serial_win = NULL;		// Window for the serial communication to be shown in (shared window)
WINDOW *serial_border = NULL;	// Window to hold the border for the serial communication
WINDOW *cmd_win = NULL;			// Window to hold and show the commands for the program

// Defined in the calibratez routines - TODO: convert this into a hook to switch status bars on the fly
extern void print_status_bar(int row, int stepsize);
extern int stepsize;

/**
 * Load curses and create all windows
 */
void tui_init() {
	int col, row;
	// Init ncurses
	initscr();					// Initialize the terminal for use with ncurses
	noecho();					// Do not echo the characters as they are typed
	keypad(stdscr, TRUE);		// The standard screen sends arrow keys as key strokes

	// Do the window init
	getmaxyx(stdscr,row,col);

	// To retain the borders on a window, we first define a window 2 columns and rows larger
	// than the window for the serial output and draw a box in it.
	serial_border = newwin(row-1, col/2, 0, col/2);
	// Create a window for the serial output to be shown during operation
	serial_win = newwin(row-3, col/2-2, 1, col/2+1);
	// When inserting a new line at the end of the window, move the cursor one line up
	// (instead of returning to the beginning of the last line)
	scrollok(serial_win, true);
	idlok(serial_win, true);

	// Create a status window for user interaction
	cmd_win = newwin(row-1, col/2, 0, 0);
	// When inserting a new line at the end of the window, move the cursor one line up
	// (instead of returning to the beginning of the last line)
	scrollok(cmd_win, true);
	idlok(cmd_win, true);

	// Update the screen to generate the windows and draw some boxes in them
	refresh();
	box(serial_border, 0, 0);
	wrefresh(serial_border);
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

	wprintw(cmd_win, "Resize detected, new size: %i x %i\n", row, col);
	wrefresh(cmd_win);

	const int serstart = (col / 2) - 1;
	const int cmdwidth = col / 2;
	const int serwidth = col - cmdwidth;
	const int serheight = row - 1;		// -1 as the last line is key help

	// Re-init curses - otherwise the hooks on the terminal are for the wrong screen size
	endwin();
	refresh();

	// Resize all windows
	WRESIZE(serial_border, serheight, serwidth);
	WRESIZE(serial_win, serheight-2, serwidth-2);
	WRESIZE(cmd_win, serheight, cmdwidth);

	// Move the serial windows
	MVWIN(serial_border, 0, serstart);
	MVWIN(serial_win, 1, serstart+1);

	// Mark the content as dirty so it gets redrawn completely
	touchwin(cmd_win);

	// At this point all windows should be within the physical border again;
	// if we update the geometry it should be valid (within the terminal).
	assert(wnoutrefresh(serial_border)!=ERR);
	assert(wnoutrefresh(serial_win)!=ERR);
	assert(wnoutrefresh(cmd_win)!=ERR);

	// Draw the border around the serial window again
	assert(wclear(serial_border)!=ERR);
	assert(box(serial_border, 0, 0)!=ERR);
	assert(wnoutrefresh(serial_border)!=ERR);

	// Because serial_border and serial_win are stacked, we just clobbered serial_win
	// and we need to mark the whole window as dirty to force a redraw
	touchwin(serial_win);
	assert(wnoutrefresh(serial_win)!=ERR);

	// Done - blit the virtual buffer to screen
	doupdate();

	// Reprint the status bar
	print_status_bar(row - 1, stepsize);
}


