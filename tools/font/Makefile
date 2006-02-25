# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

ifdef IPOD
CC = $(CROSS)-gcc
LIBS += -Wl,-elf2flt
CROSS ?= arm-uclinux-elf
else
CC ?= cc
endif

do-it:
	@$(CC) font.c -o font -Wall $(LIBS)

clean:
	@rm -f font font.gdb

install:
ifdef IPOD
	@if [ -z "$(wildcard $(DESTDIR)/sbin)" ]; then echo "*** $(DESTDIR)/sbin does not exist. Please verify that you have correctly specified your iPod location. Stop."; false; fi
	@echo "Installing to $(DESTDIR)."
	@install -m 755 font $(DESTDIR)/sbin/font
else
	@install -m 755 font /bin/font
endif
