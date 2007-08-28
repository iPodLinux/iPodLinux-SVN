/*
 * arch/armnommu/mach/io.c
 *
 * BRIEF MODULE DESCRIPTION
 *   io functions
 *
 * Copyright (C) 2001 RidgeRun, Inc. (http://www.ridgerun.com)
 * Author: RidgeRun, Inc.
 *         Greg Lonnon (glonnon@ridgerun.com) or info@ridgerun.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  OZH, 2002
 */

#include <linux/kernel.h>
#include <asm/io.h>
#if 0
void __inline__ insl(unsigned int port, void *to, int len)
{
	int i;
	for (i = 0; i < len; i++)
		((unsigned long *)to)[i] = inl(port);
}

void __inline__ insw(unsigned int port, void *to, int len)
{
	int i;

	for (i = 0; i < len; i++)
		((unsigned short *) to)[i] = inw(port);
}

void __inline__ insb(unsigned int port, void *to, int len)
{
	int i;
	for (i = 0; i < len; i++)
		((unsigned char *)to)[i] = inb(port);
}

void __inline__ outsw(unsigned int port, const void *from, int len)
{
	int i;

	for (i = 0; i < len; i++)
		outw(((unsigned short *) from)[i], port);
}

void __inline__ outsl(unsigned int port, const void *from, int len)
{
	int i;
	
	for (i = 0; i < len; i++) 
		outl(((unsigned long *) from)[i], port);
}

void __inline__ outsb(unsigned int port, const void *from, int len)
{
	int i;

	for (i = 0; i < len; i++)
		outb(((unsigned char *) from)[i], port);
}

void __inline__ outswb(unsigned int port, const void *from, int len)
{
	outsw(port, from, len >> 2);
}

void __inline__ inswb(unsigned int port, void *to, int len)
{
	int i;

	for (i = 0; i < len; i++)
		((unsigned short *) to)[i] = inb(port);
}
#endif
