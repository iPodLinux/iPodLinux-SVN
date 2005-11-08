#ifndef _TTK_H_
#error You must #include <ttk.h> *before* <ttk/mwin-emu.h>.
#endif

#ifndef _TTK_MWIN_EMU_H_
#define _TTK_MWIN_EMU_H_

#ifndef MWBACKEND

#define GrOpen t_GrOpen
#define GrClose t_GrClose
#define GrFlush() //nothing needed
#define GrSelectEvents(w,e) //nothing needed
#define GrPeekEvent t_GrPeekEvent
#define GrCheckNextEvent(e) t_GrGetNextEventTimeout(e,0)
#define GrGetNextEventTimeout t_GrGetNextEventTimeout
#define GrGetScreenInfo t_GrGetScreenInfo

#define GrNewWindow t_GrNewWindow
#define GrNewWindowEx t_GrNewWindowEx
#define GrDestroyWindow t_GrDestroyWindow
#define GrMapWindow t_GrMapWindow
#define GrUnmapWindow t_GrUnmapWindow
#define GrResizeWindow t_GrResizeWindow
#define GrMoveWindow t_GrMoveWindow
#undef GrClearWindow
#define GrClearWindow t_GrClearWindow
#define GrGetWindowInfo t_GrGetWindowInfo
#define GrGetFocus t_GrGetFocus
#define GrNewPixmap t_GrNewPixmap

#define GrNewGC t_GrNewGC
#define GrCopyGC t_GrCopyGC
#define GrDestroyGC t_GrDestroyGC
#undef GrSetWindowBackgroundColor
#define GrSetWindowBackgroundColor(w,c) // nimpl
#define GrSetGCForeground t_GrSetGCForeground
#define GrSetGCBackground t_GrSetGCBackground
#define GrSetGCUseBackground t_GrSetGCUseBackground
#define GrSetGCFont t_GrSetGCFont
#define GrSetGCMode t_GrSetGCMode
#define GrGetGCInfo t_GrGetGCInfo

#define GrPoint t_GrPoint
#define GrPoints t_GrPoints
#define GrLine t_GrLine
#define GrRect t_GrRect
#define GrFillRect t_GrFillRect
#define GrPoly t_GrPoly
#define GrFillPoly t_GrFillPoly
#define GrEllipse t_GrEllipse
#define GrFillEllipse t_GrFillEllipse
#define GrArc(x,y,a,b,c,d,e,f,g,h,m) // nimpl
#define GrCopyArea t_GrCopyArea
#define GrBitmap t_GrBitmap
#define GrLoadImageFromFile t_GrLoadImageFromFile
//#define GrDrawImageFromFile t_GrDrawImageFromFile
#define GrDrawImageToFit t_GrLoadImageToFit
#define GrFreeImage t_GrFreeImage
#define GrText t_GrText

#define GrCreateFont t_GrCreateFont
#define GrDestroyFont t_GrDestroyFont
#define GrGetGCTextSize t_GrGetGCTextSize

#define GrCreateTimer t_GrCreateTimer
#define GrDestroyTimer t_GrDestroyTimer

#undef GR_RGB
#define GR_RGB(r,g,b) (((r)<<16)|((g)<<8)|(b))
#define RfRGB(rgb) ((rgb & 0xff0000) >> 16)
#define GfRGB(rgb) ((rgb & 0x00ff00) >> 8)
#define BfRGB(rgb)  (rgb & 0x0000ff)

#define GR_BITMAP t_GR_BITMAP
#define GR_COLOR t_GR_COLOR
#define GR_COORD t_GR_COORD
#define GR_SIZE t_GR_SIZE
#define GR_DRAW_ID t_GR_DRAW_ID
#define GR_EVENT t_GR_EVENT
#define GR_EVENT_KEYSTROKE t_GR_EVENT_KEYSTROKE
#define GR_EVENT_TIMER t_GR_EVENT_TIMER
#define GR_FONT_ID t_GR_FONT_ID
#define GR_GC_ID t_GR_GC_ID
#define GR_IMAGE_ID t_GR_IMAGE_ID
#define GR_POINT t_GR_POINT
#define GR_TIMER_ID t_GR_TIMER_ID
#define GR_WINDOW_ID t_GR_WINDOW_ID
#define GR_PIXMAP_ID t_GR_PIXMAP_ID
#define GR_SCREEN_INFO t_GR_SCREEN_INFO
#define GR_WINDOW_INFO t_GR_WINDOW_INFO
#define GR_GC_INFO t_GR_GC_INFO

// Undefine old flags
/* The root window id. */
#undef GR_ROOT_WINDOW_ID

/* Drawing modes for GrSetGCMode */
#undef GR_MODE_COPY
#undef GR_MODE_SET
#undef GR_MODE_XOR
#undef GR_MODE_OR
#undef GR_MODE_AND
#undef GR_MODE_CLEAR
#undef GR_MODE_SETTO1
#undef GR_MODE_EQUIV
#undef GR_MODE_NOR
#undef GR_MODE_NAND
#undef GR_MODE_INVERT
#undef GR_MODE_COPYINVERTED
#undef GR_MODE_ORINVERTED
#undef GR_MODE_ANDINVERTED
#undef GR_MODE_ORREVERSE
#undef GR_MODE_ANDREVERSE
#undef GR_MODE_NOOP

#undef GR_MODE_DRAWMASK
#undef GR_MODE_EXCLUDECHILDREN

/* Line modes */
#undef GR_LINE_SOLID
#undef GR_LINE_ONOFF_DASH

