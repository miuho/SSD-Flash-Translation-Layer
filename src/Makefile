# Copyright 2009, 2010 Brendan Tauras

# Makefile is part of FlashSim.

# FlashSim is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.

# FlashSim is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with FlashSim.  If not, see <http://www.gnu.org/licenses/>.

##############################################################################

# FlashSim makefile
# Brendan Tauras 2010-08-03
# 
# Use the "ssd" (default) make target for separate compilation to include
# with external programs, such as DiskSim or a custom drivers.
#
# Use the "test" make target to run the most basic test of your FTL scheme
# after adding your content to the FTL, wear-leveler, and garbage-collector
# classes.
#
# Use the "trace" make target to run a more involved test of your FTL scheme
# after adding your content to the FTL, wear-leveler, and garbage-collector
# classes.  It is suggested to test with the "test" make target first.

CC = /usr/bin/gcc
CFLAGS = -I. -Wall -Wextra -g -std=c++0x
CXX = /usr/bin/g++
CXXFLAGS = $(CFLAGS)
HDR = ssd.h
SRC = ssd_address.cpp ssd_block.cpp ssd_bus.cpp ssd_channel.cpp ssd_config.cpp ssd_controller.cpp ssd_die.cpp ssd_event.cpp ssd_ftl.cpp ssd_user.cpp ssd_gc.cpp ssd_package.cpp ssd_page.cpp ssd_plane.cpp ssd_quicksort.cpp ssd_ram.cpp ssd_ssd.cpp ssd_wl.cpp StackHeapCalc.cpp
OBJ = ssd_address.o ssd_block.o ssd_bus.o ssd_channel.o ssd_config.o ssd_controller.o ssd_die.o ssd_event.o ssd_ftl.o ssd_user.o ssd_gc.o ssd_package.o ssd_page.o ssd_plane.o ssd_quicksort.o ssd_ram.o ssd_ssd.o ssd_wl.o StackHeapCalc.o
LOG = log
PERMS = 660
EPERMS = 770

ssd: $(HDR) $(SRC)
	$(CXX) $(CXXFLAGS) -c $(SRC)
	-chmod $(PERMS) $(OBJ)
#script -c "$(CXX) $(CXXFLAGS) -c $(SRC)" $(LOG)
#-chmod $(PERMS) $(LOG) $(OBJ)

test_1_%:
	make -C tests/checkpoint_1 1_$*

test_2_%:
	make -C tests/checkpoint_2 2_$*

test_3_%:
	make -C tests/checkpoint_3 3_$*

checkpoint_%:
	@number=1 ; while [[ $$number -le `find tests/checkpoint_$*/* -maxdepth 0 -type d | wc -l` ]] ; do \
	set -e ; \
	make -C tests/checkpoint_$* $*_$$number ; \
	./test_$*_$$number tests/checkpoint_$*/test_$*_$$number/test_$*_$$number.conf test_$*_$$number.log ; \
	((number = number + 1)) ; \
	done
	@echo "Pass all tests!"

clean:
	make -C tests/checkpoint_1 clean
	make -C tests/checkpoint_2 clean
	make -C tests/checkpoint_3 clean
	-rm -f $(OBJ) $(LOG)

files:
	echo $(SRC) $(HDR)
