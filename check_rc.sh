#!/bin/bash
/tmp/tsd
RC=$?
echo "RETURN_CODE=${RC}"
/tmp/r123
RC2=$?
echo "GCC_RC=${RC2}"
