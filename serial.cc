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

#include <curses.h>
extern WINDOW *serial_win;

#define error_message(...) { if(serial_win==NULL) fprintf(stderr, __VA_ARGS__); \
							  else { wprintw(serial_win, __VA_ARGS__); wrefresh(serial_win); }}
#define message(...) { if(serial_win==NULL) printf(__VA_ARGS__); \
						else { wprintw(serial_win, __VA_ARGS__); wrefresh(serial_win); }}

int serial_fd = 0;

// http://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
// http://www.easysw.com/~mike/serial/serial.html#3_1_1
int set_interface_attribs (int fd, int speed, int parity) {
        struct termios tty;
        memset (&tty, 0, sizeof tty);

        // Get terminal attributes from the file descriptor
        if (tcgetattr(fd, &tty) != 0)
        {
                error_message ("error %d from tcgetattr\n", errno);
                return -1;
        }

        // Set terminal input and output speed
        // TODO: Can this actually differ between I and O?
        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // ignore break signal
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        //tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout
        tty.c_cc[VTIME] = 0;            // no timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                error_message ("error %d from tcsetattr\n", errno);
                return -1;
        }
        return 0;
}

/**
 * When enabling blocking, a read() on the serial port (terminal) will block until
 * data is available.
 */
void set_blocking (int fd, int should_block) {
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        // Get terminal attributes
        if (tcgetattr (fd, &tty) != 0)
        {
                error_message ("error %d from tggetattr\n", errno);
                return;
        }

        // Enable blocking
        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        // Set a timeout to prevent hanging indefinitely
        //tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                error_message ("error %d setting term attributes\n", errno);
}

int serial_open() {
	const char *portname = "/dev/ttyUSB0";

	serial_fd = open (portname, O_RDWR | O_NOCTTY | O_SYNC);
	if (serial_fd < 0)	{
	        error_message ("error %d opening %s: %s\n", errno, portname, strerror (errno));
	        return -1;
	}

	set_interface_attribs (serial_fd, B115200, 0);  // set speed to 115,200 bps, 8n1 (no parity)
	set_blocking (serial_fd, 1);                // set blocking

	message("Serial port opened - sleeping to allow printer to restart\n");
	sleep(5);

	return 0;
}

void serial_close() {
	if(serial_fd > 0) {
		close(serial_fd);
		serial_fd = 0;
	}
}

int serial_cmd(const char *cmd, char **reply) {
	const unsigned int buflen = 1000;
	char buf [buflen+1]; // Ugly hack (+1) to make sure the buffer is always null-terminated
	unsigned int bp = 0;

	if(serial_fd <= 0) {
		error_message("error: serial port not open\n");
		return -1;
	}

	// As data might be in the serial buffer (for example because of debug output or
	// start of the printer), flush the buffer.
//	fd_set fds;	FD_ZERO(&fds); FD_SET(serial_fd,&fds);
//	struct timeval timeout = {1,0};
//	if (select(serial_fd+1, &fds, 0, 0, &timeout)==1) {
//		read(serial_fd, buf, buflen);
//		message("* %s\n", buf);
//	}
//	// Clear the buffer
//	memset(buf, 0, sizeof(buf));
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
					message("* %.*s\n", buf, lineend);
					//write(1 /* std out */, buf, lineend);
					//printf("\n");

					// Now move all bytes after the lineend in the buffer
					for(unsigned int i=lineend+1;i<bp;i++) {
						buf[i-lineend-1] = buf[i];
					}
					// Adjust pointer and ending
					bp -= lineend - 1;

					// Erase the end of the buffer
					for(unsigned int i=bp;i<buflen;i++) buf[i] = 0;
				}
			} else {
				// Print the reply, no need for a '\n' as the buffer has that
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

int serial_cmd(const char *cmd) {
	return serial_cmd(cmd, NULL);
}
