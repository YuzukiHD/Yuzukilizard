#!/bin/sh
key=7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f
iv=$key
openssl enc -aes-128-cbc -K $key -iv $iv -in $1 -out $2
