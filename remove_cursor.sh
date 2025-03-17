#!/bin/bash

MASTER01=`xinput list | grep master01 | head -n1 | awk '{print $4}' | tr -d 'id='`
VCOREPTR=`xinput list | grep 'Virtual core pointer' | head -n1 | awk '{print $5}' | tr -d 'id='`
VCOREKBD=`xinput list | grep 'Virtual core keyboard' | head -n1 | awk '{print $5}' | tr -d 'id='`
if [ -z "$MASTER01" ]; then
	echo "Master01 does not exist"
	exit 0
fi
SLAVE_POINTER=`xinput list | grep -A1 master01 | head -n2 | tail -n1 | sed 's/.*id=//g' | awk '{print $1}'`

echo "Reattaching ${SLAVE_POINTER} to ${VCORE}"
xinput list
xinput remove-master "${MASTER01}" AttachToMaster "${VCOREPTR}" "${VCOREKBD}"

