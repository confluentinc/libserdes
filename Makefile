LIBNAME=libserdes

CHECK_FILES+=

PACKAGE_NAME?=	libserdes
VERSION?=	SNAPSHOT

# Jenkins CI integration
BUILD_NUMBER ?= 1

.PHONY:

all: mklove-check libs check TAGS

include mklove/Makefile.base

LIBSUBDIRS=	src src-cpp



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

TAGS: .PHONY
	@(if which etags >/dev/null 2>&1 ; then \
		echo "Using etags to generate $@" ; \
		git ls-tree -r --name-only HEAD | egrep '\.(c|cpp|h)$$' | \
			etags -f $@.tmp - ; \
		cmp $@ $@.tmp || mv $@.tmp $@ ; rm -f $@.tmp ; \
	 elif which ctags >/dev/null 2>&1 ; then \
		echo "Using ctags to generate $@" ; \
		git ls-tree -r --name-only HEAD | egrep '\.(c|cpp|h)$$' | \
			ctags -e -f $@.tmp -L- ; \
		cmp $@ $@.tmp || mv $@.tmp $@ ; rm -f $@.tmp ; \
	fi)
