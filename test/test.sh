#!/bin/bash

for f in ref/*.xml; do
	echo $f
	../steamroller $f result/$(basename $f)
done
