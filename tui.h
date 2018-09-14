/*
 * tui.h - Standard text UI (tui) for most of the tool. Uses 2 columns and a status bar.
 * The status bar provides feedback over some available keys. The left column provides a
 * console for user input and status info. The right column is used for raw serial output.
 *
 *  Created on: Jan 1, 2013
 *      Author: Berend Dekens
 */

#ifndef TUI_H_
#define TUI_H_

#include <cstdio>
#include <cstdlib>
#include <ncurses.h>
#include <assert.h>

extern WINDOW *serial_win;		// Window for the serial communication to be shown in (shared window)
extern WINDOW *serial_border;	// Window to hold the border for the serial communication
extern WINDOW *cmd_win;			// Window to hold and show the commands for the program

#define MIN_WIDTH 40
#define MIN_HEIGHT 8

// Utility macros to guard some curses functions and do a proper dump to aid debugging
#define MVWIN(_win,_row,_col) if(mvwin(_win, _row, _col)==ERR) { \
		endwin(); \
		fprintf(stderr, "mvwin failed at " __FILE__ ":%d with args(%s, %d, %d)\n", __LINE__, #_win, _row, _col); \
		fprintf(stderr, "Debug info: screen size: %ix%i\n", COLS, LINES); \
		abort(); \
	}

#define WRESIZE(_win,_h,_w) if(wresize(_win, _h, _w)==ERR) { \
		endwin(); \
		fprintf(stderr, "wresize failed at " __FILE__ ":%d with args(%s, %d, %d)\n", __LINE__, #_win, _h, _w); \
		fprintf(stderr, "Debug info: screen size: %ix%i\n", COLS, LINES); \
		abort(); \
	}

typedef void (*t_print_status_bar)(int,int);

/**
 * Load curses and create all windows
 */
void tui_init(int three_wnd_mode, t_print_status_bar func_ptr);

/**
 * End curses and destroy all windows
 */
void tui_deinit();

/**
 * Resize the current TUI - will move, resize and redraw all windows
 */
void tui_resize();

#endif /* TUI_H_ */
