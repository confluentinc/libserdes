PACKAGE_NAME?=	confluent-libserdes
VERSION?=	1.0.0

BUILD_NUMBER?= 1

MOCK_CONFIG?=default

RESULT_DIR?=pkgs-$(VERSION)-$(BUILD_NUMBER)-$(MOCK_CONFIG)

BUILD_DEPENDS?= avro-c avro-cpp librdkafka

all: rpm


SOURCES:
	mkdir -p SOURCES

archive: SOURCES
	cd ../ && \
	git archive --prefix=$(PACKAGE_NAME)-$(VERSION)/ \
	  -o rpm/SOURCES/$(PACKAGE_NAME)-$(VERSION).tar.gz HEAD


build_prepare: archive
	mkdir -p $(RESULT_DIR)
	rm -f $(RESULT_DIR)/$(PACKAGE_NAME)*.rpm

# Install local depencies in mock dir.
# Local dependencies are packages not available in the standard Yum repos,
# such as avro-c.
install-deps:
	@([ -d "$(REPO_DIR)" ] || (echo "REPO_DIR must be set to a path containing $(BUILD_DEPENDS) RPMs" ; exit 1))
	[ -f $(RESULT_DIR)/state.log ] || /usr/bin/mock -r $(MOCK_CONFIG) $(MOCK_OPTIONS) \
		--resultdir=$(RESULT_DIR) --init
	$(eval DEP_PKGS := $(shell (for d in $(BUILD_DEPENDS) ; do ls $(REPO_DIR)/$${d}*.rpm | grep -v -E '(debuginfo|debugsource|\.src\.rpm)' ; done | sort -u)))
	/usr/bin/mock \
		$(MOCK_OPTIONS) \
		-r $(MOCK_CONFIG) \
		--no-clean \
		--no-cleanup-after \
		--resultdir=$(RESULT_DIR) \
		--install $(DEP_PKGS)

srpm: build_prepare
	/usr/bin/mock \
		$(MOCK_OPTIONS) \
		-r $(MOCK_CONFIG) \
		--no-clean \
		--no-cleanup-after \
		--define "__version $(VERSION)" \
		--define "__release $(BUILD_NUMBER)" \
		--resultdir=$(RESULT_DIR) \
		--buildsrpm \
		--spec=$(PACKAGE_NAME).spec \
		--sources=SOURCES
	@echo "======= Source RPM now available in $(RESULT_DIR) ======="

rpm: srpm
	/usr/bin/mock \
		$(MOCK_OPTIONS) \
		-r $(MOCK_CONFIG) \
		--no-clean \
		--no-cleanup-after \
		--define "__version $(VERSION)"\
		--define "__release $(BUILD_NUMBER)"\
		--resultdir=$(RESULT_DIR) \
		--rebuild $(RESULT_DIR)/$(PACKAGE_NAME)*.src.rpm
	@echo "======= Binary RPMs now available in $(RESULT_DIR) ======="

clean:
	rm -rf SOURCES

distclean: clean
