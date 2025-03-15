#!/bin/bash

MASTER01=`xinput list | grep master01 | head -n1 | awk '{print $4}' | tr -d 'id='`
if [ -z "$MASTER01" ]; then
	xinput create-master master01
fi
MASTER01=`xinput list | grep master01 | head -n1 | awk '{print $4}' | tr -d 'id='`
if [ -z "$MASTER01" ]; then
	echo "Could not find master01"
fi

echo "Master01: ${MASTER01}"
xinput list

read reattach_id

echo "Reattaching ${reattach_id} to master01"
xinput reattach ${reattach_id} ${MASTER01}
