/********************************************************************************************
*
* starfield.c
*
* Copyright (C) 2005 Kevin Ferrare, original starfield for Rockbox (http://www.rockbox.org)
* Copyright (C) 2006 Felix Bruns, ported to iPodlinux/podzilla2/ttk
*
* 2006 Scott Lawrence, Added color tinting, integrated into Vortex
*
* All files in this archive are subject to the GNU General Public License.
* See the file COPYING in the source tree root for full license agreement.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
********************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "pz.h"
#include "starfield.h"


#define MSG_DISP_TIME 30

#define MAX_STARS (320*240*20)/100

struct star
{
    int x,y,z;
    int velocity;
    ttk_color c;
};

struct starfield
{
    struct star tab[MAX_STARS];
    int nb_stars;
    int z_move;

    int x_range, x_offset;
    int y_range, y_offset;
};


static int t_disp = 0;
/*
static PzModule *module;
static PzConfig *config;
*/
static TWindow *window;
static TWidget *widget;
static struct starfield starfield;


static void star_placement_region( int xrange, int xoffset,
				   int yrange, int yoffset )
{
	starfield.x_range = xrange;
	starfield.x_offset = xoffset;
	starfield.y_range = yrange;
	starfield.y_offset = yoffset;
}


static void init_star(struct starfield * sf, struct star * star, int z_move)
{
    star->velocity = rand() % STAR_MAX_VELOCITY+1;

    star->x = ( rand() % sf->x_range ) + sf->x_offset;
    star->y = ( rand() % sf->y_range ) + sf->y_offset;

    if( ttk_screen->bpp >= 16 ) {
	    star->c = ttk_makecol( 192 + (rand() & 0x3f),
				   192 + (rand() & 0x3f),
				   192 + (rand() & 0x3f) );
    } else {
	    star->c = ttk_makecol( GREY );
    }
    
    if(z_move >= 0)
        star->z = Z_MAX_DIST;
    else
        star->z = rand() % Z_MAX_DIST/2+1;
}

static void move_star(struct starfield * sf, struct star * star, int z_move)
{
    star->z -= z_move * star->velocity;

    if (star->z <= 0 || star->z > Z_MAX_DIST)
        init_star(sf, star, z_move);
}

static void draw_star( struct starfield * sf,
			struct star * star, int z_move, ttk_surface surface)
{
    int x_draw, y_draw;

    x_draw = star->x / star->z + ttk_screen->w / 2;
    if (x_draw < 1 || x_draw >= ttk_screen->w)
    {
        init_star(sf, star, z_move);
        return;
    }

    y_draw = star->y / star->z + ttk_screen->h/2;

    if (y_draw < 1 || y_draw >= ttk_screen->h)
    {
        init_star(sf, star, z_move);
        return;
    }

    ttk_pixel (surface, x_draw, y_draw, star->c );
    if(star->z < 5*Z_MAX_DIST/6)
    {
        ttk_pixel (surface, x_draw, y_draw-1, star->c);
        ttk_pixel (surface, x_draw-1, y_draw, star->c);
        if(star->z < Z_MAX_DIST/2)
        {
            ttk_pixel (surface, x_draw+1, y_draw, star->c);
            ttk_pixel (surface, x_draw, y_draw+1, star->c);
        }
    }
}

static void starfield_add_stars(struct starfield * starfield, int nb_to_add)
{
    int i, old_nb_stars;
    int mv = (ttk_screen->w*ttk_screen->h*20)/100;

    old_nb_stars = starfield->nb_stars;
    starfield->nb_stars += nb_to_add;

    if(starfield->nb_stars > mv )
        starfield->nb_stars = mv;

    for(i=old_nb_stars; i < starfield->nb_stars; i++)
    {
        init_star(starfield, &(starfield->tab[i]), starfield->z_move);
    }
}

static void starfield_del_stars(struct starfield * starfield, int nb_to_del)
{
    starfield->nb_stars -= nb_to_del;
    if(starfield->nb_stars < 0)
        starfield->nb_stars = 0;
}

static void starfield_move_and_draw(struct starfield * sf, ttk_surface surface)
{
    int i;
    for(i=0;i<sf->nb_stars;++i)
    {
        move_star(sf, &(sf->tab[i]), sf->z_move);
        draw_star(sf, &(sf->tab[i]), sf->z_move, surface);
    }
}


static void draw_starfield (TWidget *this, ttk_surface surface)
{
    ttk_fillrect (surface, 0, 0, ttk_screen->w, ttk_screen->h, ttk_makecol(BLACK));

    starfield_move_and_draw(&starfield, surface);

    char str_buffer[40];
    if (t_disp > 0)
    {
        t_disp--;
        snprintf(str_buffer, sizeof(str_buffer), "Stars:%d \nSpeed:%d", starfield.nb_stars, starfield.z_move);
        ttk_text (surface, ttk_textfont, 3, ttk_screen->h - ttk_text_height(ttk_textfont) - 25, ttk_makecol(WHITE), str_buffer);
    }
}

