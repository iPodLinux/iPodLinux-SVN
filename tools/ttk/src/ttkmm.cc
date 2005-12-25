#include <ttk.h>

namespace TTK
{
    int Application::_initialized = 0;

    Application::Application() 
    {
        if (!_initialized)
            ttk_init();
        _initialized++;
    }

    Application::~Application()
    {
        _initialized--;
        if (!_initialized)
            ttk_quit();
    }

    void Application::setResolution (int w, int h, int bpp)
    {
        if (_initialized) {
            fprintf (stderr, "Warning: setting resolution to %dx%dx%d after initialization, probably won't work\n", w, h, bpp);
        }
        ttk_set_emulation (w, h, bpp);
    }

    int Application::run() 
    {
        return ttk_run();
    }

    
    Font::Font (const char *name, int size) 
    {
        _needsfree = 1;
        _fnt = ttk_get_font (name, size);
    }
    Font::Font (ttk_font fnt) 
    {
        _needsfree = 0;
        _fnt = fnt;
    }
    Font::~Font() 
    {
        if (_needsfree)
            ttk_done_font (_fnt);
    }

    ttk_fontinfo *Font::info() 
    { return _fnt.fi; }

    int Font::width (const char *str) 
    {
        return ttk_text_width (_fnt, str);
    }
    int Font::width_lat1 (const char *str) 
    {
        return ttk_text_width_lat1 (_fnt, str);
    }
    int Font::width_uc16 (const uc16 *str) 
    {
        return ttk_text_width_uc16 (_fnt, str);
    }
    int Font::height() 
    {
        return ttk_text_height (_fnt);
    }
    
    
    Color::Color (int r, int g, int b)
        : _col (ttk_makecol (r, g, b))
    {}
    Color::Color (const char *ap_prop)
        : _col (ttk_ap_getx(ap_prop)->color)
    {}
    Color::Color (ttk_color col)
        : _col (col)
    {}

    int Color::getR() 
    {
        int r, g, b;
        unmake (r, g, b);
        return r;
    }

    int Color::getG()
    {
        int r, g, b;
        unmake (r, g, b);
        return g;
    }

    int Color::getB()
    {
        int r, g, b;
        unmake (r, g, b);
        return b;
    }

    void Color::unmake (int& r, int& g, int& b) 
    {
        ttk_unmakecol (_col, &r, &g, &b);
    }

    
    Surface::Surface (int w, int h)
        : _w (w), _h (h)
    {
        _srf = ttk_new_surface (w, h, ttk_screen->bpp);
        _needsfree = 1;
    }
    Surface::Surface (const char *imagefile) 
    {
        _srf = ttk_load_image (imagefile);
        _needsfree = 1;
        ttk_surface_get_dimen (_srf, &_w, &_h);
    }
    Surface::Surface (ttk_surface srf)
    {
        _srf = srf;
        _needsfree = 0;
        ttk_surface_get_dimen (_srf, &_w, &_h);
    }
    Surface::~Surface() 
    {
        if (_needsfree)
            ttk_free_surface (_srf);
    }

    Color Surface::makecol (int r, int g, int b) 
    {
        return Color (ttk_makecol_ex (r, g, b, _srf));
    }
    void Surface::unmakecol (ttk_color col, int& r, int& g, int& b) 
    {
        ttk_unmakecol_ex (col, &r, &g, &b, _srf);
    }

    Color Surface::getPixel (int x, int y) 
    {
        return Color (ttk_getpixel (_srf, x, y));
    }

    void Surface::pixel (int x, int y, ttk_color col) { ttk_pixel (_srf, x, y, col); }

    void Surface::line (int x1, int y1, int x2, int y2, ttk_color col)
    { ttk_line (_srf, x1, y1, x2, y2, col); }
    void Surface::aaline (int x1, int y1, int x2, int y2, ttk_color col)
    { ttk_aaline (_srf, x1, y1, x2, y2, col); }

    void Surface::rect (int x1, int y1, int x2, int y2, ttk_color col)
    { ttk_rect (_srf, x1, y1, x2, y2, col); }
    void Surface::rect (int x1, int y1, int x2, int y2, const char *ap_prop)
    { ttk_ap_rect (_srf, ttk_ap_get (ap_prop), x1, y1, x2, y2); }
    void Surface::fillrect (int x1, int y1, int x2, int y2, ttk_color col)
    { ttk_fillrect (_srf, x1, y1, x2, y2, col); }
    void Surface::fillrect (int x1, int y1, int x2, int y2, const char *ap_prop)
    { ttk_ap_fillrect (_srf, ttk_ap_get (ap_prop), x1, y1, x2, y2); }