#undef GR_FILL_SOLID
#undef GR_FILL_STIPPLE
#undef GR_FILL_OPAQUE_STIPPLE
#undef GR_FILL_TILE

/* Polygon regions*/
#undef GR_POLY_EVENODD
#undef GR_POLY_WINDING

/* builtin font std names*/
#undef GR_FONT_SYSTEM_VAR
#undef GR_FONT_SYSTEM_FIXED
#undef GR_FONT_GUI_VAR
#undef GR_FONT_OEM_FIXED

/* GrText/GrGetTextSize encoding flags*/
#undef GR_TFASCII
#undef GR_TFUTF8
#undef GR_TFUC16
#undef GR_TFUC32
#undef GR_TFXCHAR2B
#undef GR_TFPACKMASK
#undef GR_TFENCMASK

/* GrText alignment flags*/
#undef GR_TFTOP
#undef GR_TFBASELINE
#undef GR_TFBOTTOM
#undef GR_TFWHEREMASK

/* GrSetFontAttr flags*/
#undef GR_TFKERNING
#undef GR_TFANTIALIAS
#undef GR_TFUNDERLINE

/* GrArc, GrArcAngle types*/
#undef GR_ARC
#undef GR_ARCOUTLINE
#undef GR_PIE

/* GrSetWindowRegion types*/
#undef GR_WINDOW_BOUNDING_MASK
#undef GR_WINDOW_CLIP_MASK

/* Booleans */
#undef GR_FALSE
#undef GR_TRUE

/* Loadable Image support definition */
#undef GR_IMAGE_MAX_SIZE

/* Button flags */
#undef GR_BUTTON_R
#undef GR_BUTTON_M
#undef GR_BUTTON_L
#undef GR_BUTTON_ANY

/* GrSetBackgroundPixmap flags */
#undef GR_BACKGROUND_TILE
#undef GR_BACKGROUND_CENTER
#undef GR_BACKGROUND_TOPLEFT
#undef GR_BACKGROUND_STRETCH
#undef GR_BACKGROUND_TRANS

/* GrNewPixmapFromData flags*/
#undef GR_BMDATA_BYTEREVERSE
#undef GR_BMDATA_BYTESWAP

#if 0 /* don't define unimp'd flags*/
/* Window property flags */
#undef GR_WM_PROP_NORESIZE
#undef GR_WM_PROP_NOICONISE
#undef GR_WM_PROP_NOWINMENU
#undef GR_WM_PROP_NOROLLUP
#undef GR_WM_PROP_ONTOP
#undef GR_WM_PROP_STICKY
#undef GR_WM_PROP_DND
#endif

/* Window properties*/
#undef GR_WM_PROPS_NOBACKGROUND
#undef GR_WM_PROPS_NOFOCUS
#undef GR_WM_PROPS_NOMOVE
#undef GR_WM_PROPS_NORAISE
#undef GR_WM_PROPS_NODECORATE
#undef GR_WM_PROPS_NOAUTOMOVE
#undef GR_WM_PROPS_NOAUTORESIZE

/* default decoration style*/
#undef GR_WM_PROPS_APPWINDOW
#undef GR_WM_PROPS_APPMASK
#undef GR_WM_PROPS_BORDER
#undef GR_WM_PROPS_APPFRAME
#undef GR_WM_PROPS_CAPTION
#undef GR_WM_PROPS_CLOSEBOX
#undef GR_WM_PROPS_MAXIMIZE

/* Flags for indicating valid bits in GrSetWMProperties call*/
#undef GR_WM_FLAGS_PROPS
#undef GR_WM_FLAGS_TITLE
#undef GR_WM_FLAGS_BACKGROUND
#undef GR_WM_FLAGS_BORDERSIZE
#undef GR_WM_FLAGS_BORDERCOLOR

/* Error codes */
#undef GR_ERROR_BAD_WINDOW_ID
#undef GR_ERROR_BAD_GC_ID
#undef GR_ERROR_BAD_CURSOR_SIZE
#undef GR_ERROR_MALLOC_FAILED
#undef GR_ERROR_BAD_WINDOW_SIZE
#undef GR_ERROR_KEYBOARD_ERROR
#undef GR_ERROR_MOUSE_ERROR
#undef GR_ERROR_INPUT_ONLY_WINDOW
#undef GR_ERROR_ILLEGAL_ON_ROOT_WINDOW
#undef GR_ERROR_TOO_MUCH_CLIPPING
#undef GR_ERROR_SCREEN_ERROR
#undef GR_ERROR_UNMAPPED_FOCUS_WINDOW
#undef GR_ERROR_BAD_DRAWING_MODE
#undef GR_ERROR_BAD_LINE_ATTRIBUTE
#undef GR_ERROR_BAD_FILL_MODE
#undef GR_ERROR_BAD_REGION_ID

/* Event types.
 * Mouse motion is generated for every motion of the mouse, and is used to
 * track the entire history of the mouse (many events and lots of overhead).
 * Mouse position ignores the history of the motion, and only reports the
 * latest position of the mouse by only queuing the latest such event for
 * any single client (good for rubber-banding).
 */
