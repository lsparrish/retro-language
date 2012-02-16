==========================================
Ngaro VM implementation for Arduino boards
==========================================

--------
Abstract
--------
Arduino[1]_ is an open-source electronics prototyping platform based on
flexible, easy-to-use hardware and software. It's intended for artists,
designers, hobbyists, and anyone interested in creating interactive objects or
environments.

This is the Ngaro VM implementation for this platform, which allowes to write
programms in Retro[2]_ langugage, instead of the official Arduino language.

-------------
Configuration
-------------

To configure and build the VM, you need to create a configuration script. The
easiest way for it, is to copy one from one of the prepared configuration files:

+------------------+-------------------------------------------------------+
| File name        | Configuration description                             |
+==================+=======================================================+
| make_mega2560.sh | For Arduino MEGA 2560 based boards, with many Retro   |
|                  | libraries and additions.                              |
+------------------+-------------------------------------------------------+
| make_mega328p.sh | For Arduino Nano 2.x/3.x, because of less RAM it is a |
|                  | reduced configuration.                                |
+------------------+-------------------------------------------------------+

This script needs to set following configuration variables:

+------------------+-------------------------------------------------------+
| Variable name    | Description                                           |
+==================+=======================================================+
| BOARD            | The board type:                                       |
|                  | * "mega2560" for the MEGA 2560 board                  |
|                  | * "mega328p" for the Nano board                       |
+------------------+-------------------------------------------------------+
| CFLAGS           | Flags for the compiler                                |
+------------------+-------------------------------------------------------+
| NATCFLAGS        | Flags for the native compiler.                        |
+------------------+-------------------------------------------------------+
| AVRCFLAGS        | Flags for the AVR compiler.                           |
+------------------+-------------------------------------------------------+
| IMAGE_BLOCK_SIZE | Size of blocks, in which the image is splitted in     |
|                  | program memory, use 9600 to not to split the image.   |
+------------------+-------------------------------------------------------+

---------
Footnotes
---------

.. [1] Read more about this project here: http://arduino.cc

.. [2] A concatenative, stack-based programming language with roots in Forth:
       http://retroforth.org