    void Surface::poly (int nv, short *vx, short *vy, ttk_color col)
    { ttk_poly (_srf, nv, vx, vy, col); }
    void Surface::aapoly (int nv, short *vx, short *vy, ttk_color col)
    { ttk_aapoly (_srf, nv, vx, vy, col); }
    void Surface::fillpoly (int nv, short *vx, short *vy, ttk_color col)
    { ttk_fillpoly (_srf, nv, vx, vy, col); }

    void Surface::ellipse (int x, int y, int rx, int ry, ttk_color col)
    { ttk_ellipse (_srf, x, y, rx, ry, col); }
    void Surface::aaellipse (int x, int y, int rx, int ry, ttk_color col)
    { ttk_aaellipse (_srf, x, y, rx, ry, col); }
    void Surface::fillellipse (int x, int y, int rx, int ry, ttk_color col)
    { ttk_fillellipse (_srf, x, y, rx, ry, col); }

    void Surface::bitmap (int x, int y, int w, int h, unsigned short *data, ttk_color col)
    { ttk_bitmap (_srf, x, y, w, h, data, col); }

    void Surface::text (ttk_font fnt, int x, int y, ttk_color col, const char *str)
    { ttk_text (_srf, fnt, x, y, col, str); }
    void Surface::text_lat1 (ttk_font fnt, int x, int y, ttk_color col, const char *str)
    { ttk_text_lat1 (_srf, fnt, x, y, col, str); }
    void Surface::text_uc16 (ttk_font fnt, int x, int y, ttk_color col, const uc16 *str)
    { ttk_text_uc16 (_srf, fnt, x, y, col, str); }

    void Surface::blitOn (Surface& dest, int x, int y)
    { ttk_blit_image (_srf, dest._srf, x, y); }
    void Surface::blit (int sx, int sy, int sw, int sh, Surface& dest, int dx, int dy) 
    { ttk_blit_image_ex (_srf, sx, sy, sw, sh, dest._srf, dx, dy); }

    Surface Surface::scale (float factor) 
    {
        return ttk_scale_surface (_srf, factor);
    }


    Window::Window() 
    {
        _win = ttk_new_window();
        _win->data2 = this;
        _needsfree = 1;
    }
    Window::Window (TWindow *from) 
    {
        _win = from;
        _win->data2 = this;
        _needsfree = 0;
    }
    Window::~Window() 
    {
        if (_needsfree)
            ttk_free_window (_win);
    }

    void Window::show() 
    { ttk_show_window (_win); }
    void Window::hide()
    { ttk_hide_window (_win); }

    void Window::draw()
    { ttk_draw_window (_win); }
    
    void Window::resize (int mode) 
    {
        switch (mode) {
        case Normal:
            ttk_window_show_header (_win);
            resize (ttk_screen->wx, ttk_screen->wy, ttk_screen->w - ttk_screen->wx,
                    ttk_screen->h - ttk_screen->wy);
            break;
        case Fullscreen:
            ttk_window_hide_header (_win);
            resize (0, 0, ttk_screen->w, ttk_screen->h);
            break;
        case Popup:
            ttk_set_popup (_win);
            break;
        }
    }

    void Window::move (int x, int y) 
    {
        _win->x = x;
        _win->y = y;
    }

    void Window::resize (int w, int h) 
    {
        _win->w = w;
        _win->h = h;
    }

    void Window::resize (int x, int y, int w, int h) 
    {
        _win->x = x;
        _win->y = y;
        _win->w = w;
        _win->h = h;
    }
    
    void Window::setTitle (const char *str) 
    {
        ttk_window_title (_win, str);
    }
    const char *Window::title() 
    {
        return _win->title;
    }

    void Window::addWidget (TWidget *wid) 
    { ttk_add_widget (_win, wid); }
    int Window::removeWidget (TWidget *wid) 
    { return ttk_remove_widget (_win, wid); }

    int Window::startInput (TWidget *inmethod) 
    {
        return ttk_input_start_for (_win, inmethod);
    }
    void Window::moveInput (int x, int y) 
    {
        ttk_input_move_for (_win, x, y);
    }
    void Window::getInputSize (int& w, int& h) 
    {
        ttk_input_size_for (_win, &w, &h);
    }
    