#undef GR_EVENT_TYPE_ERROR
#undef GR_EVENT_TYPE_NONE
#undef GR_EVENT_TYPE_EXPOSURE
#undef GR_EVENT_TYPE_BUTTON_DOWN
#undef GR_EVENT_TYPE_BUTTON_UP
#undef GR_EVENT_TYPE_MOUSE_ENTER
#undef GR_EVENT_TYPE_MOUSE_EXIT
#undef GR_EVENT_TYPE_MOUSE_MOTION
#undef GR_EVENT_TYPE_MOUSE_POSITION
#undef GR_EVENT_TYPE_KEY_DOWN
#undef GR_EVENT_TYPE_KEY_UP
#undef GR_EVENT_TYPE_FOCUS_IN
#undef GR_EVENT_TYPE_FOCUS_OUT
#undef GR_EVENT_TYPE_FDINPUT
#undef GR_EVENT_TYPE_UPDATE
#undef GR_EVENT_TYPE_CHLD_UPDATE
#undef GR_EVENT_TYPE_CLOSE_REQ
#undef GR_EVENT_TYPE_TIMEOUT
#undef GR_EVENT_TYPE_SCREENSAVER
#undef GR_EVENT_TYPE_CLIENT_DATA_REQ
#undef GR_EVENT_TYPE_CLIENT_DATA
#undef GR_EVENT_TYPE_SELECTION_CHANGED
#undef GR_EVENT_TYPE_TIMER
#undef GR_EVENT_TYPE_PORTRAIT_CHANGED
#undef GR_EVENT_TYPE_HOTKEY_DOWN
#undef GR_EVENT_TYPE_HOTKEY_UP

/* Event masks */
#undef GR_EVENTMASK

#undef GR_EVENT_MASK_NONE
#undef GR_EVENT_MASK_ERROR
#undef GR_EVENT_MASK_EXPOSURE
#undef GR_EVENT_MASK_BUTTON_DOWN
#undef GR_EVENT_MASK_BUTTON_UP
#undef GR_EVENT_MASK_MOUSE_ENTER
#undef GR_EVENT_MASK_MOUSE_EXIT
#undef GR_EVENT_MASK_MOUSE_MOTION
#undef GR_EVENT_MASK_MOUSE_POSITION
#undef GR_EVENT_MASK_KEY_DOWN
#undef GR_EVENT_MASK_KEY_UP
#undef GR_EVENT_MASK_FOCUS_IN
#undef GR_EVENT_MASK_FOCUS_OUT
#undef GR_EVENT_MASK_FDINPUT
#undef GR_EVENT_MASK_UPDATE
#undef GR_EVENT_MASK_CHLD_UPDATE
#undef GR_EVENT_MASK_CLOSE_REQ
#undef GR_EVENT_MASK_TIMEOUT
#undef GR_EVENT_MASK_SCREENSAVER
#undef GR_EVENT_MASK_CLIENT_DATA_REQ
#undef GR_EVENT_MASK_CLIENT_DATA
#undef GR_EVENT_MASK_SELECTION_CHANGED
#undef GR_EVENT_MASK_TIMER
#undef GR_EVENT_MASK_PORTRAIT_CHANGED
/* Event mask does not affect GR_EVENT_TYPE_HOTKEY_DOWN and
 * GR_EVENT_TYPE_HOTKEY_UP, hence no masks for those events. */

#undef GR_EVENT_MASK_ALL

/* update event types */
#undef GR_UPDATE_MAP
#undef GR_UPDATE_UNMAP
#undef GR_UPDATE_MOVE
#undef GR_UPDATE_SIZE
#undef GR_UPDATE_UNMAPTEMP
#undef GR_UPDATE_ACTIVATE
#undef GR_UPDATE_DESTROY
#undef GR_UPDATE_REPARENT

// And all the flags... Set to 1 if we don't recognize them.
/* The root window id. */
#define GR_ROOT_WINDOW_ID       1

/* Drawing modes for GrSetGCMode */
#define GR_MODE_COPY            1             /* src*/
#define GR_MODE_SET             1             /* obsolete, use GR_MODE_COPY*/
#define GR_MODE_XOR             2              /* src ^ dst*/
#define GR_MODE_OR              1               /* src | dst*/
#define GR_MODE_AND             1              /* src & dst*/
#define GR_MODE_CLEAR           1            /* 0*/
#define GR_MODE_SETTO1          1           /* 11111111*/ /* will be GR_MODE_SET*/
#define GR_MODE_EQUIV           1            /* ~(src ^ dst)*/
#define GR_MODE_NOR             1              /* ~(src | dst)*/
#define GR_MODE_NAND            1             /* ~(src & dst)*/
#define GR_MODE_INVERT          1           /* ~dst*/
#define GR_MODE_COPYINVERTED    1     /* ~src*/
#define GR_MODE_ORINVERTED      1       /* ~src | dst*/
#define GR_MODE_ANDINVERTED     1      /* ~src & dst*/
#define GR_MODE_ORREVERSE       1        /* src | ~dst*/
#define GR_MODE_ANDREVERSE      1       /* src & ~dst*/
#define GR_MODE_NOOP            1             /* dst*/

#define GR_MODE_DRAWMASK        0x00FF
#define GR_MODE_EXCLUDECHILDREN 0x0100          /* exclude children on clip*/

#undef MWROP_SRCCOPY
#define MWROP_SRCCOPY GR_MODE_COPY

/* Line modes */
#define GR_LINE_SOLID           1
#define GR_LINE_ONOFF_DASH      1

#define GR_FILL_SOLID           1
#define GR_FILL_STIPPLE         1
#define GR_FILL_OPAQUE_STIPPLE  1
#define GR_FILL_TILE            1

/* Polygon regions*/
#define GR_POLY_EVENODD         1
#define GR_POLY_WINDING         1

/* builtin font std names*/
#define GR_FONT_SYSTEM_VAR      1
#define GR_FONT_SYSTEM_FIXED    1
#define GR_FONT_GUI_VAR          1         /* deprecated*/
#define GR_FONT_OEM_FIXED         1      /* deprecated*/

