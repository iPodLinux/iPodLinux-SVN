#ifndef _SETTINGS_H_
#define _SETTINGS_H_

int get_int_setting(short id);
void set_int_setting(short id, int value);
char *get_string_setting(short id);
void set_string_setting(short id, char *value);
int save_settings(char *settings_file);
int load_settings(char *settings_file);
void free_all_settings(void);

#endif /* _SETTINGS_H_ */
