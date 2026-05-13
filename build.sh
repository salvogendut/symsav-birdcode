#!/bin/bash
# Build barcode screensaver for SymbOS using scc

SCC="${SCC:-../scc/bin/cc}"

"$SCC" barcode.c \
    -N "Barcode" \
    -o barcode.sav \
    -h 512

python3 add_preview.py
