/*
 * serial.c
 *
 *  Created on: Dec 5, 2012
 *      Author: cyberwizzard
 */

#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include "main.h"
#include "serial.h"

#include <curses.h>
extern WINDOW *serial_win;

bool serial_ena_output = true;		// Utility switch to silence serial I/O

#define error_message(...) { if(serial_win==NULL) fprintf(stderr, __VA_ARGS__); \
							  else { wprintw(serial_win, __VA_ARGS__); wrefresh(serial_win); }}
#define message(...) { if(serial_ena_output) { if(serial_win==NULL) printf(__VA_ARGS__); \
						else { wprintw(serial_win, __VA_ARGS__); wrefresh(serial_win); }}}

int serial_fd = 0;

// http://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
// http://www.easysw.com/~mike/serial/serial.html#3_1_1
int set_interface_attribs(int fd, int speed, int parity) {
	if(DEMO_MODE) {
		// Demo mode - pretend we set attributes
		return 0;
	}

	struct termios tty;
	memset(&tty, 0, sizeof tty);

	// Get terminal attributes from the file descriptor
	if (tcgetattr(fd, &tty) != 0) {
		error_message("error %d from tcgetattr\n", errno);
		return -1;
	}

	// Set terminal input and output speed
	// TODO: Can this actually differ between I and O?
	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK;         // ignore break signal
	tty.c_lflag = 0;                // no signaling chars, no echo,
									// no canonical processing
	tty.c_oflag = 0;                // no remapping, no delays
	tty.c_cc[VMIN] = 0;            // read doesn't block
	//tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout
	tty.c_cc[VTIME] = 0;            // no timeout

	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD); // ignore modem controls,
									 // enable reading
	tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
	tty.c_cflag |= parity;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		error_message("error %d from tcsetattr\n", errno);
		return -1;
	}
	return 0;
}

/**
 * Set DTR (Data Transfer Ready) to low and high again - this simulates a modem
 * hangup and should power cycle the printer on the other end.
 */
void set_reset_dtr() {
	int status = 0;
	// Fetch the status from the file descriptor
    ioctl(serial_fd, TIOCMGET, &status);
    // Mask the DTR bit so DTR will get low
    status &= ~TIOCM_DTR;
    ioctl(serial_fd, TIOCMSET, &status);

    // Wait...
    usleep(100);

    // Set the DTR bit high again
    status |= TIOCM_DTR;
    ioctl(serial_fd, TIOCMSET, &status);
}

/**
 * When enabling blocking, a read() on the serial port (terminal) will block until
 * data is available.
 */
void set_blocking(int fd, int should_block) {
	if(DEMO_MODE) {
		// Demo mode - pretend we set attributes
		return;
	}
	struct termios tty;
	memset(&tty, 0, sizeof tty);
	// Get terminal attributes
	if (tcgetattr(fd, &tty) != 0) {
		error_message("error %d from tggetattr\n", errno);
		return;
	}

	// Enable blocking
	tty.c_cc[VMIN] = should_block ? 1 : 0;
	// Set a timeout to prevent hanging indefinitely
	//tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	if (tcsetattr(fd, TCSANOW, &tty) != 0)
		error_message("error %d setting term attributes\n", errno);
}

int serial_open() {
	if(DEMO_MODE) {
		// Demo mode - pretend we opened a port
		return 0;
	}

	const char *portname = "/dev/ttyUSB0";

	serial_fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
	if (serial_fd < 0) {
		error_message("error %d opening %s: %s\n", errno, portname, strerror (errno));
		return -1;
	}

	set_interface_attribs(serial_fd, B115200, 0);	// set speed to 115,200 bps, 8n1 (no parity)
	set_blocking(serial_fd, 1);               		// set blocking

	// toggle the DTR line to trigger a reset at the printer (most hardware supports this)
	set_reset_dtr();

	message("Serial port opened - waiting for printer to start\n");
	return serial_waitforok(false, 30);
}

void serial_close() {
	if(serial_fd > 0) {
		close(serial_fd);
		serial_fd = 0;
	}
}

