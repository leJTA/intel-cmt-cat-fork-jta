#!/bin/bash

echo "pkill -15 master_daemon"
pkill -15 master_daemon
echo -n "" > /tmp/catpc.fifo
#pqos -r
