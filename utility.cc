/*
 * utility.c
 *
 *  Created on: Dec 31, 2012
 *      Author: cyberwizzard
 */

#include "utility.h"

/**
 * Ask for an integer input.
 * @param wnd The curses window to print the question in.
 * @param q String holding the question to ask.
 * @param ans A pointer to put the answer in.
 * @param def The default value for the question, can be an illegal value to force the user to make a choice.
 * @param min The minimum value to accept.
 * @param max The maximum value to accept.
 * @param step A step size to apply from the minimum value, for example increments of 10.
 * @return False if the user pressed escape to abort the question.
 */
bool utility_ask_int(WINDOW *wnd, string q, int *ans, int def, int min, int max, int step) {
	assert(wnd!=NULL);
	assert(ans!=NULL);
	assert(min<max);
	assert(step > 0);

	do {
		wprintw(wnd, q.c_str());
		wprintw(wnd, " [%i]: ", def);
		wrefresh(wnd);

		bool endline = false;
		stringstream ss;
		do {
			// Read a keyboard press
			int ch = getch();
			// Correct the scan code for special keys like the arrow keys (consisting of 2 keystrokes)
			if ( ch == 0 || ch == 224 )
			ch = 256 + getch();

			if(ch>='0'&&ch<='9') {
				// Numeric entry
				ss << (char)ch;
				wprintw(wnd, "%c", ch);
				wrefresh(wnd);
			} else if(ch=='-') {
				// Only allow a dash when the string buffer is empty
				if(ss.str().length() == 0) {
					ss << '-';
					wprintw(wnd, "-");
					wrefresh(wnd);
				}
			} else if(ch == 13 || ch == 10) {
				// Enter, validate
				int val = (ss.str().length()>0) ? strtol(ss.str().c_str(), NULL, 10) : def;
				if(val < min || val > max) {
					wprintw(wnd, "\nInvalid value %i: please enter a number between %i and %i\n", val, min, max);
				} else if((val-min)%step != 0) {
					wprintw(wnd, "\nInvalid value %i: please enter a multiple of %i starting from %i\n", val, step, min);
				} else {
					// Assign value
					*ans = val;
					wprintw(wnd,"\n");
					wrefresh(wnd);
					return true;
				}

				endline = true;
			} else if(ch == 27) {
				// Escape, abort
				wprintw(wnd, "abort\n");
				wrefresh(wnd);
				return false;
			}
		} while(!endline);
	} while(1);
	return false;
}



