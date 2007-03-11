#!/bin/sh
grep "_(" ../src/*.[ch] | cut -d ':' -f 1 | sort -u | sed -e 's/..\///' > POTFILES.in
cat POTFILES.in