/* GrText/GrGetTextSize encoding flags*/
#define GR_TFASCII              01
#define GR_TFUTF8               02
#define GR_TFUC16               03
#define GR_TFUC32               04
#define GR_TFXCHAR2B            05
#define GR_TFPACKMASK           06
#define GR_TFENCMASK            07

/* GrText alignment flags*/
#define GR_TFTOP                010
#define GR_TFBASELINE           020
#define GR_TFBOTTOM             030
#define GR_TFWHEREMASK          070

/* GrSetFontAttr flags*/
#define GR_TFKERNING            
#define GR_TFANTIALIAS          
#define GR_TFUNDERLINE          

/* GrArc, GrArcAngle types*/
#define GR_ARC          1           /* arc only*/
#define GR_ARCOUTLINE   2    /* arc + outline*/
#define GR_PIE          3           /* pie (filled)*/

/* GrSetWindowRegion types*/
#define GR_WINDOW_BOUNDING_MASK 0       /* outer border*/
#define GR_WINDOW_CLIP_MASK     1       /* inner border*/

/* Booleans */
#define GR_FALSE                0
#define GR_TRUE                 1

/* Loadable Image support definition */
#define GR_IMAGE_MAX_SIZE       (-1)

/* Button flags */
#define GR_BUTTON_R                   /* right button*/
#define GR_BUTTON_M                   /* middle button*/
#define GR_BUTTON_L                   /* left button */
#define GR_BUTTON_ANY                 /* any*/

/* GrSetBackgroundPixmap flags */
#define GR_BACKGROUND_TILE      0       /* Tile across the window */
#define GR_BACKGROUND_CENTER    1       /* Draw in center of window */
#define GR_BACKGROUND_TOPLEFT   2       /* Draw at top left of window */
#define GR_BACKGROUND_STRETCH   4       /* Stretch image to fit window*/
#define GR_BACKGROUND_TRANS     8       /* Don't fill in gaps */

/* GrNewPixmapFromData flags*/
#define GR_BMDATA_BYTEREVERSE   01      /* byte-reverse bitmap data*/
#define GR_BMDATA_BYTESWAP      02      /* byte-swap bitmap data*/

#if 0 /* don't define unimp'd flags*/
/* Window property flags */
#define GR_WM_PROP_NORESIZE     0x04    /* don't let user resize window */
#define GR_WM_PROP_NOICONISE    0x08    /* don't let user iconise window */
#define GR_WM_PROP_NOWINMENU    0x10    /* don't display a window menu button */
#define GR_WM_PROP_NOROLLUP     0x20    /* don't let user roll window up */
#define GR_WM_PROP_ONTOP        0x200   /* try to keep window always on top */
#define GR_WM_PROP_STICKY       0x400   /* keep window after desktop change */
#define GR_WM_PROP_DND          0x2000  /* accept drag and drop icons */
#endif

/* Window properties*/
#define GR_WM_PROPS_NOBACKGROUND 0x00000001L/* Don't draw window background*/
#define GR_WM_PROPS_NOFOCUS      0x00000002L /* Don't set focus to this window*/
#define GR_WM_PROPS_NOMOVE       0x00000004L /* Don't let user move window*/
#define GR_WM_PROPS_NORAISE      0x00000008L /* Don't let user raise window*/
#define GR_WM_PROPS_NODECORATE   0x00000010L /* Don't redecorate window*/
#define GR_WM_PROPS_NOAUTOMOVE   0x00000020L /* Don't move window on 1st map*/
#define GR_WM_PROPS_NOAUTORESIZE 0x00000040L /* Don't resize window on 1st map*/

/* default decoration style*/
#define GR_WM_PROPS_APPWINDOW   0x00000000L /* Leave appearance to WM*/
#define GR_WM_PROPS_APPMASK     0xF0000000L /* Appearance mask*/
#define GR_WM_PROPS_BORDER      0x80000000L /* Single line border*/
#define GR_WM_PROPS_APPFRAME    0x40000000L /* 3D app frame (overrides border)*/
#define GR_WM_PROPS_CAPTION     0x20000000L /* Title bar*/
#define GR_WM_PROPS_CLOSEBOX    0x10000000L /* Close box*/
#define GR_WM_PROPS_MAXIMIZE    0x08000000L /* Application is maximized*/

/* Flags for indicating valid bits in GrSetWMProperties call*/
#define GR_WM_FLAGS_PROPS       0x0001  /* Properties*/
#define GR_WM_FLAGS_TITLE       0x0002  /* Title*/
#define GR_WM_FLAGS_BACKGROUND  0x0004  /* Background color*/
#define GR_WM_FLAGS_BORDERSIZE  0x0008  /* Border size*/
#define GR_WM_FLAGS_BORDERCOLOR 0x0010  /* Border color*/

/* Error codes */
#define GR_ERROR_BAD_WINDOW_ID          1
#define GR_ERROR_BAD_GC_ID              2
#define GR_ERROR_BAD_CURSOR_SIZE        3
#define GR_ERROR_MALLOC_FAILED          4
#define GR_ERROR_BAD_WINDOW_SIZE        5
#define GR_ERROR_KEYBOARD_ERROR         6
#define GR_ERROR_MOUSE_ERROR            7
#define GR_ERROR_INPUT_ONLY_WINDOW      8
#define GR_ERROR_ILLEGAL_ON_ROOT_WINDOW 9
#define GR_ERROR_TOO_MUCH_CLIPPING      10
#define GR_ERROR_SCREEN_ERROR           11
#define GR_ERROR_UNMAPPED_FOCUS_WINDOW  12
#define GR_ERROR_BAD_DRAWING_MODE       13
#define GR_ERROR_BAD_LINE_ATTRIBUTE     14
#define GR_ERROR_BAD_FILL_MODE          15
#define GR_ERROR_BAD_REGION_ID          16

