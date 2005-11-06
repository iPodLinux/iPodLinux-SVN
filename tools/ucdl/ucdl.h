/* uCdl v1.0
 * A library for dynamic loading under uClinux.
 *
 * See copyright notices in ucdl.c.
 * In short: Copyright (c) 2005 Joshua Oreman. MIT license.
 */

int uCdl_init (const char *symfile);
void *uCdl_open (const char *path);
void *uCdl_sym (void *handle, const char *name);
void uCdl_close (void *handle);
const char *uCdl_error();
