PKGNAME=	libserdes
LIBNAME=	libserdes++
LIBVER=		2

CXXSRCS=	Serdes.cpp
HDRS=		serdescpp.h

OBJS=		$(CXXSRCS:%.cpp=%.o)

CPPFLAGS+=	-I../src



all: lib check


include ../mklove/Makefile.base

# No linker script/symbol hiding for C++ library
DISABLE_LDS=y

# OSX and Cygwin requires linking required libraries
ifeq ($(_UNAME_S),Darwin)
	FWD_LINKING_REQ=y
endif
ifeq ($(_UNAME_S),AIX)
	FWD_LINKING_REQ=y
endif
ifeq ($(shell uname -o 2>/dev/null),Cygwin)
	FWD_LINKING_REQ=y
endif

ifeq ($(FWD_LINKING_REQ),y)
	LIBS := -L../src -lserdes $(LIBS)
endif

LIBS += -lstdc++

CHECK_FILES+= $(LIBFILENAME) $(LIBNAME).a


file-check: lib
check: file-check

install: lib-install

clean: lib-clean

-include $(DEPS)
