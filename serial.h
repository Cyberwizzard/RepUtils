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

/**
 * When the printer reboots (when the serial port is opened and reset), it will send a
 * starting banner. For example Teacup starts with "start" followed by "ok".
 * Using this function we wait for that unsolicited 'ok' before continuing.
 * @param flush Flush the current serial buffer before waiting for 'ok'
 * @param timeout Timeout in seconds before canceling the wait
 */
int serial_waitforok(bool flush, int timeout);

/**
 * Set DTR (Data Transfer Ready) to low and high again - this simulates a modem
 * hangup and should power cycle the printer on the other end.
 */
void set_reset_dtr();

void serial_verbose(bool b);


#endif /* SERIAL_H_ */
