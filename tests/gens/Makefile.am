#################################################################################
#  Copyright 2015 Barcelona Supercomputing Center                               #
#                                                                               #
#  This file is part of the DLB library.                                        #
#                                                                               #
#  DLB is free software: you can redistribute it and/or modify                  #
#  it under the terms of the GNU Lesser General Public License as published by  #
#  the Free Software Foundation, either version 3 of the License, or            #
#  (at your option) any later version.                                          #
#                                                                               #
#  DLB is distributed in the hope that it will be useful,                       #
#  but WITHOUT ANY WARRANTY; without even the implied warranty of               #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
#  GNU Lesser General Public License for more details.                          #
#                                                                               #
#  You should have received a copy of the GNU Lesser General Public License     #
#  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 #
#################################################################################


noinst_SCRIPTS = basic-generator mpi-generator

EXTRA_DIST = basic-generator.in mpi-generator.in config.py

CLEANFILES = $(noinst_SCRIPTS)

if SUPPORTED_SILENT_RULES
GEN_verbose = $(GEN_verbose_@AM_V@)
GEN_verbose_ = $(GEN_verbose_@AM_DEFAULT_V@)
GEN_verbose_0 = @echo "  GEN     " $@;
endif

versions = performance
if DEBUG_LIB
versions += debug
endif

if IGNORE_MPI_TESTS
ignore_mpi_tests = yes
else
ignore_mpi_tests =
endif

if HAVE_HWLOC
hwloc_flags = $(HWLOC_LDFLAGS) $(HWLOC_LIBS) -Wl,-rpath,$(HWLOC_LIBSDIR)
else
hwloc_flags =
endif

basic-generator: basic-generator.in
	$(GEN_verbose)rm -f $@ ; \
	sed \
		-e's|@SRCDIR\@|"$(abs_top_srcdir)"|g' \
		-e's|@BUILDDIR\@|"$(abs_top_builddir)"|g' \
		-e's|@COMPILE_VERSIONS\@|"$(versions)"|g' \
		-e's|@PERFO_CFLAGS\@|"$(PERFO_CFLAGS)"|g' \
		-e's|@DEBUG_CFLAGS\@|"$(DEBUG_CFLAGS)"|g' \
		-e's|@PERFO_CXXFLAGS\@|"$(PERFO_CFLAGS)"|g' \
		-e's|@DEBUG_CXXFLAGS\@|"$(DEBUG_CFLAGS)"|g' \
		-e's|@HWLOC_LDFLAGS\@|"$(hwloc_flags)"|g' \
		-e's|@CONFIG_CC\@|"$(CC)"|g' \
		-e's|@CONFIG_CXX\@|"$(CXX)"|g' \
		$^ > $@.tmp ; \
	mv $@.tmp $@ ; \
	chmod 755 $@

mpi-generator: mpi-generator.in
	$(GEN_verbose)rm -f $@ ; \
	sed \
		-e's|@SRCDIR\@|"$(abs_top_srcdir)"|g' \
		-e's|@BUILDDIR\@|"$(abs_top_builddir)"|g' \
		-e's|@COMPILE_VERSIONS\@|"$(versions)"|g' \
		-e's|@PERFO_CFLAGS\@|"$(PERFO_CFLAGS)"|g' \
		-e's|@DEBUG_CFLAGS\@|"$(DEBUG_CFLAGS)"|g' \
		-e's|@PERFO_CXXFLAGS\@|"$(PERFO_CFLAGS)"|g' \
		-e's|@DEBUG_CXXFLAGS\@|"$(DEBUG_CFLAGS)"|g' \
		-e's|@HWLOC_LDFLAGS\@|"$(hwloc_flags)"|g' \
		-e's|@CONFIG_CC\@|"$(MPICC)"|g' \
		-e's|@CONFIG_CXX\@|"mpicxx"|g' \
		-e's|@MPIRUN\@|$(MPIRUN)|g' \
		-e's|@MPIRUN_BIND\@|$(MPIRUN_BIND)|g' \
		-e's|@TEST_IGNORE\@|"$(ignore_mpi_tests)"|g' \
		$^ > $@.tmp ; \
	mv $@.tmp $@ ; \
	chmod 755 $@

