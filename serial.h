/*
 * serial.h
 *
 *  Created on: Dec 5, 2012
 *      Author: cyberwizzard
 */

#ifndef SERIAL_H_
#define SERIAL_H_

int serial_open();
void serial_close();
int serial_cmd(const char *cmd, char **reply);
//int serial_cmd(const char *cmd);


#endif /* SERIAL_H_ */
