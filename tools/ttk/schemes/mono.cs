\name Standard (B&W)
\def black  #000000
\def white  #ffffff
\def gray   #a0a0a0
\def dkgray #505050

  header: bg => white, fg => black, line => black -1, accent => gray
	  shadow => dkgray, shine => gray,
	  gradient.top => white,
	  gradient.middle => white,
	  gradient.bottom => dkgray,
	  gradient.bar => gray
   music: bar => dkgray
 battery: border => black, bg => white, fill.normal => black +1, fill.low => dkgray +1, fill.charge => black +1,
	  bg.low => white, bg.charging => gray
    lock: border => black, fill => black
 loadavg: bg => gray, fg => dkgray, spike => white

  window: bg => white, fg => black, border => gray -3
  dialog: bg => white, fg => black, line => black,
          title.fg => black,	
          button.bg => white, button.fg => black, button.border => gray,
	  button.sel.bg => gray, button.sel.fg => black, button.sel.border => black, button.sel.inner => black +1
   error: bg => gray, fg => black, line => black,
          title.fg => black,
          button.bg => gray, button.fg => black, button.border => black,
          button.sel.bg => dkgray, button.sel.fg => white, button.sel.border => black, button.sel.inner => gray +1
  scroll: box => black, bg => white +1, bar => dkgray +2
   input: bg => white, fg => black, selbg => dkgray, selfg => white, border => gray, cursor => dkgray

    menu: bg => white, fg => black, choice => black, icon => black,
          selbg => black, selfg => white, selchoice => white, icon0 => black,
          icon0 => black, icon1 => dkgray, icon2 => gray, icon3 => white
  slider: border => black, bg => white, full => black
textarea: bg => white, fg => black

# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => white,
	default.fg => black,
	default.border => gray,
	selected.bg => dkgray,
	selected.fg => white,
	selected.border => gray,
	special.bg => gray,
	special.fg => black,
	special.border => dkgray

button:
	default.bg => gray,
	default.fg => black,
	default.border => black,
	selected.bg => dkgray,
	selected.fg => white,
	selected.border => black
