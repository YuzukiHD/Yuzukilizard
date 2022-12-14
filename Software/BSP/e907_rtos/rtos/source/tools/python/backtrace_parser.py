#!/usr/bin/env python
#coding:utf-8

#Copyright (C) 2015-2018 Alibaba Group Holding Limited
#The file is from Alios-Things, which is licensed under Apache License 2.0
#Modified:
# 2020-06-01   Zeng Zhijin    support riscv

import os
import sys
import subprocess
import argparse
import re
import logging
import operator

#--------------------------------------------------------------
# Global defines
#--------------------------------------------------------------
_TOOLCHAIN_NM_OPT_        = "-nlCS"
_TOOLCHAIN_SIZE_OPT_      = "-Axt"
_TOOLCHAIN_OBJDUMP_OPT_   = "-D"
_TOOLCHAIN_ADDR2LINE_OPT_ = "-pfiCe"
_TOOLCHAIN_READELF_OPT_   = "-h"

_TOOLCHAIN_PREFIX_          = ""
_TOOLCHAIN_GCC_             = ""
_TOOLCHAIN_NM_              = ""
_TOOLCHAIN_SIZE_            = ""
_TOOLCHAIN_OBJDUMP_         = ""
_TOOLCHAIN_ADDR2LINE_       = ""
_TOOLCHAIN_READELF_         = ""

_ARM_TOOLCHAIN_PREFIX_          = "arm-melis-eabi-"
_ARM_TOOLCHAIN_GCC_             = "arm-melis-eabi-gcc"
_ARM_TOOLCHAIN_NM_              = "arm-melis-eabi-nm"
_ARM_TOOLCHAIN_SIZE_            = "arm-melis-eabi-size"
_ARM_TOOLCHAIN_OBJDUMP_         = "arm-melis-eabi-objdump"
_ARM_TOOLCHAIN_ADDR2LINE_       = "arm-melis-eabi-addr2line"
_ARM_TOOLCHAIN_READELF_         = "arm-melis-eabi-readelf"

_RISCV_TOOLCHAIN_PREFIX_        = "riscv64-unknown-elf-"
_RISCV_TOOLCHAIN_GCC_           = "riscv64-unknown-elf-gcc"
_RISCV_TOOLCHAIN_NM_            = "riscv64-unknown-elf-nm"
_RISCV_TOOLCHAIN_SIZE_          = "riscv64-unknown-elf-size"
_RISCV_TOOLCHAIN_OBJDUMP_       = "riscv64-unknown-elf-objdump"
_RISCV_TOOLCHAIN_ADDR2LINE_     = "riscv64-unknown-elf-addr2line"
_RISCV_TOOLCHAIN_READELF_       = "riscv64-unknown-elf-readelf"

_HOST_READELF_            = "readelf"
_HOST_READELF_OPT_        = "-h"

_CRASH_LOG_ = "crash_log"

MATCH_ADDR = re.compile(r'0x[0-9a-f]{1,8}', re.IGNORECASE)

_ARCH_TYPE_ARM_         = "ARM"
_ARCH_TYPE_RISCV_       = "RISC-V"
_ARCH_TYPE_RISCV_ID_    = "0xf3"

g_arch = [_ARCH_TYPE_ARM_, _ARCH_TYPE_RISCV_, _ARCH_TYPE_RISCV_ID_]

def get_arch_from_elf(elf_file):
    if not elf_file:
        return ""

    arch_info = subprocess.check_output(
        [_HOST_READELF_, _HOST_READELF_OPT_, elf_file])

    for line in arch_info.splitlines():
        if 'Machine' in line:
            temp = line.split()
            for arch in g_arch:
                if arch in temp:
                    return arch
    for line in arch_info.splitlines():
        if "系统架构" in line:
            temp = line.split()
            for arch in g_arch:
                if arch in temp:
                    return arch
    return ""

