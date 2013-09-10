ifndef PROJECT_NAME
$(error Run this make from the root, aka one level up)
endif

# Which OS is this? Can be overriden by setting it first
# eg to compile Windows edition on another platform use: $ make OS_NAME=Windows
ifeq ($(OS_NAME),)
override OS_NAME=$(shell uname -s)
endif

ifeq ($(OS_NAME),)
$(error "No OS Kernelname? ('uname -s' failed)")
endif

# Check for supported platforms
ifeq ($(OS_NAME),Linux)
else
ifeq ($(OS_NAME),Windows)
else
ifeq ($(OS_NAME),Darwin)
else
$(error "Unsupported platform: $(OS_NAME), possible: Linux, Windows, Darwin")
endif
endif
endif

ifeq ($(OS_PROC),)
OS_PROC=$(shell uname -m)
else
CFLAGS+=-march=$(OS_PROC)
endif

ifeq ($(OS_BITS),)
	ifeq ($(OS_PROC),x86_64)
		OS_BITS=64
	else
		OS_BITS=32
	endif
endif

# Default to no extension
EXT:=

# Figure out the HOSTCC
ifeq ($(HOSTCC),)
HOSTCC := $(CC)
endif

ifeq ($(shell echo $(CFLAGS) | grep -c "DEBUG"),0)
	CFLAGS += -O3 -fno-trapping-math -ftracer -ffast-math -DNDEBUG
	CFLAG S+= -fstack-protector -Wstack-protector -fstack-protector-all
ifeq ($(OS_BITS),64)
	CFLAGS += -fprefetch-loop-arrays
endif
else
	CFLAGS += -g3 -O0
endif

# Standard Warnings
CFLAGS	+=	-Wall
CFLAGS	+=	-Werror

# Extended warnings
CFLAGS	+=	-Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings
CFLAGS	+=	-Waggregate-return -Wstrict-prototypes -Wmissing-prototypes
CFLAGS	+=	-Wmissing-declarations -Wredundant-decls -Wnested-externs
CFLAGS	+=	-Winline -Wbad-function-cast -fstrict-aliasing
CFLAGS	+=	-fno-common -Wno-packed -Wswitch-default
ifneq ($(OS_NAME),Windows)
CFLAGS	+=	-Wformat=2 -Wformat-security
endif
CFLAGS	+=	-Wmissing-format-attribute
CFLAGS	+=	-D_REENTRANT -D_THREAD_SAFE -pipe -Wunused -Winit-self
CFLAGS	+=	-Wextra -Wno-long-long -Wmissing-include-dirs
CFLAGS	+=	-Wno-variadic-macros
CFLAGS	+=	-ansi -std=c99
CFLAGS	+=	-pedantic

# Enable GNU features (needed for our use of pthreads etc)
CFLAGS  +=	-D_GNU_SOURCE

# Location of libfutil
LIBFUTIL:=../../libfutil
OBJFUTIL:=$(LIBFUTIL)/src/
CFLAGS	+= -I$(LIBFUTIL)/include/

ifeq ($(shell echo $(CFLAGS) | grep -c "DJB_RENDEZVOUS"),1)
# Rendezvous: Location of defiantclient + dependencies
LIBDEFIANTCLIENT:=../../rendezvous/client/src/
CFLAGS	+= -I$(LIBDEFIANTCLIENT)
LDLIBS += -lcrypto -lpbc -lgmp
endif

# Used for both Rendezvous and ACS
LDLIBS += -ljansson

# Linux
ifeq ($(OS_NAME),Linux)
CFLAGS	+= -D_LINUX
LDLIBS	+= -lpthread -lrt
# We need librt for clock_gettime()
endif

# Darwin
ifeq ($(OS_NAME),Darwin)
CFLAGS  += -D_DARWIN
endif

# Windows
ifeq ($(OS_NAME),Windows)
# Windows Cross Compilation using mingw
ifeq ($(OS_BITS),32)
# mingw32
MINGW_PFX=i686-pc-mingw32
else
$(error "Apologies, but mingw/MXE does not really support 64bit cross-compilation")
endif

MINGW_PATH=${HOME}/.cross/mxe/usr

MINGW_BASE=$(MINGW_PATH)/$(MINGW_PFX)

CC=$(MINGW_PATH)/bin/$(MINGW_PFX)-gcc
LD=$(MINGW_PATH)/bin/$(MINGW_PFX)-ld
AR=$(MINGW_PATH)/bin/$(MINGW_PFX)-ar
RC=$(MINGW_PATH)/bin/$(MINGW_PFX)-windres

