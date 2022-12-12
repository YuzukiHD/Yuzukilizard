# Makefile for eyesee-mpp/system/public/newfs_msdos
CUR_PATH := .

#set source files here.
SRCCS := \
    newfs_msdos.c

#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
    $(CUR_PATH)/../include

LOCAL_SHARED_LIBS :=

LOCAL_STATIC_LIBS :=

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC :=
LOCAL_TARGET_STATIC :=
LOCAL_TARGET_BIN := newfs_msdos

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(inc_paths) -fPIC -Wall -DANDROID
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(inc_paths) -fPIC -Wall -DANDROID
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

LOCAL_LDFLAGS := $(LOCAL_LDFLAGS) \
    -Wl,-Bstatic \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,-Bdynamic \
    $(foreach y, $(LOCAL_SHARED_LIBS), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(y)))))

#generate object files
OBJS := $(SRCCS:%=%.o) #OBJS=$(patsubst %,%.o,$(SRCCS))

#generate exe file.
.PHONY: all
all: $(LOCAL_TARGET_BIN)
	@echo ===================================
	@echo build eyesee-mpp-system-newfs_msdos done
	@echo ===================================

$(LOCAL_TARGET_BIN): $(OBJS)
	$(CXX) $(LOCAL_LDFLAGS) -o $@ $+
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

#patten rules to generate local object files
%.cpp.o: %.cpp
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<

%.c.o: %.c
	$(CC) $(LOCAL_CFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<

# clean all
.PHONY: clean
clean:
	-rm -f $(OBJS) $(LOCAL_TARGET_BIN)
