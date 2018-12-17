#  Copyright 2005 Adam Jackson.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

SUBDIRS = src man
MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL


if BUILDING_RPM
RPMDIR=`$(RPM) --eval %_rpmdir`
SRCRPMDIR=`$(RPM) --eval %_srcrpmdir`
RPMARCH=`$(RPM) --eval %_target_cpu`
RPM_RELEASE=`$(EGREP) ^Release: <gnome-inform7.spec | cut -f2 -d " "`
BINRPM = $(PACKAGE)-$(VERSION)-$(RPM_RELEASE).$(RPMARCH).rpm
DEBUGRPM = $(PACKAGE)-debuginfo-$(VERSION)-$(RPM_RELEASE).$(RPMARCH).rpm
SRCRPM = $(PACKAGE)-$(VERSION)-$(RPM_RELEASE).src.rpm
rpm: dist
	$(ECHO) "Building RPM package..."; \
	if $(RPMBUILD) -ta --clean $(PACKAGE)-$(VERSION).tar.xz; then \
		if $(TEST) -e $(RPMDIR)/$(BINRPM); then \
			$(CP) $(RPMDIR)/$(BINRPM) $(builddir); \
		elif $(TEST) -e $(RPMDIR)/$(RPMARCH)/$(BINRPM); then \
			$(CP) $(RPMDIR)/$(RPMARCH)/$(BINRPM) $(builddir); \
		fi; \
		$(TEST) -e $(SRCRPMDIR)/$(SRCRPM) && \
		$(CP) $(SRCRPMDIR)/$(SRCRPM) $(builddir); \
		if $(TEST) -e $(RPMDIR)/$(DEBUGRPM); then \
			$(CP) $(RPMDIR)/$(DEBUGRPM) $(builddir); \
		elif $(TEST) -e $(RPMDIR)/$(RPMARCH)/$(DEBUGRPM); then \
			$(CP) $(RPMDIR)/$(RPMARCH)/$(DEBUGRPM) $(builddir); \
		else \
			$(ECHO) "No debug package; install redhat-rpm-config if you want one."; \
		fi; \
	fi
else
rpm:
	@$(ECHO) \
	"To build an RPM package, you must run configure with --enable-rpm."; \
	exit 1
endif

if BUILDING_DEB
DEBSOURCEPKG=$(PACKAGE)_$(VERSION).orig.tar.xz
DEBSOURCEDIR=$(PACKAGE)-$(VERSION)
deb: dist
	@$(ECHO) "Building Debian package..."; \
	if $(TEST) ! -d debian; then \
		$(ECHO) "The files needed for building a Debian package are not" \
		"included by default in the distribution. To build a package, check" \
		"out the project from source control."; \
		exit 1; \
	fi; \
	$(MV) $(PACKAGE)-$(VERSION).tar.xz $(DEBSOURCEPKG) && \
	$(TAR) --extract --xz --file $(DEBSOURCEPKG) && \
	$(MKDIR_P) $(DEBSOURCEDIR)/debian && \
	$(CP) \
		debian/changelog \
		debian/compat \
		debian/control \
		debian/copyright \
		debian/docs \
		debian/rules \
		$(DEBSOURCEDIR)/debian && \
	cd $(DEBSOURCEDIR) && \
	DEB_BUILD_MAINT_OPTIONS=hardening=-format \
	$(DEBUILD) -rfakeroot -D -us -uc; \
	cd $(srcdir); \
	rm --force $(DEBSOURCEPKG); \
	rm --recursive --force $(DEBSOURCEDIR)
else
deb:
	@$(ECHO) \
	"To build a Debian package, you must run configure with --enable-deb."; \
	exit 1
endif
