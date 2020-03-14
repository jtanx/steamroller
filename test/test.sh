#!/bin/bash

for f in ref/*.xml; do
	echo $f
	../steamroller ' STEAMROLLED ' $f result/$(basename $f)
done
