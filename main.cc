/*
 * main.c
 *
 *  Created on: Dec 5, 2012
 *      Author: cyberwizzard
 */
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "serial.h"
#include "machine.h"
#include "calibratez.h"

#define _(x) ASSERT(x)

int yaxis_break_in();
int zaxis_break_in();
int pid_auto_tuning();

int main(void) {
	printf("RepRap Bed Level Tool by Berend Dekens\n");
	if(serial_open() < 0) return -1;
	printf("Opened serial port\n");

	calibratez_heightloop();
	//zaxis_break_in();
	//pid_auto_tuning();

	// Send a barrier command to the printer before shutting down
	set_dwell(100);
	// Close the serial port
	serial_close();
}

int pid_auto_tuning() {
	const int p = 8192, i = 512, d = 24576;
	double np = (p * 3) / 2048;
	double start_temp = 50;					// Starting temperature for the calibration
	double start_tolerance = 15.0;			// 3 degrees around the starting temperature is acceptable
	double dead_time_temp_delta = 10.0;		// When this threshold has been exceeded above the start, it marks the end of the dead time
	double test_temp = 150;					// Testing temperature
	double abort_temp = 220;				// If the process goes out of control, we abort at this temperature
	double temp = 0.0, temp_err = 100.0;	// Current temp and error from target
	int temp_ok_cnt = 0;					// Number of consecutive measurements where the temperature was ok
	int temp_ok_max = 30;					// Target before beginning

	FILE *fhp = fopen("pid_temp.plot", "w");
	if(fhp==NULL) {
		printf("Could not open gnuplot instruction file\n");
		return -1;
	}
	fprintf(fhp, "set size square\nset ylabel \"Temp (C)\"\nset xlabel \"Time (s)\"\n");
	fprintf(fhp, "set terminal png enhanced size 1920,1080\nset output \"pid_temp.png\"\n");
	fprintf(fhp, "plot \"pid_temp.data\" using 1:2 with lines\n");
	fclose(fhp);

	FILE *fh = fopen("pid_temp.data", "w");
	if(fh==NULL) {
		printf("Could not open temperature log file\n");
		return -1;
	}
	fprintf(fh, "# Time , Temperature (C)\n");

	print_pid();
	sleep(4);

	// Test before starting if we need to cool the printer
	get_temperature(0, &temp);
	// Sanity testing
	if(temp < 10.0 || temp > 250) {
		printf("Unsane temperature reported: %.0f degrees\n", temp);
		return -1;
	}

	// Make sure the heating is off
	set_temperature(0, 0.0);

	// Test if we should wait for the printer to cool first
//	if(temp > start_temp) {
//		printf("Hotend too warm: waiting for printer to cool down\n");
//		enable_fan(true);
//		while(temp > start_temp) {
//			sleep(1);
//			get_temperature(0, &temp);
//		}
//		enable_fan(false);
//	}

	printf("Switching to P-only control loop\n");
	set_pid_p(np);
	set_pid_i(0);
	set_pid_d(0);
	print_pid();

	printf("Setting testing temperature to %.0f degrees\n", test_temp);
	set_temperature(0, test_temp);

	printf("Waiting for printer to reach target temperature\n");
	get_temperature(0, &temp);
	while(temp < test_temp) {
		usleep(1000000);		// Delay 100 ms
		get_temperature(0, &temp);
	}

	serial_verbose(false);

	printf("Target reached, tuning P to obtain stable oscillation\n");

	int w_id = 0;
	const int w_max = 10;
	int w_period[w_max];		// Period of a single temperature oscillation wave
	double w_temp_min[w_max];	// Minimum temperature during wave
	double w_temp_max[w_max];	// Maximum temperature during wave

	int t_id = 0;
	const int t_max = 10000;
	double temps[t_max];
	double temp_min = test_temp, temp_max = test_temp;

	int t = 1;				// Time index in 10ms steps
	int t_start = -1;		// Time index of the rising flank of the wave
	int t_crossing = -1;	// Time index of the high to low crossing of the wave through the target temperature

	while(1) {
		// Read current temperature
		get_temperature(0, &temp);
		printf("\rT: %.2f C      ", temp);
		// Min/max logic to detect oscillation amplitude
		if(temp < temp_min) temp_min = temp;
		if(temp > temp_max) temp_max = temp;

		// Safety: if something goes wrong, shut down the test
		if(temp > abort_temp) {
			printf("\nWARNING: MAXIMUM TEMPERATURE REACHED - ABORTING AUTOTUNE\n");
			set_temperature(0, 0.0);
			fclose(fh);
			return -1;
		}

		// Detect a low to high crossing: this is a new cycle
		if(t_crossing != -1 && t_start != -1 && temp > test_temp) {
			printf("\nWave: %.2f s (%i/%i) - ", (double)(t - t_crossing) / 100.0, t_crossing - t_start, t - t_crossing);
			printf("Amplitude: %.2f C ([%.2f-%.2f])\n", temp_max-temp_min, temp_min, temp_max);

			// New oscillation wave: reset min/max
			temp_min = temp_max = test_temp;

			// Erase t_start to restart the detection
			t_start = -1;
		}

		if(t_start == -1 && temp > test_temp) {
			// Temperature rising edge of wave
			t_start = t;
			// Erase last high-low crossing
			t_crossing = -1;
		}
		if(t_crossing == -1 && t_start != -1 && temp < test_temp) {
			// Crossing from high to low through the target temperature
			t_crossing = t;
		}

		// Store the temperatures during the test
		if(t_id < t_max) {
			temps[t_id] = temp;
			t_id = (t_id+1) % t_max;
		}
		fprintf(fh, "%.1f	%.2f\n", (double)(t_id) / 100.0, temp);

		usleep(10000);	// 10ms
		t++;			// Forward time
	}
	fclose(fh);

	printf("\nPrinting recorded temperatures:\n");
	for(int j=0; j<t_id;j++) printf("%.1f, %.2f\n", (double)j/10.0, temps[j]);

	printf("Target temperature reached, shutting down heating\n");
	set_temperature(0,0.0);
	enable_fan(true);
	set_pid_i(i);
	set_pid_d(d);

	printf("Waiting for machine to cool down\n");
	while(temp > 30.0f) {
		sleep(1);
		get_temperature(0, &temp);
	}

	printf("PID tuning complete\n");

	return 0;
}

