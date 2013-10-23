# Proxy all targets to the Makefile in the src dir
#
# * Debian
#  - Build dependencies are states in debian/control
#    as such, just use 'dpkg-buildpackage -b' to get them
#    (which would require at least 'dpkg-dev build-essential' package)
#    or use when the above packages are there:
#     apt-get install `dpkg-checkbuilddeps 2>&1 | grep "Unmet build dependencies" | cut -f3- -d: | sed 's/(.*)//g'`
#
# * Mac build requirements:
#  - Xcode Command Line Tools
#    (installable from inside Xcode -> Preferences -> Downloads)
#  - mkdir /usr/local/include (otherwise things fail)
#  - might need to set the PATH to include path of PostgreSQL binaries, eg:
#    PATH=${PATH}:/Library/PostgreSQL/9.1/bin/
#    Do of course check that you have 9.1 there and change otherwise
#
# * On BSD variants: 
#    Use GNU Make (gmake) for this Makefile
#
# * Windows cross-compilation:
#    Uses MXE (http://www.mxe.cc)
#    gets automatically downloaded (using git), compiled and used
#
# If on Debian one sees:
# 'Use of uninitialized value $includedir in concatenation (.) or string at (eval 9) line 1.'
# This is a problem in apxs2, edit /usr/share/apache2/build/config_vars.mk with s/-I$(includedir)//g
#

# The name of this project
PROJECT_NAME:=jumpbox
PROJECT_VERSION:=$(shell head -n 1 debian/changelog | cut -f2 -d"(" | cut -f1 -d")")
PROJECT_GIT_HASH:=$(shell git log --pretty=format:'%H' -n 1 2>/dev/null || echo "unknownGIThash")
PROJECT_GIT_TIME:='$(shell git log --pretty=format:'%ci' -n 1 2>/dev/null || echo "unknownGITtime")'

# Check if it is modified, and include a notice about that
ifneq ($(shell git status --porcelain 2>/dev/null | wc -l),0)
PROJECT_GIT_HASH:=$(PROJECT_GIT_HASH)-modified
endif

# Debug build
CFLAGS += -DDEBUG

# Do stack dumps?
CFLAGS += -DDEBUG_STACKDUMPS

# Rendezvous modules?
CFLAGS += -DDJB_RENDEZVOUS

#########################################################
export PROJECT_NAME
export PROJECT_VERSION
export PROJECT_GIT_HASH
export PROJECT_GIT_TIME
export CFLAGS

all:
	@$(MAKE) --no-print-directory -C server all

help:
	@$(MAKE) --no-print-directory -C server help

clean:
	@$(MAKE) --no-print-directory -C server clean
	@rm -rf debian/saferdefiance-jumpbox-chromium-plugin/ debian/saferdefiance-jumpbox-daemon/
	@rm -f debian/*.debhelper.log debian/*.substvars debian/*.debhelper

depend:
	@$(MAKE) --no-print-directory -C server depend

tags:
	@$(MAKE) --no-print-directory -C server tags

install:
	@$(MAKE) --no-print-directory -C server install

deb: fakeroot depend
	@echo "* Building Debian packages (unsigned)..."
	@dpkg-buildpackage -rfakeroot -b -us -uc

fakeroot:
ifeq ($(shell which fakeroot),)
	$(error "Fakeroot not found, please install: apt-get install fakeroot")
endif

# Mark targets as phony
.PHONY: all help clean depend tags deb fakeroot

