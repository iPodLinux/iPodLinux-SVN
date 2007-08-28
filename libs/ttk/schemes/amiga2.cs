\name Amiga 2.x
# Amiga 2.x color scheme
#  brought in from pz0 by bleullama
#  originally created by bleullama

\def gray   #aaaaaa
\def white  #ffffff
\def black  #000000
\def oldblue   #83acd6
\def blue   #6688bb


  header: bg => blue, fg => black, line => black -1, accent => black
	  shine => white, shadow => black,
	  gradient.top => blue,
	  gradient.middle => white,
	  gradient.bottom => blue,
	  gradient.bar => white

   music: bar.bg => black, bar => white

 battery: border => black, bg => white, fill.normal => black +1, 
		fill.low => white +1, fill.charge => black +1,
		bg.low => blue, bg.charging => blue,
                chargingbolt => black

    lock: border => black, fill => black

 loadavg: bg => blue, fg => black, spike => white

  window: bg => gray, fg => black, border => white -3

  dialog: bg => blue, fg => black, line => gray,
          title.fg => black,
          button.bg => blue, button.fg => black, button.border => white,
	  button.sel.bg => black, button.sel.fg => white, button.sel.border => black, button.sel.inner => white +1

  error:  bg => blue, fg => black, line => white,
          title.fg => black,
          button.bg => blue, button.fg => black, button.border => white,
	  button.sel.bg => black, button.sel.fg => white, button.sel.border => black, button.sel.inner => white +1

  scroll: box => blue, bg => black +1, bar => white +2

   input: bg => gray, fg => black, selbg => blue, selfg => black,
	  border => black, cursor => white

    menu: bg => gray, fg => black, choice => white, icon => black,
          selbg => black, selfg => white,
	  selchoice => blue, icon0 => black, icon1 => #555555, icon2 => #aaaaaa, icon3 => white

  slider: border => white, bg => black, full => blue

textarea: bg => gray, fg => black

box:
	default.bg => blue,
	default.fg => black,
	default.border => black,
	selected.bg => white,
	selected.fg => black,
	selected.border => black,
	special.bg => gray,
	special.fg => black,
	special.border => white

button:
	default.bg => blue,
	default.fg => black,
	default.border => white,
	selected.bg => black,
	selected.fg => white,
	selected.border => black
