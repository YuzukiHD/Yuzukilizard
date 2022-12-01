#!/bin/sh

[ -e /dev/by-name/bootA_fifo ] && dd if=/dev/zero of=/dev/by-name/bootA_fifo
[ -e /dev/by-name/rootfsA_fifo ] && dd if=/dev/zero of=/dev/by-name/rootfsA_fifo
[ -e /dev/by-name/riscv32A_fifo ] && dd if=/dev/zero of=/dev/by-name/riscv32A_fifo

sleep 1

[ -e /dev/by-name/bootA_fifo ] && return 1
[ -e /dev/by-name/rootfsA_fifo ] && return 1
[ -e /dev/by-name/riscv32A_fifo ] && return 1

return 0
