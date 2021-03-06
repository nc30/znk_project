#!/bin/sh

echo "===="
make -f Makefile_cygwin.mak -C libZnk
_status=$?; if test $_status -ne 0; then exit $_status; fi 
echo "===="
echo ""

echo "===="
make -f Makefile_cygwin.mak -C moai
_status=$?; if test $_status -ne 0; then exit $_status; fi 
echo "===="
echo ""

echo "===="
make -f Makefile_cygwin.mak -C http_decorator
_status=$?; if test $_status -ne 0; then exit $_status; fi 
echo "===="
echo ""

echo "===="
make -f Makefile_cygwin.mak -C plugin_futaba
_status=$?; if test $_status -ne 0; then exit $_status; fi 
echo "===="
echo ""

echo "===="
make -f Makefile_cygwin.mak -C plugin_2ch
_status=$?; if test $_status -ne 0; then exit $_status; fi 
echo "===="
echo ""

echo "All compilings are done successfully."