/* Event types.
 * Mouse motion is generated for every motion of the mouse, and is used to
 * track the entire history of the mouse (many events and lots of overhead).
 * Mouse position ignores the history of the motion, and only reports the
 * latest position of the mouse by only queuing the latest such event for
 * any single client (good for rubber-banding).
 */
#define GR_EVENT_TYPE_ERROR             (-1)
#define GR_EVENT_TYPE_NONE              0
#define GR_EVENT_TYPE_EXPOSURE          1
#define GR_EVENT_TYPE_BUTTON_DOWN       2
#define GR_EVENT_TYPE_BUTTON_UP         3
#define GR_EVENT_TYPE_MOUSE_ENTER       4
#define GR_EVENT_TYPE_MOUSE_EXIT        5
#define GR_EVENT_TYPE_MOUSE_MOTION      6
#define GR_EVENT_TYPE_MOUSE_POSITION    7
#define GR_EVENT_TYPE_KEY_DOWN          8
#define GR_EVENT_TYPE_KEY_UP            9
#define GR_EVENT_TYPE_FOCUS_IN          10
#define GR_EVENT_TYPE_FOCUS_OUT         11
#define GR_EVENT_TYPE_FDINPUT           12
#define GR_EVENT_TYPE_UPDATE            13
#define GR_EVENT_TYPE_CHLD_UPDATE       14
#define GR_EVENT_TYPE_CLOSE_REQ         15
#define GR_EVENT_TYPE_TIMEOUT           16
#define GR_EVENT_TYPE_SCREENSAVER       17
#define GR_EVENT_TYPE_CLIENT_DATA_REQ   18
#define GR_EVENT_TYPE_CLIENT_DATA       19
#define GR_EVENT_TYPE_SELECTION_CHANGED 20
#define GR_EVENT_TYPE_TIMER             21
#define GR_EVENT_TYPE_PORTRAIT_CHANGED  22
#define GR_EVENT_TYPE_HOTKEY_DOWN       23
#define GR_EVENT_TYPE_HOTKEY_UP         24

/* Event masks */
#define GR_EVENTMASK(n)                 (1 << (n))

#define GR_EVENT_MASK_NONE              GR_EVENTMASK(GR_EVENT_TYPE_NONE)
#define GR_EVENT_MASK_ERROR             0x80000000L
#define GR_EVENT_MASK_EXPOSURE          GR_EVENTMASK(GR_EVENT_TYPE_EXPOSURE)
#define GR_EVENT_MASK_BUTTON_DOWN       GR_EVENTMASK(GR_EVENT_TYPE_BUTTON_DOWN)
#define GR_EVENT_MASK_BUTTON_UP         GR_EVENTMASK(GR_EVENT_TYPE_BUTTON_UP)
#define GR_EVENT_MASK_MOUSE_ENTER       GR_EVENTMASK(GR_EVENT_TYPE_MOUSE_ENTER)
#define GR_EVENT_MASK_MOUSE_EXIT        GR_EVENTMASK(GR_EVENT_TYPE_MOUSE_EXIT)
#define GR_EVENT_MASK_MOUSE_MOTION      GR_EVENTMASK(GR_EVENT_TYPE_MOUSE_MOTION)
#define GR_EVENT_MASK_MOUSE_POSITION    GR_EVENTMASK(GR_EVENT_TYPE_MOUSE_POSITION)
#define GR_EVENT_MASK_KEY_DOWN          GR_EVENTMASK(GR_EVENT_TYPE_KEY_DOWN)
#define GR_EVENT_MASK_KEY_UP            GR_EVENTMASK(GR_EVENT_TYPE_KEY_UP)
#define GR_EVENT_MASK_FOCUS_IN          GR_EVENTMASK(GR_EVENT_TYPE_FOCUS_IN)
#define GR_EVENT_MASK_FOCUS_OUT         GR_EVENTMASK(GR_EVENT_TYPE_FOCUS_OUT)
#define GR_EVENT_MASK_FDINPUT           GR_EVENTMASK(GR_EVENT_TYPE_FDINPUT)
#define GR_EVENT_MASK_UPDATE            GR_EVENTMASK(GR_EVENT_TYPE_UPDATE)
#define GR_EVENT_MASK_CHLD_UPDATE       GR_EVENTMASK(GR_EVENT_TYPE_CHLD_UPDATE)
#define GR_EVENT_MASK_CLOSE_REQ         GR_EVENTMASK(GR_EVENT_TYPE_CLOSE_REQ)
#define GR_EVENT_MASK_TIMEOUT           GR_EVENTMASK(GR_EVENT_TYPE_TIMEOUT)
#define GR_EVENT_MASK_SCREENSAVER       GR_EVENTMASK(GR_EVENT_TYPE_SCREENSAVER)
#define GR_EVENT_MASK_CLIENT_DATA_REQ   GR_EVENTMASK(GR_EVENT_TYPE_CLIENT_DATA_REQ)
#define GR_EVENT_MASK_CLIENT_DATA       GR_EVENTMASK(GR_EVENT_TYPE_CLIENT_DATA)
#define GR_EVENT_MASK_SELECTION_CHANGED GR_EVENTMASK(GR_EVENT_TYPE_SELECTION_CHANGED)
#define GR_EVENT_MASK_TIMER             GR_EVENTMASK(GR_EVENT_TYPE_TIMER)
#define GR_EVENT_MASK_PORTRAIT_CHANGED  GR_EVENTMASK(GR_EVENT_TYPE_PORTRAIT_CHANGED)
/* Event mask does not affect GR_EVENT_TYPE_HOTKEY_DOWN and
 * GR_EVENT_TYPE_HOTKEY_UP, hence no masks for those events. */

