#!/bin/bash

make -C src

if [[ "$1" == *"r"* ]] || [[ "$2" == *"r"* ]]; then
    HISTFILE="/dev/null" ./src/myshell
fi

if [[ "$1" == *"c"* ]] || [[ "$2" == *"c"* ]]; then
    git add .
    git commit --allow-empty -m stage2
    git push origin master
fi