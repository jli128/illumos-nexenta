#!/bin/bash

revs=$(cd $GATEROOT; hg qseries | awk -F- '/\-specific\-.*patch/ {print $4}' | awk -F\. '{print $1}')

for r in $revs; do
	cd $GATEROOT
	if hg log -p -r $r | grep "\-\-\- /dev/null" >/dev/null; then
		echo "$r: HAS NEW FILES"
	else
		echo "$r: OK"
	fi
done
