===============
Local Variables
===============

--------
Overview
--------
This vocabulary provides an easy way to give functions access to local variables within
certain limitations.

* Variable names are limited to twelve characters
* Functions using local variables are not reentrant


-------
Example
-------
::

  : foo ( nos tos - result )  locals{ tos nos }  @nos @tos + @nos * ;

Note here that **locals{** will modify the temporary variable names to match the names
you specify.


---------
Functions
---------
+---------+-----------------+----------------------+-------------------------------------+
| Name    | Stack (Runtime) | Stack (Compile-Time) | Usage                               |
+=========+=================+======================+=====================================+
| locals{ | ``-``           | -a                   | Parse for and setup local variables.|
|         |                 |                      | The parsing ends when **}** is      |
|         |                 |                      | found. Local variables are created  |
|         |                 |                      | and initialized in reverse order of |
|         |                 |                      | stack comments.                     |
+---------+-----------------+----------------------+-------------------------------------+
| a       | -a              | a-a                  | First local variable                |
+---------+-----------------+----------------------+-------------------------------------+
| b       | -a              | a-a                  | Second local variable               |
+---------+-----------------+----------------------+-------------------------------------+
| c       | -a              | a-a                  | Third local variable                |
+---------+-----------------+----------------------+-------------------------------------+
| d       | -a              | a-a                  | Fourth local variable               |
+---------+-----------------+----------------------+-------------------------------------+
| e       | -a              | a-a                  | Fifth local variable                |
+---------+-----------------+----------------------+-------------------------------------+
| f       | -a              | a-a                  | Sixth local variable                |
+---------+-----------------+----------------------+-------------------------------------+

The initial variable names will be replaced by **locals{** each time it is used.