static int down_starfield (TWidget *this, int button) 
{
    switch (button) {
    case TTK_BUTTON_HOLD:
        ttk_window_hide_header (window);
        return 0;
        break;
    }
    return TTK_EV_UNUSED;
}

static int button_starfield (TWidget *this, int button, int time) 
{
    switch (button) {
    case TTK_BUTTON_HOLD:
        ttk_window_show_header (window);
        return 0;
        break;

    case TTK_BUTTON_PREVIOUS:
        starfield_del_stars(&starfield, 100);
        t_disp=MSG_DISP_TIME;
        this->dirty++;
        return 0;
        break;

    case TTK_BUTTON_NEXT:
        starfield_add_stars(&starfield, 100);
        t_disp=MSG_DISP_TIME;
        this->dirty++;
        return 0;
        break;

    case TTK_BUTTON_MENU:
        pz_close_window (window);
        return 0;
        break;

    case TTK_BUTTON_ACTION:
        return 0;
        break;
    }
    return TTK_EV_UNUSED;
}

static int scroll_starfield (TWidget *this, int dir) 
{
#ifdef IPOD
    TTK_SCROLLMOD(dir, 3);
#else
    TTK_SCROLLMOD(dir, 1);
#endif
    if(dir > 0) {
        starfield.z_move++;
        t_disp = MSG_DISP_TIME;
    } else {
        starfield.z_move--;
        t_disp = MSG_DISP_TIME;
    }
    this->dirty++;
    return 0;
}

static int timer_starfield (TWidget *this) 
{
    /* try something out here XXXXXX */
/*
    static int pos = 0;
    static int p2 = 0;

    p2++;
    if( p2 > 2 ) p2=0,pos++;
    if( pos > 32 ) pos = 0;

    star_placement_region( (MAX_INIT_STAR_X/8)-1, 
			   (-MAX_INIT_STAR_X) + (pos*MAX_INIT_STAR_X/16) ,
			   MAX_INIT_STAR_Y*2, 
			   -MAX_INIT_STAR_Y);
*/
    this->dirty++;
    return 0;
}

static TWidget *new_starfield_widget() 
{
    widget = ttk_new_widget (0, 0);
    widget->w = ttk_screen->w - ttk_screen->wx;
    widget->h = ttk_screen->h - ttk_screen->wy;
    widget->focusable = 1;
    widget->dirty = 1;
    widget->draw    =    draw_starfield;
    widget->button  =  button_starfield;
    widget->down    =    down_starfield;
    widget->scroll  =  scroll_starfield;
    widget->timer   =   timer_starfield;

    pz_widget_set_timer(widget, 10);

    starfield.nb_stars = 0;
    starfield.z_move = 1;

    star_placement_region( MAX_INIT_STAR_X*2, -MAX_INIT_STAR_X,
			   MAX_INIT_STAR_Y*2, -MAX_INIT_STAR_Y);

    starfield_add_stars(&starfield, 600);

    return widget;
}

static TWindow *new_starfield_window() 
{
    window = pz_new_window ("Starfield", PZ_WINDOW_NORMAL);

    ttk_add_widget (window, new_starfield_widget());

    return pz_finish_window (window);
}

/*static int save_starfield_config(TWidget *this, int key, int time)
{
    if(key == TTK_BUTTON_MENU){
        pz_save_config(config); 
    }
    return ttk_menu_button(this, key, time);
}*/

static void init_starfield()
{
/* STANDALONE
    module = pz_register_module ("starfield", NULL);
*/

    //config = pz_load_config(pz_module_get_cfgpath(module, "starfield.conf"));

    pz_menu_add_action ("/Extras/Demos/Starfield", new_starfield_window);

    //pz_menu_add_setting("/Extras/Demos/Starfield/Settings/Start Stars", 1, config, stars_options);
    //pz_menu_add_setting("/Extras/Demos/Starfield/Settings/Start Speed", 2, config, speed_options);

    //((TWidget *)pz_get_menu_item("/Extras/Demos/Starfield/Settings")->data)->button = save_starfield_config;
}

/* STANDALONE
PZ_MOD_INIT(init_starfield)
*/


/** Additions For External Use ***********************************************/

void Module_Starfield_init( void )
{
	init_starfield();
}

void Module_Starfield_session( int nstars, int velocity )
{
	starfield.nb_stars = 0;
	starfield.z_move = velocity;

	star_placement_region( MAX_INIT_STAR_X*2, -MAX_INIT_STAR_X,
			       MAX_INIT_STAR_Y*2, -MAX_INIT_STAR_Y);

	starfield_add_stars(&starfield, 200);
}

void Module_Starfield_draw( ttk_surface srf )
{
	starfield_move_and_draw( &starfield, srf );
}

void Module_Starfield_genregion( int xr, int xo, int yr, int yo )
{
	star_placement_region( xr, xo, yr, yo );
}
