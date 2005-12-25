#ifndef _TTK_TTKMM_H_
#define _TTK_TTKMM_H_

#ifndef __cplusplus
#error ttkmm.h should only be included by C++ applications
#endif

namespace TTK
{
    class Application 
    {
    public:
        Application();
        ~Application();
        
        static void setResolution (int w, int h, int bpp);

        int run();

    private:
        static int _initialized;
    };

    typedef ttk_point Point;

    class Font
    {
    public:
        Font (const char *name, int size);
        Font (ttk_font fnt);
        ~Font();

        ttk_fontinfo *info();

        int width (const char *str);
        int width_lat1 (const char *str);
        int width_uc16 (const uc16 *str);
        int height();

        operator ttk_font() { return _fnt; }

    protected:
        ttk_font _fnt;
        int _needsfree;
    };

    class Color
    {
    public:
        Color (int r = 0, int g = 0, int b = 0);
        Color (const char *ap_prop);
        Color (ttk_color col);
        
        int getR();
        int getG();
        int getB();
        void unmake (int& r, int& g, int& b);

        operator ttk_color() { return _col; }

    protected:
        ttk_color _col;
    };

    class Surface
    {
    public:
        Surface (int w, int h);
        Surface (const char *imagefile);
        Surface (ttk_surface srf);
        ~Surface();
        
        Color makecol (int r, int g, int b);
        void unmakecol (ttk_color col, int& r, int& g, int& b);
        
        Color operator()(int x, int y) { return getPixel (x, y); }
        Color getPixel (int x, int y);

        void pixel (int x, int y, ttk_color col);

        void line (int x1, int y1, int x2, int y2, ttk_color col);
        void aaline (int x1, int y1, int x2, int y2, ttk_color col);

        void rect (int x1, int y1, int x2, int y2, ttk_color col);
        void rect (int x1, int y1, int x2, int y2, const char *ap_prop);
        void fillrect (int x1, int y1, int x2, int y2, ttk_color col);
        void fillrect (int x1, int y1, int x2, int y2, const char *ap_prop);

        void poly (int nv, short *vx, short *vy, ttk_color col);
        void aapoly (int nv, short *vx, short *vy, ttk_color col);
        void fillpoly (int nv, short *vx, short *vy, ttk_color col);
        
        void circle (int x, int y, int r, ttk_color col) { ellipse (x, y, r, r, col); }
        void aacircle (int x, int y, int r, ttk_color col) { aaellipse (x, y, r, r, col); }
        void fillcircle (int x, int y, int r, ttk_color col) { fillellipse (x, y, r, r, col); }

        void ellipse (int x, int y, int rx, int ry, ttk_color col);
        void aaellipse (int x, int y, int rx, int ry, ttk_color col);
        void fillellipse (int x, int y, int rx, int ry, ttk_color col);

        void bitmap (int x, int y, int w, int h, unsigned short *data, ttk_color col);
        
        void text (ttk_font fnt, int x, int y, ttk_color col, const char *str);
        void text_lat1 (ttk_font fnt, int x, int y, ttk_color col, const char *str);
        void text_uc16 (ttk_font fnt, int x, int y, ttk_color col, const uc16 *str);

        void blitOn (Surface& dest, int x, int y);
        void blit (int sx, int sy, int sw, int sh, Surface& dest, int dx, int dy);
        
        Surface scale (float factor);
        
        int width() { return _w; }
        int height() { return _h; }

        operator ttk_surface() { return _srf; }

    protected:
        ttk_surface _srf;
        int _w, _h;
        int _needsfree;
    };

    class Widget;

    class Window
    {
    public:
        Window();
        Window (TWindow *from);
        ~Window();
        
        void show();
        void hide();

        // Only needs to be called in rare cases.
        void draw();

        enum { Normal, Fullscreen, Popup };
        void resize (int mode);
        void move (int x, int y);
        void resize (int w, int h);
        void resize (int x, int y, int w, int h);

        int x() { return _win->x; }
        int y() { return _win->y; }
        int w() { return _win->w; }
        int h() { return _win->h; }
        
        void setTitle (const char *str);
        const char *title();

        void addWidget (TWidget *wid);
        int removeWidget (TWidget *wid);

        int startInput (TWidget *inmethod);
        void moveInput (int x, int y);
        void getInputSize (int& w, int& h);

        void dirty();

        operator TWindow*() { return _win; }
        
    protected:
        TWindow *_win;
        int _needsfree;
    };

    class Widget 
    {
    public:
        Widget (int x = 0, int y = 0, int w = -1, int h = -1, int focusable = 1);
        Widget (TWidget *from);
        virtual ~Widget() {}
        
        void setTimer (int ms);
        void setFramerate (int fps);
        void setFramerateInv (int fps_m1);
        
        void addToHeader();
        void removeFromHeader();
        
        void move (int x, int y);
        void resize (int w, int h);
        void resize (int x, int y, int w, int h);

        int x() { return _wid->x; }
        int y() { return _wid->y; }
        int w() { return _wid->w; }
        int h() { return _wid->h; }
        
        void dirty();
        
        void setHoldTime (int holdtime);
        void setKeyRepeat (int flag);
        void setRawkeysFlag (int flag);

        Window *window();

        /* Event handlers: */
        virtual void draw (Surface srf) = 0;
        virtual int scroll (int distance) { return TTK_EV_UNUSED; }
        virtual int stap (int location) { return TTK_EV_UNUSED; }
        virtual int button (int button, int time) { return TTK_EV_UNUSED; }
        virtual int down (int button) { return TTK_EV_UNUSED; }
        virtual int held (int button) { return TTK_EV_UNUSED; }
        virtual int input (int chr) { return TTK_EV_UNUSED; }
        virtual int frame() { return 0; }
        virtual int timer() { return 0; }

        operator TWidget*() { return _wid; }

    protected:
        void _setHandlers();

        TWidget *_wid;
    };

    //// Stuff that can't be classed
    // For input methods:
    void inputChar (int ch);
    void endInput();
    // Basic settings:
    void setTransitionFrames (int frames);
    void setClicker (void (*fn)());
    void setScrollMultiplier (int num, int denom);
    // Timers:
    typedef ttk_timer Timer;
    Timer createTimer (int ms, void (*fn)());
    void destroyTimer (Timer tim);
    // Events:
    void setEventHandler (int (*fn)(int,int,int));
    void setUnusedHandler (int (*fn)(int,int,int));
    int buttonPressed (int button); // returns 1 if pressed, 0 if not
    // Misc:
    int iPodVersion();
    void click();

    // And, moving a few things into this namespace...
    ttk_font& textFont = ttk_textfont;
    ttk_font& menuFont = ttk_menufont;
    ttk_screeninfo*& screen = ttk_screen;
}

#endif
