/*
 * utility.h
 *
 *  Created on: Dec 31, 2012
 *      Author: cyberwizzard
 */

#ifndef UTILITY_H_
#define UTILITY_H_

#include <curses.h>
#include <assert.h>
#include <stdlib.h>
#include <cstdio>
#include <string>
#include <iostream>
#include <sstream>

using namespace std;

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
bool utility_ask_int(WINDOW *wnd, string q, int *ans, int def, int min = 0, int max = 999, int step = 1);

/**
 * Ask for an yes or no answer.
 * @param wnd The curses window to print the question in.
 * @param q String holding the question to ask.
 * @param ans A pointer to put the answer in, True for yes, False for no.
 * @param def The default value for the question, 0 for yes, any other value for no
 * @return False if the user pressed escape to abort the question.
 */
bool utility_ask_bool(WINDOW *wnd, string q, bool *ans, int def);

#endif /* UTILITY_H_ */