#define GR_EVENT_MASK_ALL               ((GR_EVENT_MASK) -1L)

/* update event types */
#define GR_UPDATE_MAP           1
#define GR_UPDATE_UNMAP         2
#define GR_UPDATE_MOVE          3
#define GR_UPDATE_SIZE          4
#define GR_UPDATE_UNMAPTEMP     5       /* unmap during window move/resize*/
#define GR_UPDATE_ACTIVATE      6       /* toplevel window [de]activate*/
#define GR_UPDATE_DESTROY       7
#define GR_UPDATE_REPARENT      8

#undef WHITE
#undef LTGRAY
#undef GRAY
#undef DKGRAY
#undef LTGREY
#undef GREY
#undef DKGREY
#undef BLACK

#undef BLACK
#undef BLUE
#undef GREEN
#undef CYAN
#undef RED
#undef MAGENTA
#undef BROWN
#undef LTGRAY
#undef GRAY
#undef LTBLUE
#undef LTGREEN
#undef LTCYAN
#undef LTRED
#undef LTMAGENTA
#undef YELLOW
#undef WHITE

#define BLACK GR_RGB(0,0,0)
#define DKGREY GR_RGB(80,80,80)
#define LTGREY GR_RGB(160,160,160)
#define WHITE GR_RGB(255,255,255)
#define GREY DKGREY
#define GRAY GREY
#define DKGRAY DKGREY
#define LTGRAY LTGREY

#define BLUE            GR_RGB( 0  , 0  , 128 )
#define GREEN           GR_RGB( 0  , 128, 0   )
#define CYAN            GR_RGB( 0  , 128, 128 )
#define RED             GR_RGB( 128, 0  , 0   )
#define MAGENTA         GR_RGB( 128, 0  , 128 )
#define BROWN           GR_RGB( 128, 64 , 0   )
#define LTBLUE          GR_RGB( 0  , 0  , 255 )
#define LTGREEN         GR_RGB( 0  , 255, 0   )
#define LTCYAN          GR_RGB( 0  , 255, 255 )
#define LTRED           GR_RGB( 255, 0  , 0   )
#define LTMAGENTA       GR_RGB( 255, 0  , 255 )
#define YELLOW          GR_RGB( 255, 255, 0   )

#undef MWARC
#undef MWOUTLINE
#undef MWARCOUTLINE
#undef MWPIE
#undef MWELLIPSE
#undef MWELLIPSEFILL

#define MWARC           0x0001  /* arc*/
#define MWOUTLINE       0x0002
#define MWARCOUTLINE    0x0003  /* arc + outline*/
#define MWPIE           0x0004  /* pie (filled)*/
#define MWELLIPSE       0x0008  /* ellipse outline*/
#define MWELLIPSEFILL   0x0010  /* ellipse filled*/

#endif // !defined MWBACKEND

// Typedef stuff.
typedef unsigned short t_GR_BITMAP;
typedef int t_GR_COLOR;
typedef int t_GR_COORD;
typedef int t_GR_SIZE;
typedef TWindow *t_GR_DRAW_ID;
typedef ttk_font t_GR_FONT_ID;
typedef ttk_gc t_GR_GC_ID;
typedef ttk_surface t_GR_IMAGE_ID;
typedef ttk_point t_GR_POINT;
typedef TWidget *t_GR_TIMER_ID;
typedef TWindow *t_GR_WINDOW_ID;
typedef TWindow *t_GR_PIXMAP_ID;
typedef struct 
{
    int        rows;
    int        cols;
    int        xdpcm;
    int        ydpcm;
    int        planes;
    int        bpp;
    long       ncolors;
    int        fonts;
    int        buttons;
    int        modifiers;
    int        pixtype;
    int        xpos;
    int        ypos;
    int        vs_width;
    int        vs_height;
    int        ws_width;
    int        ws_height;
} t_GR_SCREEN_INFO;

typedef struct {
    int wid;             /**< window id (or 0 if no such window) */
    int parent;          /**< parent window id */
    int child;           /**< first child window id (or 0) */
    int sibling;         /**< next sibling window id (or 0) */
    int inputonly;            /**< TRUE if window is input only */
    int mapped;               /**< TRUE if window is mapped */
    int realized;             /**< TRUE if window is mapped and visible */
    int x;                   /**< parent-relative x position of window */
    int y;                   /**< parent-relative  y position of window */
    int width;                /**< width of window */
    int height;               /**< height of window */
    int bordersize;           /**< size of border */
    int bordercolor;         /**< color of border */
    int background;          /**< background color */
    int eventmask;      /**< current event mask for this client */
    void *props;            /**< window properties */
    int cursor;          /**< cursor id*/
    unsigned long processid;      /**< process id of owner*/
} t_GR_WINDOW_INFO;

