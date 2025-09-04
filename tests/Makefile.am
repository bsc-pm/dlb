#################################################################################
#  Copyright 2009-2021 Barcelona Supercomputing Center                          #
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
#  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                #
#################################################################################

LIT_OPTIONS = --xunit-xml-output=test.log.xml -sv $(LIT_FLAGS)
LIT = PYTHONPATH=$(top_srcdir)/tests $(PYTHON) $(top_srcdir)/scripts/lit/lit.py $(LIT_OPTIONS)

if HAVE_STATIC_LIBS
if HAVE_PYTHON27
check-local: lit-check
else
check-local: bets-check
endif
else
check-local:
	@echo
	@echo "DLB needs static libraries to run make check."
	@echo "Reconfigure with '---enable-static'."
	@echo
endif

lit-check:
	@echo "  Tip: export DLB_EXTRA_TESTS=1 to enable all tests"
	@echo "  Tip: Use LIT_FLAGS to limit the number of threads:"
	@echo "       make check LIT_FLAGS=\"-j N\""
	$(LIT) $(srcdir)/test/$(TEST)*

bets-check:
	@echo "  Tip: export DLB_EXTRA_TESTS=1 to enable all tests"
	$(top_srcdir)/scripts/bets $(BETS_OPTIONS) $(srcdir)/test/$(TEST)*
