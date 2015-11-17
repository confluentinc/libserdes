PKGNAME=	libserdes
LIBNAME=	libserdes++
LIBVER=		1

CXXSRCS=	Serdes.cpp
HDRS=		serdescpp.h serdescpp-avro.h

OBJS=		$(CXXSRCS:%.cpp=%.o)

CPPFLAGS+=	-I../src



all: lib check


include ../mklove/Makefile.base

# No linker script/symbol hiding for C++ library
WITH_LDS=n

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

ifeq ($(WITH_LDS),y)
# Enable linker script if supported by platform
LIB_LDFLAGS+= $(LDFLAG_LINKERSCRIPT)$(LIBNAME).lds
endif

$(LIBNAME).lds: $(HDRS)
	@(printf "$(MKL_YELLOW)Generating linker script $@ from $(HDRS)$(MKL_CLR_RESET)\n" ; \
	  cat ../src/serdes.h | ../lds-gen.pl > $@)

-include $(DEPS)
