TARGET=rmond
SOURCES=$(PWD)/src
SUBDIRS=$(SOURCES)
define subdirs_call
set -e
for i in $(SUBDIRS); do $(MAKE) -C $$i $(1); done
endef

all:
	$(call subdirs_call, $@)

install:
	$(call subdirs_call, $@)

clean:
	$(call subdirs_call, $@)

rpms:
	cd .. && tar -cvjf $(TARGET).tar.bz2 --exclude .svn $(TARGET) && rpmbuild -ta $(TARGET).tar.bz2
