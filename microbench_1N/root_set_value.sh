#!/bin/bash
#
# This script modifies a kernel parameter 
# This script must be called by root
# Args:
#   $1: file in /proc (full path)
#   $2: value

if [ $# -eq 2 ]; then
   echo "$2" > "$1"
fi
