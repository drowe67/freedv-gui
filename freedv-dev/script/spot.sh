#!/bin/bash
#
# spot.sh
# David Rowe Sep 2015
#

# Demo script for "spotting" based on FreeDV txt string. Posts a
# date-stamped text file to a web server.  Called from FreeDV GUI
# program when a callsign is received in the txt msg.


# Q: how to remove repeated spots, or those close in time?
#
# Set up automated lftp login:
#
#   $ lftp ftp://username@server
#   Password:
#   lftp username@server:~>  set bmk:save-passwords true
#   lftp username@server:~> bookmark add yourserver
#   lftp username@server:~> bookmark list
#   lftp username@server:~> quit

SPOTFILE=/home/david/tmp/freedvspot.html
FTPSERVER=ftp.rowetel.com

echo `date -u` "  " $1 "<br>" >> $SPOTFILE
tail -n 25 $SPOTFILE > /tmp/spot.tmp1
mv /tmp/spot.tmp1 $SPOTFILE
lftp -e "cd www;put $SPOTFILE;quit" $FTPSERVER
