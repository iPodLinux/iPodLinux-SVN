\name Atari ST TOS

\def white #fff
\def black #000
\def green #0f0


  header: bg => white, fg => black, line => black, accent => black
	  shine => black, shadow => black,
	  gradient.top => green,
	  gradient.middle => white,
	  gradient.bottom => black,
	  gradient.bar => white

   music: bar => green +2

    menu: bg => white, fg => black, choice => black, icon => black,
          selbg => black, selfg => white,
	  selchoice => white, 
	  icon0 => black, icon1 => white, icon2 => black, icon3 => white

  scroll: box => white, bg => black +1, bar => green +2

 battery: border => black, bg => white, fill.normal => green, 
		fill.low => black, fill.charge => black +1,
		bg.low => green, bg.charging => green

    lock: border => black, fill => black

 loadavg: bg => white, fg => green, spike => black

  window: bg => green, fg => black, border => white -3


  dialog: bg => white, fg => black, line => green,
          title.fg => black,
          button.bg => white, button.fg => white, button.border => white,
	  button.sel.bg => green, button.sel.fg => black, 
	  button.sel.border => white, button.sel.inner => green +1

  error:  bg => white, fg => black, line => green,
          title.fg => black,
          button.bg => white, button.fg => white, button.border => white,
	  button.sel.bg => green, button.sel.fg => black, 
	  button.sel.border => white, button.sel.inner => green +1

   input: bg => white, fg => black, selbg => green, selfg => black,
	  border => black, cursor => green

  slider: border => black, bg => white, full => green

textarea: bg => black, fg => white

# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => white,
	default.fg => black,
	default.border => black,
	selected.bg => black,
	selected.fg => white,
	selected.border => green,
	special.bg => green,
	special.fg => black,
	special.border => black

button:
	default.bg => white,
	default.fg => black,
	default.border => black,
	selected.bg => green,
	selected.fg => black,
	selected.border => black
