# Dynamic Memory Allocator

Author: Khiem Vuong

Built as a project for CSci 2021, Fall 2018 at University of Minnesota, Twin Cities.

Professor in charge: Stephen McCamant

Performance (in comparison with standard C library -- built and run successfully on UMN CSE Lab Machine):
- Space utilization: 95%
- Throughput: 100% (exact ratio: 1.24)

Feel free to take the code for your reference, just give proper credits.

## Instructions 

CS:APP Malloc Lab
Handout files for students
Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
May not be used, modified, or copied without permission.

### Main Files:

mm.{c,h}	
	Your solution malloc package. mm.c is the file that you
	will be handing in, and is the only file you should modify.

mm-implicit.c
	A sample implementation similar to the one in the textbook,
        which you are allowed to copy code and ideas from.

mdriver.c	
	The malloc driver that tests your mm.c file

short{1,2}-bal.rep
	Two tiny tracefiles to help you get started. 

Makefile	
	Builds the driver

simulate-speed.pl
	Script to measured simulated throughput performance using
	Valgrind Callgrind


### Other support files for the driver

config.h	Configures the malloc lab driver
fsecs.{c,h}	Wrapper function for the different timer packages
clock.{c,h}	Routines for accessing the Pentium and Alpha cycle counters
fcyc.{c,h}	Timer functions based on cycle counters
ftimer.{c,h}	Timer functions based on interval timers and gettimeofday()
memlib.{c,h}	Models the heap and sbrk function

### Building and running the driver

To build the driver, type "make" to the shell.

To run the driver on a tiny test trace:

	unix> mdriver -V -f short1-bal.rep

The -V option prints out helpful tracing and summary information.

To get a list of the driver flags:

	unix> mdriver -h

