==================
Retro for RetroBSD
==================

--------
Overview
--------
This is an implementation of Retro for use with RetroBSD.


-----------
Constraints
-----------
This build uses a 16-bit image and provides 20k cells of memory. The
default image used is running ^diet'extreme to reduce the number of
string buffers to 4 and cap string length at 80 characters.


----------
Setting Up
----------
Run *make*. This will generate an image suitable for low-memory devices
and place it in the **retro-src** directory. Copy the **retro-src**
directory to the **src** directory in your RetroBSD source tree. Switch
to that, then run **make**.

