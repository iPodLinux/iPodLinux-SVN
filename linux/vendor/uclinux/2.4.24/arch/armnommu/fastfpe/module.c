/*
    Fast Floating Point Emulator
    (c) Peter Teichmann <teich-p@rcs.urz.tu-dresden.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/init.h>

#ifndef MODULE
#define kern_fp_enter	fp_enter

extern char fpe_type[];
#endif

static void (*orig_fp_enter)(void);	/* old kern_fp_enter value */
extern void (*kern_fp_enter)(void);	/* current FP handler */
extern void fastfpe_enter(void);	/* forward declarations */

#ifdef MODULE
/*
 * Return 0 if we can be unloaded.  This can only happen if
 * kern_fp_enter is still pointing at fastfpe_enter
 */
static int fpe_unload(void)
{
  return (kern_fp_enter == fastfpe_enter) ? 0 : 1;
}
#endif

static int __init fpe_init(void)
{
#ifdef MODULE
  if (!mod_member_present(&__this_module, can_unload))
    return -EINVAL;
  __this_module.can_unload = fpe_unload;
#else
  if (fpe_type[0] && strcmp(fpe_type, "fastfpe"))
    return 0;
#endif

  printk("Fast Floating Point Emulator V0.0 (c) Peter Teichmann.\n");

  /* Save pointer to the old FP handler and then patch ourselves in */
  orig_fp_enter = kern_fp_enter;
  kern_fp_enter = fastfpe_enter;

  return 0;
}

static void __exit fpe_exit(void)
{
  /* Restore the values we saved earlier. */
  kern_fp_enter = orig_fp_enter;
}

module_init(fpe_init);
module_exit(fpe_exit);

MODULE_AUTHOR("Peter Teichmann <teich-p@rcs.urz.tu-dresden.de>");
MODULE_DESCRIPTION("Fast floating point emulator");