CFLAGS  += -std=gnu99
CFLAGS	+= -DWIN32_LEAN_AND_MEAN -DVC_EXTRALEAN -DNOMINMAX -DSTRICT
CFLAGS  += -D_WIN32 -D_X86_ -D_WIN32_WINNT=0x666
#CFLAGS	+= -Wno-pedantic-ms-format
CFLAGS  += -I${MINGW_BASE}/include/
LDFLAGS += -L ${MINGW_BASE}/lib/
LDLIBS	+= -lws2_32 -ladvapi32 -lsecur32 -lcrypto -lssl
# Windows has .exe as an extension
EXT=.exe

# Avoid warning: ISO C does not support the 'I64' ms_printf length modifier
CFLAGS += -Wno-format
endif

# Try to get the Compiler version (assume gcc first)
CC_VERSION=$(shell $(CC) -v 2>&1 | grep "gcc version" | cut -f3 -d' ')
CC_TYPE=gcc

ifeq ($(CC_VERSION),)
ifeq ($(OS_NAME),Darwin)
# OSX 10.7 Lion (Xcode 3.1)
# Apple clang version 3.1 (tags/Apple/clang-318.0.58) (based on LLVM 3.1svn)
#
# OSX 10.8 Mountain Lion (Xcode 4.2)
# Apple LLVM version 4.2 (clang-425.0.28) (based on LLVM 3.2svn)
# 
# OSX 10.9 Mavericks (XCode 5)
# Apple LLVM version 5.0 (clang-500.1.69) (based on LLVM 3.3svn)
CC_VERSION=$(shell $(CC) -v 2>&1 | head -n1 | cut -f4 -d' ')
CC_TYPE=clang
# Don't report unused arguments
CFLAGS+=-Qunused-arguments 
endif
endif

ifeq ($(CC_VERSION),)
ifneq ($(OS_NAME),Windows)
$(error "We don't have a (working) compiler? ${CC}")
endif
endif

ifeq ($(OS_BITS),32)
CFLAGS += -D_32BIT
CFLAGS+=-m32
LDFLAGS+=-m32
else
ifeq ($(OS_BITS),64)
CFLAGS += -D_64BIT
CFLAGS+=-m64
LDLAGS+=-m64
else
$(error Unknown number of bits: $(OS_BITS))
endif
endif



# Force architecture for gcc on OS X Snow Leopard 
ifeq ($(OS_NAME)-$(CC_TYPE),Darwin-gcc)
#RENDEZVOUS: ian has to comment this out to compile on his laptop:
#ifeq ($(OS_BITS),32)
#CFLAGS+=-arch i386
#LDFLAGS+=-arch i386
#else
CFLAGS+=-arch x86_64
LDFLAGS+=-arch x86_64
#endif
endif

BUILD_GIT:=$(shell git log --pretty=format:'%H' -n 1)
CFLAGS += -DPROJECT_VERSION=$(PROJECT_VERSION)
CFLAGS += -DPROJECT_BUILDTIME=$(PROJECT_BUILDTIME)
CFLAGS += -DPROJECT_GIT=$(BUILD_GIT)

# Prettifiers
LINK	= @echo "* Linking $@"; $(CC) $(CFLAGS) $(LDFLAGS)

# Dependencies we care about when they change
DEPS		=	Makefile

# RFC6234 functions
RFC6234_OBJS	+=	$(OBJFUTIL)rfc6234/hmac.o	\
			$(OBJFUTIL)rfc6234/sha1.o	\
			$(OBJFUTIL)rfc6234/sha224-256.o	\
			$(OBJFUTIL)rfc6234/sha384-512.o	\
			$(OBJFUTIL)rfc6234/usha.o

# DJB
BINS		+=	djb$(EXT)
DJB_OBJS	+=	djb.o					\
			acs.o					\
			preferences.o				\
			$(OBJFUTIL)httpsrv.o			\
			$(OBJFUTIL)buf.o			\
			$(OBJFUTIL)conn.o			\
			$(OBJFUTIL)list.o			\
			$(OBJFUTIL)misc.o			\
			$(OBJFUTIL)rwl.o			\
			$(OBJFUTIL)thread.o

ifeq ($(shell echo $(CFLAGS) | grep -c "DEBUG_STACKDUMPS"),1)
DJB_OBJS	+=	$(OBJFUTIL)stack.o
endif

