# Makefile for eyesee-mpp/system/public/libcutils
CUR_PATH := .

#set source files here.
SRCCS := \
    hashmap.c \
    native_handle.c \
    socket_inaddr_any_server.c \
    socket_local_client.c \
    socket_local_server.c \
    socket_loopback_client.c \
    socket_loopback_server.c \
    socket_network_client.c \
    sockets.c \
    config_utils.c \
    cpu_info.c \
    load_file.c \
    list.c \
    open_memstream.c \
    strdup16to8.c \
    strdup8to16.c \
    record_stream.c \
    process_name.c \
    properties.c \
    threads.c \
    sched_policy.c \
    iosched_policy.c \
    str_parms.c \
    fs.c \
    memory.c
#SRCCS += atomic.c.arm arch-arm/memset32.S

#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
    $(CUR_PATH)/../include/ \
    $(CUR_PATH)/../include/cutils

LOCAL_SHARED_LIBS := \
    libpthread

LOCAL_STATIC_LIBS :=

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC := libcutils
LOCAL_TARGET_STATIC := libcutils
LOCAL_TARGET_BIN :=

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(inc_paths) -fPIC -Wall \
    -DHAVE_SYS_UIO_H \
    -DHAVE_IOCTL \
    -DANDROID_SMP=1
#    -ansi -pedantic
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(inc_paths) -fPIC -Wall \
    -DHAVE_SYS_UIO_H \
    -DHAVE_IOCTL \
    -DANDROID_SMP=1
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)


LOCAL_DYNAMIC_LDFLAGS := $(LOCAL_LDFLAGS) -shared \
    -Wl,-Bsymbolic \
    -Wl,-rpath,/usr/lib \
    -L $(CUR_PATH)/../liblog \
    -Wl,-Bstatic \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,-Bdynamic \
    $(foreach y, $(LOCAL_SHARED_LIBS), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(y)))))

#generate object files
OBJS := $(SRCCS:%=%.o) #OBJS=$(patsubst %,%.o,$(SRCCS))

#add dynamic lib name suffix and static lib name suffix.
target_dynamic := $(if $(LOCAL_TARGET_DYNAMIC),$(LOCAL_TARGET_DYNAMIC).so,)
target_static := $(if $(LOCAL_TARGET_STATIC),$(LOCAL_TARGET_STATIC).a,)

#generate exe file.
.PHONY: all
all: $(target_dynamic) $(target_static)
	@echo ===================================
	@echo build eyesee-mpp-system-libcutils done
	@echo ===================================

$(target_dynamic): $(OBJS)
	$(CXX) $+ $(LOCAL_DYNAMIC_LDFLAGS) -o $@
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

$(target_static): $(OBJS)
	$(AR) -rcs -o $@ $+
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
	-rm -f $(OBJS) $(target_dynamic) $(target_static)
