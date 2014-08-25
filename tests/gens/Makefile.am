

noinst_SCRIPTS = single-generator mpi_ompss-generator mpi_nanox-generator mpi_omp-generator

EXTRA_DIST = single-generator.in mpi-generator.in config.py

CLEANFILES = $(noinst_SCRIPTS)

ompss_cc  = "env OMPI_CC=mcc OMPI_CFLAGS=--ompss mpicc"
ompss_cxx = "env OMPI_CXX=mcxx OMPI_CXXFLAGS=--ompss mpicxx"
nanox_cc  = "env OMPI_CC=mcc mpicc"
nanox_cxx = "env OMPI_CXX=mcxx mpicxx"
omp_cc    = "env OMPI_CC=$(CC) OMPI_CFLAGS=$(OPENMP_CFLAGS) mpicc"
omp_cxx   = "env OMPI_CXX=$(CXX) OMPI_CXXFLAGS=$(OPENMP_CFLAGS) mpicxx"
versions = performance
if DEBUG_LIB
versions += debug
endif

single-generator: single-generator.in
	rm -f $@
	sed \
		-e's,@SRCDIR\@,"$(abs_top_srcdir)",g' \
		-e's,@BUILDDIR\@,"$(abs_top_builddir)",g' \
		-e's,@COMPILE_VERSIONS\@,"$(versions)",g' \
		-e's,@PERFO_CFLAGS\@,"$(PERFO_CFLAGS)",g' \
		-e's,@DEBUG_CFLAGS\@,"$(DEBUG_CFLAGS)",g' \
		-e's,@PERFO_CXXFLAGS\@,"$(PERFO_CFLAGS)",g' \
		-e's,@DEBUG_CXXFLAGS\@,"$(DEBUG_CFLAGS)",g' \
		-e's,@CONFIG_CC\@,"$(CC)",g' \
		-e's,@CONFIG_CXX\@,"$(CXX)",g' \
		$^ > $@.tmp
	mv $@.tmp $@
	chmod 755 $@

mpi_ompss-generator: mpi-generator.in
	rm -f $@
	sed \
		-e's,@SRCDIR\@,"$(abs_top_srcdir)",g' \
		-e's,@BUILDDIR\@,"$(abs_top_builddir)",g' \
		-e's,@COMPILE_VERSIONS\@,"$(versions)",g' \
		-e's,@PERFO_CFLAGS\@,"$(PERFO_CFLAGS)",g' \
		-e's,@DEBUG_CFLAGS\@,"$(DEBUG_CFLAGS)",g' \
		-e's,@PERFO_CXXFLAGS\@,"$(PERFO_CFLAGS)",g' \
		-e's,@DEBUG_CXXFLAGS\@,"$(DEBUG_CFLAGS)",g' \
		-e's,@CONFIG_CC\@,$(ompss_cc),g' \
		-e's,@CONFIG_CXX\@,$(ompss_cxx),g' \
		$^ > $@.tmp
	mv $@.tmp $@
	chmod 755 $@

mpi_nanox-generator: mpi-generator.in
	rm -f $@
	sed \
		-e's,@SRCDIR\@,"$(abs_top_srcdir)",g' \
		-e's,@BUILDDIR\@,"$(abs_top_builddir)",g' \
		-e's,@COMPILE_VERSIONS\@,"$(versions)",g' \
		-e's,@PERFO_CFLAGS\@,"$(PERFO_CFLAGS)",g' \
		-e's,@DEBUG_CFLAGS\@,"$(DEBUG_CFLAGS)",g' \
		-e's,@PERFO_CXXFLAGS\@,"$(PERFO_CFLAGS)",g' \
		-e's,@DEBUG_CXXFLAGS\@,"$(DEBUG_CFLAGS)",g' \
		-e's,@CONFIG_CC\@,$(nanox_cc),g' \
		-e's,@CONFIG_CXX\@,$(nanox_cxx),g' \
		$^ > $@.tmp
	mv $@.tmp $@
	chmod 755 $@

mpi_omp-generator: mpi-generator.in
	rm -f $@
	sed \
		-e's,@SRCDIR\@,"$(abs_top_srcdir)",g' \
		-e's,@BUILDDIR\@,"$(abs_top_builddir)",g' \
		-e's,@COMPILE_VERSIONS\@,"$(versions)",g' \
		-e's,@PERFO_CFLAGS\@,"$(PERFO_CFLAGS)",g' \
		-e's,@DEBUG_CFLAGS\@,"$(DEBUG_CFLAGS)",g' \
		-e's,@PERFO_CXXFLAGS\@,"$(PERFO_CFLAGS)",g' \
		-e's,@DEBUG_CXXFLAGS\@,"$(DEBUG_CFLAGS)",g' \
		-e's,@CONFIG_CC\@,$(omp_cc),g' \
		-e's,@CONFIG_CXX\@,$(omp_cxx),g' \
		$^ > $@.tmp
	mv $@.tmp $@
	chmod 755 $@