typedef struct {
    t_GR_GC_ID gcid;                /**< GC id (or 0 if no such GC) */
    int mode;                     /**< drawing mode */
    int region;          /**< user region */
    int xoff;                     /**< x offset of user region */
    int yoff;                     /**< y offset of user region */
    t_GR_FONT_ID font;              /**< font number */
    t_GR_COLOR foreground;          /**< foreground RGB color or pixel value */
    t_GR_COLOR background;          /**< background RGB color or pixel value */
    int fgispixelval;         /**< TRUE if 'foreground' is actually a GR_PIXELVAL */
    int bgispixelval;         /**< TRUE if 'background' is actually a GR_PIXELVAL */
    int usebackground;        /**< use background in bitmaps */
    int exposure;             /**< send exposure events on GrCopyArea */
} t_GR_GC_INFO;

typedef struct {
  int type;           /**< event type */
  const char *name;            /**< function name which failed */
  int code;                /**< error code */
  int id;                     /**< resource id (maybe useless) */
} t_GR_EVENT_ERROR;

/**
 * Event for a mouse button pressed down or released.
 */
typedef struct {
  int type;           /**< event type */
  t_GR_WINDOW_ID wid;             /**< window id event delivered to */
  t_GR_WINDOW_ID subwid;          /**< sub-window id (pointer was in) */
  int rootx;               /**< root window x coordinate */
  int rooty;               /**< root window y coordinate */
  int x;                   /**< window x coordinate of mouse */
  int y;                   /**< window y coordinate of mouse */
  int buttons;            /**< current state of all buttons */
  int changebuttons;      /**< buttons which went down or up */
  int modifiers;          /**< modifiers (, etc)*/
  int time;              /**< tickcount time value*/
} t_GR_EVENT_BUTTON;

/**
 * Event for a keystroke typed for the window with has focus.
 */
typedef struct {
  int type;           /**< event type */
  t_GR_WINDOW_ID wid;             /**< window id event delived to */
  t_GR_WINDOW_ID subwid;          /**< sub-window id (pointer was in) */
  int rootx;               /**< root window x coordinate */
  int rooty;               /**< root window y coordinate */
  int x;                   /**< window x coordinate of mouse */
  int y;                   /**< window y coordinate of mouse */
  int buttons;            /**< current state of buttons */
  int modifiers;          /**< modifiers (, etc)*/
  unsigned short ch;                    /**< 16-bit unicode key value, xxx */
  unsigned short scancode;         /**< OEM scancode value if available*/
  int hotkey;               /**< TRUE if generated from GrGrabKey(t_GR_GRAB_HOTKEY_x) */
} t_GR_EVENT_KEYSTROKE;

/**
 * Event for exposure for a region of a window.
 */
typedef struct {
  int type;           /**< event type */
  t_GR_WINDOW_ID wid;             /**< window id */
  int x;                   /**< window x coordinate of exposure */
  int y;                   /**< window y coordinate of exposure */
  int width;                /**< width of exposure */
  int height;               /**< height of exposure */
} t_GR_EVENT_EXPOSURE;

/**
 * General events for focus in or focus out for a window, or mouse enter
 * or mouse exit from a window, or window unmapping or mapping, etc.
 * Server portrait mode changes are also sent using this event to
 * all windows that request it.
 */
typedef struct {
  int type;           /**< event type */
  t_GR_WINDOW_ID wid;             /**< window id */
  t_GR_WINDOW_ID otherid;         /**< new/old focus id for focus events*/
} t_GR_EVENT_GENERAL;

/**
 * Events for mouse motion or mouse position.
 */
typedef struct {
  int type;           /**< event type */
  t_GR_WINDOW_ID wid;             /**< window id event delivered to */
  t_GR_WINDOW_ID subwid;          /**< sub-window id (pointer was in) */
  int rootx;               /**< root window x coordinate */
  int rooty;               /**< root window y coordinate */
  int x;                   /**< window x coordinate of mouse */
  int y;                   /**< window y coordinate of mouse */
  int buttons;            /**< current state of buttons */
  int modifiers;          /**< modifiers (, etc)*/
} t_GR_EVENT_MOUSE;

/**
 * GrRegisterInput() event.
 */
typedef struct {
  int type;           /**< event type */
  int           fd;             /**< input file descriptor*/
} t_GR_EVENT_FDINPUT;

/**
 * int_UPDATE
 */
typedef struct {
  int type;           /**< event type */
  t_GR_WINDOW_ID wid;             /**< select window id*/
  t_GR_WINDOW_ID subwid;          /**< update window id (=wid for UPDATE event)*/
  int x;                   /**< new window x coordinate */
  int y;                   /**< new window y coordinate */
  int width;                /**< new width */
  int height;               /**< new height */
  int utype;         /**< update_type */
} t_GR_EVENT_UPDATE;

/**
 * int_SCREENSAVER
 */
typedef struct {
  int type;           /**< event type */
  int activate;             /**< true = activate, false = deactivate */
} t_GR_EVENT_SCREENSAVER;

/**
 * int_CLIENT_DATA_REQ
 */
typedef struct {
  int type;           /**< event type */
  t_GR_WINDOW_ID wid;             /**< ID of requested window */
  t_GR_WINDOW_ID rid;             /**< ID of window to send data to */
  int serial;           /**< Serial number of transaction */
  int mimetype;         /**< Type to supply data as */
} t_GR_EVENT_CLIENT_DATA_REQ;

/**
 * int_CLIENT_DATA
 */
typedef struct {
  int type;           /**< event type */
  t_GR_WINDOW_ID wid;             /**< ID of window data is destined for */
  t_GR_WINDOW_ID rid;             /**< ID of window data is from */
  int serial;           /**< Serial number of transaction */
  unsigned long len;            /**< Total length of data */
  unsigned long datalen;        /**< Length of following data */
  void *data;                   /**< Pointer to data (filled in on client side) */
} t_GR_EVENT_CLIENT_DATA;

