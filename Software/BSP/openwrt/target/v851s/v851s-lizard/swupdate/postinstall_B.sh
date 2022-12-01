#!/bin/sh

[ -e /dev/by-name/bootB_fifo ] && dd if=/dev/zero of=/dev/by-name/bootB_fifo
[ -e /dev/by-name/rootfsB_fifo ] && dd if=/dev/zero of=/dev/by-name/rootfsB_fifo
[ -e /dev/by-name/riscv32B_fifo ] && dd if=/dev/zero of=/dev/by-name/riscv32B_fifo

sleep 1

[ -e /dev/by-name/bootB_fifo ] && return 1
[ -e /dev/by-name/rootfsB_fifo ] && return 1
[ -e /dev/by-name/riscv32B_fifo ] && return 1

return 0
