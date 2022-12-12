# Patch files

## Kernel

- `0001-add-d310t9362v1-mipi-display-panel-driver.patch`
  - Add d310t9362v1 MIPI Panel Driver for Tina Linux
- `0002-fix-make-file-for-undefined-reference-to-d310t9362v1`
  - fix makefile for undefined reference to d310t9362v1
- `0003-add-SPi-NAND-Driver-for-FS35SQA001G.patch`
  - Add SPI NAND Driver for FS35SQA001G

### How to use

Put in `kernel/linux-4.9` and `git am *.patch` to apply patch.