    void Window::dirty()
    {
        _win->dirty++;
    }
    
    
    Widget::Widget (int x, int y, int w, int h, int focusable) 
    {
        _wid = ttk_new_widget (x, y);
        _wid->w = (w < 0)? ttk_screen->w - ttk_screen->wx : w;
        _wid->h = (h < 0)? ttk_screen->h - ttk_screen->wy : h;
        _wid->focusable = focusable;
        _wid->data2 = this;
        _setHandlers();
    }
    Widget::Widget (TWidget *from) 
    {
        _wid = from;
        _wid->data2 = this;
        _setHandlers();
    }

    void e_draw (TWidget *wid, ttk_surface srf)
    { Widget *Wid = (Widget *)wid->data2; Wid->draw (srf); }
    int e_scroll (TWidget *wid, int dist)
    { Widget *Wid = (Widget *)wid->data2; return Wid->scroll (dist); }
    int e_stap (TWidget *wid, int where)
    { Widget *Wid = (Widget *)wid->data2; return Wid->stap (where); }
    int e_button (TWidget *wid, int button, int time)
    { Widget *Wid = (Widget *)wid->data2; return Wid->button (button, time); }
    int e_down (TWidget *wid, int button)
    { Widget *Wid = (Widget *)wid->data2; return Wid->down (button); }
    int e_held (TWidget *wid, int button)
    { Widget *Wid = (Widget *)wid->data2; return Wid->held (button); }
    int e_input (TWidget *wid, int ch)
    { Widget *Wid = (Widget *)wid->data2; return Wid->input (ch); }
    int e_frame (TWidget *wid)
    { Widget *Wid = (Widget *)wid->data2; return Wid->frame(); }
    int e_timer (TWidget *wid)
    { Widget *Wid = (Widget *)wid->data2; return Wid->timer(); }

    void wid_destroy (TWidget *wid) 
    {
        Widget *Wid = (Widget *)wid->data2;
        Wid->~Widget();
    }

    void Widget::_setHandlers() 
    {
        _wid->draw = e_draw;
        _wid->scroll = e_scroll;
        _wid->stap = e_stap;
        _wid->button = e_button;
        _wid->down = e_down;
        _wid->held = e_held;
        _wid->input = e_input;
        _wid->frame = e_frame;
        _wid->destroy = wid_destroy;
    }

    void Widget::setTimer (int ms) 
    { ttk_widget_set_timer (_wid, ms); }
    void Widget::setFramerate (int fps)
    { ttk_widget_set_fps (_wid, fps); }
    void Widget::setFramerateInv (int fps_m1)
    { ttk_widget_set_inv_fps (_wid, fps_m1); }

    void Widget::addToHeader() 
    {
        ttk_add_header_widget (_wid);
    }
    void Widget::removeFromHeader()
    {
        ttk_remove_header_widget (_wid);
    }
    
    void Widget::move (int x, int y)
    {
        _wid->x = x;
        _wid->y = y;
    }
    void Widget::resize (int w, int h) 
    {
        _wid->w = w;
        _wid->h = h;
    }
    void Widget::resize (int x, int y, int w, int h) 
    {
        _wid->x = x;
        _wid->y = y;
        _wid->w = w;
        _wid->h = h;
    }

    void Widget::dirty() 
    { _wid->dirty++; }

    void Widget::setHoldTime (int holdtime)
    { _wid->holdtime = holdtime; }

    void Widget::setKeyRepeat (int flag)
    { _wid->keyrepeat = flag; }

    void Widget::setRawkeysFlag (int flag)
    { _wid->rawkeys = flag; }

    Window *Widget::window() 
    {
        TWindow *win = _wid->win;
        if (!win->data2)
            win->data2 = new Window (win);
        return (Window *)win->data2;
    }

    
    void inputChar (int ch) 
    { ttk_input_char (ch); }
    void endInput()
    { ttk_input_end(); }

    void setTransitionFrames (int frames)
    { ttk_set_transition_frames (frames); }
    void setClicker (void (*fn)())
    { ttk_set_clicker (fn); }
    void setScrollMultiplier (int num, int denom)
    { ttk_set_scroll_multiplier (num, denom); }

    Timer createTimer (int ms, void (*fn)()) 
    { return ttk_create_timer (ms, fn); }
    void destroyTimer (Timer tim) 
    { ttk_destroy_timer (tim); }

    void setEventHandler (int (*fn)(int,int,int)) 
    { ttk_set_global_event_handler (fn); }
    void setUnusedHandler (int (*fn)(int,int,int)) 
    { ttk_set_global_unused_handler (fn); }
    int buttonPressed (int button)
    { return ttk_button_pressed (button); }

    int iPodVersion() 
    { return ttk_get_podversion(); }

    void click() 
    { ttk_click(); }
}
