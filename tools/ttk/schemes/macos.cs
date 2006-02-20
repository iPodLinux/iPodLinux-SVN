\name MacOS
# BMacOSeOS style color scheme
# 2006-02-19 by BleuLlama

\def yellow    #F9C822
\def dkyellow  #B57C14
\def ltyellow  #ff0
\def midyellow #EBA91D
\def black     #000

\def white     #fff
\def blue      #00f
\def red       #f00
\def green     #0f0

\def gray      #bbb
\def dkgray    #888
\def ltgray    #ddd

# this defines the color tint for the stuff.
\def accentblue	#313163


  header: bg => gray, fg => black, line => black-1, accent => accentblue
	  shine => ltgray, shadow => dkgray,
	  gradient.top => gray,
	  gradient.middle => white,
	  gradient.bottom => dkgray,
	  gradient.bar => white -3 

   music: bar => white +2

  window: bg => white, fg => black, border => ltgray

    menu: bg => white, fg => black, choice => black, icon => black,
          selbg => dkgray, selfg => white,
	  selchoice => white, 
	  icon0 => dkgray, icon1 => gray, icon2 => ltgray, icon3 => white

 battery: border => black, bg => <dkgray to gray>,
		fill.normal => <ltgray to dkgray>,
		fill.low => red, fill.charge => <ltgray to gray>,
		bg.low => <dkgray to white>,
		bg.charging => <dkgray to gray>

    lock: border => black, fill => black

  scroll: box => black, bg => dkgray +1, bar => <horiz ltgray to gray>+1

  slider: border => black, bg => dkgray, full => <vert ltgray to gray>

 loadavg: bg => black, fg => blue, spike => white


  dialog: bg => gray, fg => black, line => gray,
          title.fg => black,
          button.bg => ltgray, button.fg => black, button.border => black,
	  button.sel.bg => gray, button.sel.fg => black, button.sel.border => dkgray, button.sel.inner => ltgray +1

  error:  bg => blue, fg => black, line => gray,
          title.fg => white,
          button.bg => ltgray, button.fg => black, button.border => black,
	  button.sel.bg => gray, button.sel.fg => black, button.sel.border => dkgray, button.sel.inner => ltgray +1

   input: bg => ltgray, fg => black, selbg => gray, selfg => black,
	  border => dkgray, cursor => white

textarea: bg => ltgray, fg => black


# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => <vert ltgray to gray>,
	default.fg => black,
	default.border => dkgray,

	selected.bg => <vert gray to dkgray>,
	selected.fg => black,
	selected.border => dkgray,

	special.bg => <horiz dkgray to gray>,
	special.fg => black,
	special.border => black

button:
	default.bg => <vert ltgray to gray>,
	default.fg => black,
	default.border => black,
	selected.bg => <vert gray to dkgray>,
	selected.fg => black,
	selected.border => dkgray
