# E907 RTOS

## Toolchain
Download https://github.com/YuzukiHD/Yuzukilizard/releases/download/Compiler.0.0.1/riscv64-elf-x86_64-20201104.tar.gz and put to `rtos/tools/xcompiler/on_linux/compiler/`


## Compile
```
root@root# cd e907_rtos/rtos/source
root@root# source melis-env.sh        # apply environment
root@root# lunch                      # launch environment

Lunch menu... pick a combo:
The supported board:
    1. v851-e907-lizard
What is your choice? 1
You have select v851-e907-lizard
============================================
Project Based On Platform sun20iw3p1 v851-e907-lizard
============================================

root@root# make menuconfig             # Configuration function
root@root# make                        # Build 
```

