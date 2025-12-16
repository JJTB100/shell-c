#!/bin/bash

gcc -Wall -Wextra -Wpedantic src/main.c -o main

if [[ "$1" == *"r"* ]] || [[ "$2" == *"r"* ]]; then
    ./main
fi

if [[ "$1" == *"c"* ]] || [[ "$2" == *"c"* ]]; then
    git add .
    git commit --allow-empty -m stage2
    git push origin master
fi