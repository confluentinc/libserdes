-include ../Makefile.config

PKGNAME=	libserdes
LIBNAME=	libserdes
LIBVER=		1

SRCS_$(ENABLE_AVRO_C)+= serialize-avro.c deserialize-avro.c schema-avro.c
HDRS_$(ENABLE_AVRO_C)+= serdes-avro.h

SRCS=		serdes.c rest.c schema-cache.c framing.c tinycthread.c \
		$(SRCS_y)

HDRS=		serdes.h serdes-common.h $(HDRS_y)

OBJS=		$(SRCS:.c=.o)




all: lib check

include ../mklove/Makefile.base


CHECK_FILES+= $(LIBFILENAME) $(LIBNAME).a

file-check: lib
check: file-check
#	@(printf "%-30s " "Symbol visibility" ; \
#		(($(SYMDUMPER) $(LIBFILENAME) | grep -q rd_kafka_new) && \
#		($(SYMDUMPER) $(LIBFILENAME) | grep -vq rd_kafka_destroy) && \
#		printf "$(MKL_GREEN)OK$(MKL_CLR_RESET)\n") || \
#		printf "$(MKL_RED)FAILED$(MKL_CLR_RESET)\n")

install: lib-install

clean: lib-clean

ifeq ($(WITH_LDS),y)
# Enable linker script if supported by platform
LIB_LDFLAGS+= $(LDFLAG_LINKERSCRIPT)$(LIBNAME).lds
endif

$(LIBNAME).lds: $(HDRS)
	@(printf "$(MKL_YELLOW)Generating linker script $@ from $(HDRS)$(MKL_CLR_RESET)\n" ; \
	  cat $(HDRS) | ../lds-gen.pl > $@)

-include $(DEPS)
