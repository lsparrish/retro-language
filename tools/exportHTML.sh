#! /bin/sh

for f in $1/*.rst
do
  rst2html.py --stylesheet doc/default.css $f >${f%.*}.html
done