class Core_Dump(object):
    def __init__(self, crash_log, elf_file, toolchain_path):
        super(Core_Dump, self).__init__()
        self.crash_log       = crash_log
        self.elf_file        = elf_file.name
        self.toolchain_path  = toolchain_path

        self.parse_addr_list = []
        self.arch       = _ARCH_TYPE_ARM_

        self.check_env()
        self.global_list = self.get_filelist('./', [])

    def get_filelist(self, dir, Filelist):
        newDir = dir
        if os.path.isfile(dir):
            if os.path.splitext(dir)[1] == '.elf':
                Filelist.append(dir)
        elif os.path.isdir(dir):
            for s in os.listdir(dir):
                newDir=os.path.join(dir,s)
                self.get_filelist(newDir, Filelist)
        return Filelist

    def getTargetFileName(self, list, pc_addr, try_list):
        for e in list:
            arch_info = subprocess.check_output(
                [_TOOLCHAIN_READELF_, _TOOLCHAIN_READELF_OPT_ , e])

            for line in arch_info.splitlines():
                if 'Entry' in line:
                    entry_line=line.split(":", 1)[1]
                    entry=entry_line.strip()
                    if cmp(entry.lower()[2:5], pc_addr.lower()[2:5]):
                        if cmp(entry.lower()[2:4], "40") or cmp(pc_addr.lower()[2:4], "40"):
                            continue
                        else:
                            try_list.append(e)
                    else:
                        try_list.append(e)
            for line in arch_info.splitlines():
                if "入口" in line:
                    entry_line=line.split("：", 1)[1]
                    entry=entry_line.strip()
                    if cmp(entry.lower()[2:5], pc_addr.lower()[2:5]):
                        if cmp(entry.lower()[2:4], "40") or cmp(pc_addr.lower()[2:4], "40"):
                            continue
                        else:
                            try_list.append(e)
                    else:
                        try_list.append(e)
        list_len=len(list)
        trylist_len=len(try_list)
        if  list_len == 1 and trylist_len == 0:
            try_list.append(e)
        return try_list

    def find_pc_addr(self, pc_addr):
        list = self.global_list
        elf = self.getTargetFileName(list, pc_addr, [])
        i=0
        list_len=len(elf)
        err_num=0
        for e in elf:
            try:
                pc_trans = subprocess.check_output([_TOOLCHAIN_ADDR2LINE_, _TOOLCHAIN_ADDR2LINE_OPT_, e, pc_addr])
            except Exception as err:
                logging.exception(err)
            else:
                if not "?? ??:0" in pc_trans:
                    if i==0:
                        print pc_trans.strip()
                    elif i==list_len:
                        print("    or "+pc_trans.strip())
                    else:
                        print("    or "+pc_trans.strip())
                else:
                    err_num+=1
                i+=1
        if (err_num == list_len) and (err_num != 0):
            print "addr invalid"
        print ""

    def get_value_form_line(self, line, index):
        val_list = re.findall(MATCH_ADDR, line)
        if val_list:
            if index > len(val_list):
                return ""
            return val_list[index]
        else:
            return ""

    def parse_addr(self, line, index):
        addr = self.get_value_form_line(line, index)
        if addr:
            self.parse_addr_list.append(addr)
            self.find_pc_addr(addr)

    def check_env(self):
        global _TOOLCHAIN_GCC_
        global _TOOLCHAIN_ADDR2LINE_

        if sys.version_info.major > 2:
            error = """
            This parser tools do not support Python Version 3 and above.
            Python {py_version} detected.
            """.format(py_version=sys.version_info)

            print error
            sys.exit(1)

    def show(self):

        log_lines = iter(self.crash_log.read().splitlines())

        for line in log_lines:
            if "backtrace" in line:
                self.parse_addr(line, 0)
            else:
                pass

def main():

    global _TOOLCHAIN_PREFIX_
    global _TOOLCHAIN_GCC_
    global _TOOLCHAIN_NM_
    global _TOOLCHAIN_SIZE_
    global _TOOLCHAIN_OBJDUMP_
    global _TOOLCHAIN_ADDR2LINE_
    global _TOOLCHAIN_READELF_

    parser = argparse.ArgumentParser(description='Melis crash log core dump')

    # specify arguments
    parser.add_argument(metavar='CRASH LOG', type=argparse.FileType('rb', 0),
                        dest='crash_log', help='path to backtrace log file')

    parser.add_argument(metavar='ELF FILE', type=argparse.FileType('rb', 0),
                        dest='elf_file', help='elf file of application')

    parser.add_argument('-p','--path', help="absolute path of toolchain", default='')

    args = parser.parse_args()

    arch = get_arch_from_elf(args.elf_file.name)
    if (arch == _ARCH_TYPE_RISCV_) or (arch == _ARCH_TYPE_RISCV_ID_):
        _TOOLCHAIN_PREFIX_          = _RISCV_TOOLCHAIN_PREFIX_
        _TOOLCHAIN_GCC_             = _RISCV_TOOLCHAIN_GCC_
        _TOOLCHAIN_NM_              = _RISCV_TOOLCHAIN_NM_
        _TOOLCHAIN_SIZE_            = _RISCV_TOOLCHAIN_SIZE_
        _TOOLCHAIN_OBJDUMP_         = _RISCV_TOOLCHAIN_OBJDUMP_
        _TOOLCHAIN_ADDR2LINE_       = _RISCV_TOOLCHAIN_ADDR2LINE_
        _TOOLCHAIN_READELF_         = _RISCV_TOOLCHAIN_READELF_
    else:
        _TOOLCHAIN_PREFIX_          = _ARM_TOOLCHAIN_PREFIX_
        _TOOLCHAIN_GCC_             = _ARM_TOOLCHAIN_GCC_
        _TOOLCHAIN_NM_              = _ARM_TOOLCHAIN_NM_
        _TOOLCHAIN_SIZE_            = _ARM_TOOLCHAIN_SIZE_
        _TOOLCHAIN_OBJDUMP_         = _ARM_TOOLCHAIN_OBJDUMP_
        _TOOLCHAIN_ADDR2LINE_       = _ARM_TOOLCHAIN_ADDR2LINE_
        _TOOLCHAIN_READELF_         = _ARM_TOOLCHAIN_READELF_

    core_dump_ins = Core_Dump(args.crash_log, args.elf_file, args.path)

    core_dump_ins.show()

    if args.crash_log:
        args.crash_log.close()

    if args.elf_file:
        args.elf_file.close()

if __name__ == "__main__":
    main()
