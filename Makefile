LIBNAME=libserdes

CHECK_FILES+=

VERSION = $(shell python rpm/get_version.py)
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

build_prepare: clean
	mkdir -p SOURCES
	git archive --format tar --output SOURCES/$(LIBNAME)-$(VERSION).tar HEAD:

srpm: clean build_prepare
	/usr/bin/mock \
		--define "__version $(VERSION)"\
		--define "__release $(BUILD_NUMBER)"\
		--resultdir=. \
		--buildsrpm \
		--spec=rpm/$(LIBNAME).spec \
		--sources=SOURCES

rpm: srpm
	/usr/bin/mock \
		--define "__version $(VERSION)"\
		--define "__release $(BUILD_NUMBER)"\
		--resultdir=. \
		--rebuild *.src.rpm
