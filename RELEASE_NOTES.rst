=============
Release Notes
=============


----------
Retro 11.4
----------


core language / standard image
==============================

  - clean ups and refactorings
    - {{ and }}
    - :is and :devector
    - each@
    - <puts>


libraries
=========

  - d' renamed to decimal' (resolve naming conflict)
  - add queries'
  - add dump'
  - add fixed'
  - add double'
  - add unsigned'
  - add introspection'


vm
==

  - add retro-curses


website
=======

  - fix broken links





----------
Retro 11.3
----------


Compatibility Issues
====================

This release of Retro brings a number of changes that may require small
alterations to existing sources. Specifically:

  - maximum string length is now 256 cells by default
  - hidden functions are no longer revectorable

The first is not a big deal. It's now possible to alter both the maximum
string length and the number of temporary buffers. To restore a 512 cell
size as in 11.2:

::

  here 512 allot constant <512-TIB>
  [ <512-TIB> ] is tib
  [ 512 ] is STRING-LENGTH

The second change shouldn't be too critical. Basically it means that this
will no longer work:

::

  {{
    : foo  ( - ) 50 ;
  ---reveal---
    : bar1  foo foo + ;
    [ 100 ] is foo
  }}

Any private definitions are no longer revectorable. If you have no
**---reveal---** in a namespace, all functions are non-revectorable. This
won't break anything in the standard language or libraries, but could
be a problem if you rely on the old behavior in your code.


core language / standard image
==============================

  - new method of implementing quotes
  - maximum string length can be altered now
  - number of string buffers can be altered now
  - internal factors in kernel are no longer revectorable
  - removed use of low level conditionals outside of the kernel
  - reduced amount of padding in kernel
  - reduced default string length to 256 cells
  - metacompiler now strips unused space at end of kernel
  - reduced image size to under 9k cells
  - added until loop combinator
  - hidden functions are no longer revectorable


libraries
=========

  - fixed all reported bugs
  - added diet' library for reducing memory usage by trimming string size, buffers
  - added fiction' library for simple interactive fiction games
  - documentation blocks have consistent formatting now


extensions
==========

  - include lua bindings (now updated for the lua 5.2 release)
  - include sqlite bindings


ngaro vm
========

  - c

    - add --help flag
    - load image from $RETROIMAGE environment variable if not found in working directory
    - added variant for Windows users (can be built with tcc)

  - golang

    - updated to work with current weekly builds
    - added README to explain how to build it and cover concurrency additions

  - html5

    - use bootstrap for ui elements

  - php

    - fixed bugs in host environment queries, now feature complete

  - embedded

    - support pic32 boards running RetroBSD
    - updates to arduino implementation from Oleksandr

      - now supports MEGA 2560 and Nano boards

    - slightly lower memory usage in mbed implementation


examples
========

  - fixed bugs
  - updated to use new language features
  - added bingo card generator
  - added hex dump utility
  - added tab completion example (from Luke)
  - added example of building strings using a combinator


documentation
=============

  - add single file covering all of the libraries
  - expansions to quick reference
  - minor updates to fix small mistakes, clarify things


other
=====

  - properly support multi-line strings in vim highlighter
  - the debugger now has a source display view

