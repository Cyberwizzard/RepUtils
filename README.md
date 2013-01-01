RepUtils
========

RepUtils is a package containing multiple small utility tools (in one program) to perform bed leveling, belt slack testing and other things.

Currently only the bed leveling mode is implemented.

Instructions
=======
Requirements:
- Linux based system (todo: cygwin version)
- GCC 4.x
- ncurses

When using *buntu:
'sudo apt-get install build-essentials libncurses5-dev'

To compile run:
'g++ -o reputils *.cc -lcurses'

Changelog
=======
0.2 - 2013-01-01
- Rewrote UI code to be more modular
- Support console resizing
- Clean up of various debug code
- Implemented serial DTR cycle to reset 3D printer when starting
- Modified startup to wait for printer start
- Improved levelling procedure and instructions

0.1 - 2012-12-30
- Initial version
- Hardcoded device strings and settings
- Bed levelling works