int serial_cmd(const char *cmd, char **reply) {
	if(DEMO_MODE) {
		// Demo mode - pretend we send the command
		message("> %s", cmd);
		message("DEMO MODE: command ok\n");
		return 0;
	}

	const unsigned int buflen = 1000;
	char buf [buflen+1]; // Ugly hack (+1) to make sure the buffer is always null-terminated
	unsigned int bp = 0;		// Buffer pointer, points to the end of the data in the buffer

	if(serial_fd <= 0) {
		error_message("error: serial port not open\n");
		return -1;
	}

	// As data might be in the serial buffer (for example because of debug output or
	// start of the printer), flush (clear) the buffer.
	tcflush(serial_fd, TCIFLUSH);

	// Show what command we will send (no need for a \n)
	message("> %s", cmd);
	// Send command
	write(serial_fd, cmd, strlen(cmd));
	// Now loop until we read an 'ok' in the stream - this signals that
	// the command was accepted.
	while(bp < buflen) {
		int br=read(serial_fd, &buf[bp], buflen-bp);
		// Debug: show whats in the buffer between reads
		//message("debug (buf): %s\n", buf);
		//for(unsigned int i=0;i<buflen;i++) {
		//	if(buf[i] == 0x0) break;
		//	message("%02X ", buf[i]);
		//}
		//message("\n");
		if(br==0) {
			error_message("error: stream closed during read\n");
			return -1;
		}
		if(br < 0) {
			error_message("error while reading from port: %s (%i)\n", strerror (errno), errno);
			return -1;
		}
		bp+=br;
		buf[bp] = 0x0;	// Always make sure its \0 terminated to make a valid string

		// Scan the buffer contents if there is at least space for 'ok' and it has a line ending.
		// Without a line ending, the reply of the printer is incomplete.
		while (bp > 2 && strchr(buf, '\n') != NULL){
			// It should start with ok; if not, we discard the line.
			if(strcasestr(buf,"ok") != buf) {
				// This line does not start with 'ok': search for a newline.
				int lineend = -1;
				for(unsigned int i=0;i<bp;i++) {
					// Scan up to the end of the data in the buffer, or \0 (which should not happen)
					if(buf[i]==0x0) break;
					if(buf[i]=='\n') {
						lineend = i;
						break;
					}
				}

				// Test if a line ending was found
				if(lineend >= 0) {
					// Print the discarded data
					message("* %.*s\n", lineend, buf);

					// Now move all bytes after the lineend in the buffer
					for(unsigned int i=lineend+1;i<bp;i++)
						buf[i-lineend-1] = buf[i];
					// Adjust pointer and ending
					bp = bp - lineend - 1;

					// Erase the end of the buffer
					//for(unsigned int i=bp;i<buflen;i++) buf[i] = 0;
					// Adjust pointer and ending
					unsigned int old_bp = bp;
					bp = bp - lineend - 1;

					// Erase the end of the buffer
					for(unsigned int i=bp;i<=old_bp;i++) buf[i] = 0x0;
				}
			} else {
				// Print the reply
				message("< %s\n", buf);

				// Buffer starts with 'ok' and has a line ending, return the buffer.
				if(reply != NULL) {
					*reply = strdup(buf);
				}
				return 0;
			}
		}
	}

	error_message("error: reply buffer overflow\n");
	return -1;
}

/**
 * When the printer reboots (when the serial port is opened and reset), it will send a
 * starting banner. For example Teacup starts with "start" followed by "ok".
 * Using this function we wait for that unsolicited 'ok' before continuing.
 * @param flush Flush the current serial buffer before waiting for 'ok'
 * @param timeout Timeout in seconds before canceling the wait
 */
int serial_waitforok(bool flush, int timeout) {
	// Alternative: use an ioctl to see if theres data:
	// ioctl(serial_file_descriptor, FIONREAD, &bytes_available);
	fd_set fds;			// Used for the select() to wait for data on the serial port
	FD_ZERO(&fds);		// Clear the struct
	FD_SET(serial_fd, &fds);

	if(timeout < 0) timeout = 0;
	else if(timeout > 30) timeout = 30;
	struct timeval timer;
	timer.tv_usec = 0;
	timer.tv_sec = timeout;

	if(DEMO_MODE) {
		// Demo mode
		message("DEMO MODE: startup ok\n");
		return 0;
	}

	const unsigned int buflen = 1000;
	char buf [buflen+1]; // Ugly hack (+1) to make sure the buffer is always null-terminated
	unsigned int bp = 0;		// Buffer pointer, tracks the number of bytes used in the buffer

	if(serial_fd <= 0) {
		error_message("error: serial port not open\n");
		return -1;
	}

	// As data might be in the serial buffer (for example because of debug output or
	// start of the printer), flush (clear) the buffer.
	if(flush) tcflush(serial_fd, TCIFLUSH);

	// Now loop until we read an 'ok' in the stream - this signals that
	// the command was accepted.
	while(bp < buflen) {
		// Wait for data to arrive in the serial buffer
		int n = select(serial_fd+1, &fds, NULL, NULL, &timer);
		if (n < 0) {
			error_message("error: select failed - wait aborted\n");
			sleep(5);
			return -1;
		} else if (n == 0) {
			error_message("error: no response from printer\n");
			sleep(5);
			return -1;
		}

		// Read from the serial descriptor
		int br=read(serial_fd, &buf[bp], buflen-bp);
		if(br==0) {
			error_message("error: stream closed during read\n");
			return -1;
		}
		if(br < 0) {
			error_message("error while reading from port: %s (%i)\n", strerror (errno), errno);
			return -1;
		}
		bp+=br;			// Move the buffer pointer
		buf[bp] = 0x0;	// Always make sure its \0 terminated to make a valid string

		// Scan the buffer contents if there is at least space for 'ok' and it has a line ending.
		// Without a line ending, the reply of the printer is incomplete.
		while (bp > 2 && strchr(buf, '\n') != NULL){
			// It should start with ok; if not, we discard the line.
			if(strcasestr(buf,"ok") != buf) {
				// This line does not start with 'ok': search for a newline.
				int lineend = -1;
				for(unsigned int i=0;i<bp;i++) {
					// Scan up to the end of the data in the buffer, or \0 (which should not happen)
					if(buf[i]==0x0) break;
					if(buf[i]=='\n') {
						lineend = i;
						break;
					}
				}

				// Test if a line ending was found
				if(lineend >= 0) {
					// Print the discarded data
					message("* %.*s", lineend, buf);

					// Now move all bytes after the lineend in the buffer to
					// the beginning of the byte buffer.
					for(unsigned int i=lineend+1;i<bp;i++) {
						buf[i-lineend-1] = buf[i];
					}
					// Adjust pointer and ending
					unsigned int old_bp = bp;
					bp = bp - lineend - 1;

					// Erase the end of the buffer
					for(unsigned int i=bp;i<=old_bp;i++)
						buf[i] = 0x0;
				}
			} else {
				// Print the reply
				message("< '%s'", buf);
				return 0;
			}
		}
	}

	error_message("error: reply buffer overflow\n");
	return -1;
}

int serial_cmd(const char *cmd) {
	return serial_cmd(cmd, NULL);
}

void serial_verbose(bool b) {
	serial_ena_output = b;
}
