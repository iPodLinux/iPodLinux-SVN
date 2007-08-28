#ifndef _SLIDER_H_
#define _SLIDER_H_

void destroy_slider();
void slider_draw();
int slider_event(GR_EVENT * event);
void new_settings_slider(GR_WINDOW_ID wid, int setting,
		int slider_min, int slider_max);
void new_int_slider(GR_WINDOW_ID wid, int *setting,
		int slider_min, int slider_max);
void new_settings_slider_window(char *title, int setting,
		int slider_min, int slider_max);
void new_int_slider_window(char *title, int *setting,
		int slider_min, int slider_max);

#endif /* _SLIDER_H_ */
