#!/bin/bash

autoscan
aclocal
autoheader
automake -a -c
autoconf