# Rendezvous support?
ifeq ($(shell echo $(CFLAGS) | grep -c "DJB_RENDEZVOUS"),1)
DJB_OBJS	+=	rendezvous.o				\
			$(LIBDEFIANTCLIENT)defiantclient.o	\
			$(LIBDEFIANTCLIENT)defiantbf.o		\
			$(LIBDEFIANTCLIENT)crc.o		\
			$(LIBDEFIANTCLIENT)utils.o		\
			$(LIBDEFIANTCLIENT)onion.o		\
			$(LIBDEFIANTCLIENT)outguess.o		\
			$(LIBDEFIANTCLIENT)defianterrors.o	\
			$(LIBDEFIANTCLIENT)defiantrequest.o
endif

# All the objects in this project nicely in alpha order
OBJS	:= $(shell echo $(DJB_OBJS) | tr ' ' '\n' | sort | uniq | tr '\n' ' ')

export CFLAGS
export LDFLAGS

ifeq ($(OS_NAME),Windows)
$(CC):
	@echo "* Fetching MXE (MingW Cross-Compilation Environment)..."
	@mkdir -p ${HOME}/.cross
	@cd ${HOME}/.cross && git clone https://github.com/mxe/mxe.git
	@echo "- Disabling SSL2 in OpenSSL"
	@sed -i 's/no-shared/no-shared no-ssl2/g' ${HOME}/.cross/mxe/src/openssl.mk
	@cd ${HOME}/.cross/mxe && make
	@echo "* MXE should be ready now, try 'make' again"
	@false

intro: $(CC)
else
intro:
endif
	@echo "Version   : $(PROJECT_VERSION)"
	@echo "Build Time: $(PROJECT_BUILDTIME)"
	@echo "Git Hash  : $(BUILD_GIT)"
	@echo "Target OS : $(OS_NAME) $(OS_BITS)-bits"
	@echo "Compiler  : $(CC) ($(CC_TYPE) $(CC_VERSION))"
#	@echo "CFLAGS    : $(CFLAGS)"

# The final targets we want to produce
all: intro ${BINS} runtests
	@echo "* All Done"

# Include all the dependencies
-include $(OBJS:.o=.d)

depend: clean
	@echo "* Making dependencies"
	@$(MAKE) -s $(OBJS)
	@echo "* Making dependencies - done"

tags: $(SRCS)
	@echo "* Generating tags"
	@(TOTAG=`cat *.d | sed -e 's/[:\\]/ /g' -e 's/ /\n/g' | sort | uniq | tr '\n' ' ')` && \
	ctags -x >tags $${TOTAG} && \
	ctags -e >TAGS $${TOTAG} )
	@echo "* Generating tags - done"

djb$(EXT): $(DEPS) $(DJB_OBJS)
	$(LINK) -o $@ $(DJB_OBJS) $(DJB_LDLIBS) $(LDLIBS)


%.o: %.c $(DEPS)
	@echo "* Compiling $@";
	@$(CC) -c $(CFLAGS) $*.c -o $*.o
	@$(CC) -MM $(CFLAGS) $*.c > $*.d
	@cp -f $*.d $*.d.tmp
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
	  sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp

install: all
	@echo "* Nothing to install"

clean:
	@echo "* Cleansing"
	@rm -rf $(BINS) *.o *.so *.lo *.la *.slo *.loT *.d .libs/ ../tests/*.o ../tests/*.d rfc6234/*.o rfc6234/*.d
	@echo "* Cleansing Dependencies (libfutil)"
	@make -C $(LIBFUTIL) clean
ifeq ($(shell echo $(CFLAGS) | grep -c "DJB_RENDEZVOUS"),1)
	@echo "* Cleansing Dependencies (libdefiantclient)"
	@make -C $(LIBDEFIANTCLIENT) clean
	@echo "* Cleansing complete"
endif

runtests:
ifeq ($(HOSTCC),$(CC))
	@echo "* No tests available"
else
	@echo "* Can't run tests as we did not compile natively"
endif

help:
	@echo "Available Targets:"
	@echo "all	- build it all + depend + tags"
	@echo "depend	- Force building of dependencies"
	@echo "tags	- Force generation of ctags and etags"
	@echo "install  - Install modules"
	@echo "runtests - Run various tests"

# Mark targets as phony
.PHONY : all install clean deb depend tags help

