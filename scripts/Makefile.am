
# Install scripts in $(bindir) and distribute them.
dist_bin_SCRIPTS = run_with_dlb.sh 

EXTRA_DIST = \
	bets \
	run_with_dlb.sh.in \
	dlb.spec \
	debian/compat \
	debian/source/format \
	debian/rules \
	debian/control \
	$(END)

run_with_dlb.sh: run_with_dlb.sh.in
	rm -f $@
	sed \
	    -e's,@PREFIX_LIB\@,"$(libdir)",g' \
	    -e's,@NCPUS\@,$(NCPUS),g' \
	$^ > $@.tmp
	mv $@.tmp $@
	chmod 755 $@

rpm:
	mkdir -p SOURCES BUILD RPMS SPECS SRPMS
	cp $(top_build_prefix)/$(PACKAGE_TARNAME)-$(VERSION).tar.gz SOURCES
	THREADS=$$(getconf _NPROCESSORS_ONLN); THREADS=$${THREADS:-1};\
	rpmbuild -v -bb --clean $(srcdir)/dlb.spec --define '_topdir $(CURDIR)' \
		--define 'version $(VERSION)'\
		--define 'release $(shell date +%Y%m%d)'\
		--define "threads $${THREADS}"\
		--define 'configure_options CFLAGS=-D_FORTIFY_SOURCE=0'

clean-rpm:
	rm -rf BUILDROOT SOURCES BUILD RPMS SPECS SRPMS

deb:
	cp $(top_build_prefix)/$(PACKAGE_TARNAME)-$(VERSION).tar.gz $(PACKAGE_TARNAME)_$(VERSION).orig.tar.gz
	tar -xf $(PACKAGE_TARNAME)_$(VERSION).orig.tar.gz
	rm -rf upstream_dir
# Set up directory structure
	mv $(PACKAGE_TARNAME)-$(VERSION) upstream_dir
	cp -rf $(srcdir)/debian upstream_dir/
	cp -rf upstream_dir/COPYING upstream_dir/debian/copyright
	rm -f upstream_dir/debian/changelog
# Generate an empty changelog if git is not present
	if test ! -d $(top_srcdir)/.git; \
	  then dch --create -c upstream_dir/debian/changelog --empty --package $(PACKAGE_TARNAME) -v "$(VERSION)-$(shell date +%Y%m%d)$(DEB_RELEASE)"; \
	else git --git-dir="$(top_srcdir)/.git" --work-tree="$(top_srcdir)/.git" log -1 --pretty=format:"$(PACKAGE_TARNAME) ($(VERSION)-$(shell date +%Y%m%d)$(DEB_RELEASE)) unstable; urgency=low%x0A%x0A  * %h %s%x0A%x0A -- %an <%ae>  %aD" > upstream_dir/debian/changelog; \
	fi
# Build
	THREADS=$$(getconf _NPROCESSORS_ONLN); THREADS=$${THREADS:-1};  cd upstream_dir; DEB_BUILD_OPTIONS="nocheck parallel=$${THREADS}" debuild -us -uc
	-rm -rf upstream_dir



clean-deb:
	rm -rf $(PACKAGE_TARNAME)_$(VERSION).orig.tar.gz $(PACKAGE_TARNAME)_$(VERSION)*.debian.tar.gz $(PACKAGE_TARNAME)_$(VERSION)*.dsc $(PACKAGE_TARNAME)_$(DEB_RELEASE)*.build $(PACKAGE_TARNAME)_$(DEB_RELEASE)*.changes *.deb

clean-local: clean-rpm clean-deb
