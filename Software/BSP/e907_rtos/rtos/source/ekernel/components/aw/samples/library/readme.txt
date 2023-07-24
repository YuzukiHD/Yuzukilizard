step 1:
   in components/samples dir, mkdir library, 你可以在你认为何时的位置创建你的源码工作目录 。
step 2:
  在libary目录下，创建三个源码文件，然后书写Makefile,包含以下内容.
  1 lib-y += libfile1.o
  2 lib-y += libfile2.o
  3 lib-y += libfile3.o
  4  
  5 TARGET := $(srctree)/emodules/staging/libdemo.a
  6 include $(MELIS_BASE)/scripts/Makefile.rename
~                                                    
  1-3行，通过lib-y导入要编译的目标文件
  5行：重命名为libdemo.a,并且放置到 emodules/staging目录下
  6行:包括系统Makefile.

step3:
 编译即可.
   
