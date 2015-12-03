LIBNAME=libserdes

CHECK_FILES+=

PACKAGE_NAME?=	libserdes
VERSION?=	SNAPSHOT

# Jenkins CI integration
BUILD_NUMBER ?= 1

.PHONY:

all: mklove-check libs check

include mklove/Makefile.base

LIBSUBDIRS_$(ENABLE_AVRO_CPP) += src-cpp
LIBSUBDIRS=	src $(LIBSUBDIRS_y)



libs:
	@(for d in $(LIBSUBDIRS); do $(MAKE) -C $$d || exit $?; done)

file-check: examples
check: file-check
	@(for d in $(LIBSUBDIRS); do $(MAKE) -C $$d $@ || exit $?; done)

install:
	@(for d in $(LIBSUBDIRS); do $(MAKE) -C $$d $@ || exit $?; done)

examples tests: .PHONY libs
	$(MAKE) -C $@

clean:
	@$(MAKE) -C tests $@
	@$(MAKE) -C examples $@
	@(for d in $(LIBSUBDIRS); do $(MAKE) -C $$d $@ ; done)

distclean: clean
	./configure --clean
	rm -f config.log config.log.old


archive:
	git archive --prefix=$(PACKAGE_NAME)-$(VERSION)/ \
		-o $(PACKAGE_NAME)-$(VERSION).tar.gz HEAD
	git archive --prefix=$(PACKAGE_NAME)-$(VERSION)/ \
		-o $(PACKAGE_NAME)-$(VERSION).zip HEAD