/**
 * int_SELECTION_CHANGED
 */
typedef struct {
  int type;           /**< event type */
  t_GR_WINDOW_ID new_owner;       /**< ID of new selection owner */
} t_GR_EVENT_SELECTION_CHANGED;

/**
 * int_TIMER
 */
typedef struct {
  int  type;          /**< event type, int_TIMER */
  t_GR_WINDOW_ID   wid;           /**< ID of window timer is destined for */
  t_GR_TIMER_ID    tid;           /**< ID of expired timer */
} t_GR_EVENT_TIMER;

/**
 * Union of all possible event structures.
 * This is the structure returned by GrGetNextEvent() and similar routines.
 */
typedef union {
  int type;                   /**< event type */
  t_GR_EVENT_ERROR error;                 /**< error event */
  t_GR_EVENT_GENERAL general;             /**< general window events */
  t_GR_EVENT_BUTTON button;               /**< button events */
  t_GR_EVENT_KEYSTROKE keystroke;         /**< keystroke events */
  t_GR_EVENT_EXPOSURE exposure;           /**< exposure events */
  t_GR_EVENT_MOUSE mouse;                 /**< mouse motion events */
  t_GR_EVENT_FDINPUT fdinput;             /**< fd input events*/
  t_GR_EVENT_UPDATE update;               /**< window update events */
  t_GR_EVENT_SCREENSAVER screensaver;     /**< Screen saver events */
  t_GR_EVENT_CLIENT_DATA_REQ clientdatareq; /**< Request for client data events */
  t_GR_EVENT_CLIENT_DATA clientdata;        /**< Client data events */
  t_GR_EVENT_SELECTION_CHANGED selectionchanged; /**< Selection owner changed */
  t_GR_EVENT_TIMER timer;                 /**< Timer events */
} t_GR_EVENT;

// Func prototypes of the above 40 functions go here.
int t_GrOpen();
void t_GrClose();
void t_GrGetScreenInfo (t_GR_SCREEN_INFO *inf);
int t_GrPeekEvent (t_GR_EVENT *ev);
int t_GrGetNextEventTimeout (t_GR_EVENT *ev, int timeout);

t_GR_WINDOW_ID t_GrNewWindow (int _unused1, int x, int y, int w, int h, int _unused2, int _unused3, int _unused4);
t_GR_WINDOW_ID t_GrNewWindowEx (int _unused1, const char *title, int _unused3, int x, int y, int w, int h, int _unused4);
void t_GrDestroyWindow (t_GR_WINDOW_ID w);
void t_GrMapWindow (t_GR_WINDOW_ID w);
void t_GrUnmapWindow (t_GR_WINDOW_ID w);
void t_GrResizeWindow (t_GR_WINDOW_ID win, int w, int h);
void t_GrMoveWindow (t_GR_WINDOW_ID win, int x, int y);
void t_GrClearWindow (t_GR_WINDOW_ID w, int _unused);
void t_GrGetWindowInfo (t_GR_WINDOW_ID w, t_GR_WINDOW_INFO *inf);
t_GR_WINDOW_ID t_GrGetFocus();
t_GR_WINDOW_ID t_GrNewPixmap (int w, int h, void *_unused1);

t_GR_GC_ID t_GrNewGC();
t_GR_GC_ID t_GrCopyGC (t_GR_GC_ID other);
void t_GrDestroyGC (t_GR_GC_ID gc);
void t_GrSetGCForeground (t_GR_GC_ID gc, t_GR_COLOR col);
void t_GrSetGCBackground (t_GR_GC_ID gc, t_GR_COLOR col);
void t_GrSetGCUseBackground (t_GR_GC_ID gc, int flag);
void t_GrSetGCMode (t_GR_GC_ID gc, int mode);
void t_GrSetGCFont (t_GR_GC_ID gc, t_GR_FONT_ID font);
void t_GrGetGCInfo (t_GR_GC_ID gc, t_GR_GC_INFO *inf);

void t_GrPoint (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y);
void t_GrLine (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x1, int y1, int x2, int y2);
void t_GrRect (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, int w, int h);
void t_GrFillRect (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, int w, int h);
void t_GrPoly (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int n, t_GR_POINT *v);
void t_GrFillPoly (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int n, t_GR_POINT *v);
void t_GrEllipse (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, int rx, int ry);
void t_GrFillEllipse (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, int rx, int ry);
void t_GrCopyArea (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int dx, int dy, int sw, int sh, t_GR_DRAW_ID src, int sx, int sy, int _unused1);
void t_GrBitmap (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, int w, int h, t_GR_BITMAP *imgbits);
t_GR_IMAGE_ID t_GrLoadImageFromFile (const char *file, int _unused1);
void t_GrDrawImageToFit (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, int w, int h, t_GR_IMAGE_ID img);
void t_GrFreeImage (t_GR_IMAGE_ID img);
void t_GrText (t_GR_DRAW_ID dst, t_GR_GC_ID gc, int x, int y, const void *str, int count, int _unused);

struct t_GR_LOGFONT;
t_GR_FONT_ID t_GrCreateFont (const char *path, int size, struct t_GR_LOGFONT *_unused1);
void t_GrDestroyFont (t_GR_FONT_ID font);
void t_GrGetGCTextSize (t_GR_GC_ID gc, const void *str, int count, int flags, int *width, int *height, int *base);

t_GR_TIMER_ID t_GrCreateTimer (t_GR_WINDOW_ID win, int ms);
void t_GrDestroyTimer (t_GR_TIMER_ID tim);

#endif // !defined _TTK_MWIN_EMU_H_
