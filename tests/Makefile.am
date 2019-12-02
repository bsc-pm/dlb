#################################################################################
#  Copyright 2009-2019 Barcelona Supercomputing Center                          #
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

LIT_OPTIONS = --xunit-xml-output=test.log.xml -sv
LIT = PYTHONPATH=$(top_srcdir)/tests $(top_srcdir)/scripts/lit/lit.py $(LIT_OPTIONS)

if LIT_SUPPORT
check-local: lit-check
else
check-local: bets-check
endif

lit-check:
	$(LIT) $(srcdir)/test/$(TEST)*

bets-check:
	$(top_srcdir)/scripts/bets $(BETS_OPTIONS) $(srcdir)/test/$(TEST)*
