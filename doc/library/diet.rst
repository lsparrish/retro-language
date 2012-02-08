=====
diet'
=====


--------
Overview
--------
This library reduces overall memory consumption by reducing the string length
and number of string buffers.

While easy enough to do manually, this automates the process and makes it
easily reversible.


-------
Loading
-------
The following should suffice:

::

  needs diet'


---------
Functions
---------

+-------------+-----------+------------------------------------------------+
| Name        | Stack     | Usage                                          |
+=============+===========+================================================+
| shrink      | ``-``     | Reduce string length. This is invoked during   |
|             |           | the initial load of the library                |
+-------------+-----------+------------------------------------------------+
| extreme     | ``-``     | Further reductions of string length and reduces|
|             |           | the buffers                                    |
+-------------+-----------+------------------------------------------------+
| bloat       | ``-``     | Restore buffers and string length to the       |
|             |           | default                                        |
+-------------+-----------+------------------------------------------------+
