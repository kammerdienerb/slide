#!/usr/bin/env bash

BASE=$(basename "$1" ".pdf")
PDF="$1"
PNG="${BASE}.png"

convert +profile "*" -fuzz 20% -channel RGB -negate -transparent "black" -quality 100 -density 1000 "$PDF" "$PNG"