int zaxis_break_in() {
	const float speed_min = 50.0;
	const float speed_max = 200.0;
	float speed = speed_min;
	const float speed_step = 100.0;
	const int speed_cycles = 100;
	int cycles = 2;

	// Home the Z axis to get a clean start
	home_z();

	printf("Running break in program for new Z axis\n");

	for(int i=0;i<speed_cycles;i++) {
		speed = speed_min;
		cycles = 2;
		while(speed < speed_max) {
			// Home Z axis to make sure in case of missed steps we start anew
			printf("Home Z\n");
			home_z();

			// Run the carriage a couple of times up and down
			for(int j=0;j<cycles;j++) {
				printf("Running up and down (%i/%i)\n",j,cycles);
				set_z(30.0f, 0, speed);
				set_z(0.0f, 0, speed);
			}

			// Increase speed for next run
			speed *= 2.0f;
			cycles = cycles * 2;

			// Delay
			sleep(1);
		}
	}

	printf("Done\n");

	return 0;
}

int yaxis_break_in() {
	const float speed_min = 100.0;
	const float speed_max = 4000.0;
	float speed = 100.0;
	const float speed_step = 100.0;
	const int speed_cycles = 100;
	int cycles = 2;

	// Home the Y axis to get a clean start
	//home_y();

	printf("Running break in program for new axis\n");

	for(int i=0;i<speed_cycles;i++) {
		speed = speed_min;
		cycles = 2;
		while(speed < speed_max) {
			// Home Y axis to make sure in case of missed steps we start anew
			printf("Home Y\n");
			home_y();

			// Run the carriage a couple of times up and down
			for(int j=0;j<cycles;j++) {
				printf("Running up and down (%i/%i)\n",j,cycles);
				set_y(180.0f, 0, speed);
				set_y(0.0f, 0, speed);
			}

			// Increase speed for next run
			speed *= 2.0f;
			cycles = cycles * 2;

			// Delay
			sleep(1);
		}
	}

	printf("Done\n");

	return 0;
}

