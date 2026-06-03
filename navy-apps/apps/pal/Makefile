NAME = pal
SRCS = $(shell find src/ -name "*.c")
LIBS += libndl
CFLAGS += -Wno-implicit-function-declaration -Wno-int-conversion
# F_div_F uses int64_t division which generates __divdi3; link libgcc to provide it
LIB_FILES += $(shell gcc -m32 -print-libgcc-file-name)

include $(NAVY_HOME)/Makefile.app
