
EXTRA_DIST = basic-generator.in config.py

noinst_SCRIPTS = basic-generator

CLEANFILES = basic-generator

versions = performance
if DEBUG_LIB
versions += debug
endif

basic-generator: basic-generator.in
	rm -f $@
	tmp_build_dirs=$(abs_top_builddir)/src/.libs ; \
	tmp_srcdir=$(abs_top_srcdir) ; \
	tmp_builddir=$(abs_top_builddir) ; \
	tmp_compile_versions="$(versions)" ; \
	tmp_perfo_cflags='"$(PERFO_CFLAGS) -fopenmp"' ; \
	tmp_debug_cflags='"$(DEBUG_CFLAGS) -fopenmp"' ; \
	tmp_cc='"$(CC)"' ; \
	tmp_cxx='"$(CXX)"' ; \
	sed \
		-e's,@SRCDIR\@,'"$$tmp_srcdir"',g' \
		-e's,@BUILDDIR\@,'"$$tmp_builddir"',g' \
		-e's,@COMPILE_VERSIONS\@,'"$$tmp_compile_versions"',g' \
		-e's,@PERFO_CFLAGS\@,'"$$tmp_perfo_cflags"',g' \
		-e's,@DEBUG_CFLAGS\@,'"$$tmp_debug_cflags"',g' \
		-e's,@PERFO_CXXFLAGS\@,'"$$tmp_perfo_cflags"',g' \
		-e's,@DEBUG_CXXFLAGS\@,'"$$tmp_debug_cflags"',g' \
		-e's,@CONFIG_CC\@,'"$$tmp_cc"',g' \
		-e's,@CONFIG_CXX\@,'"$$tmp_cxx"',g' \
		$^ > $@.tmp
	mv $@.tmp $@
	chmod 755 $@
