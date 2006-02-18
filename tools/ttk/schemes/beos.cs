\name BeOS
# BeOS style color scheme
# 2006-02-17 by BleuLlama

\def yellow    #F9C822
\def dkyellow  #B57C14
\def ltyellow  #ff0
\def midyellow #EBA91D
\def black     #000

\def white     #fff
\def blue      #00f
\def red       #f00
\def green     #0f0

\def gray      #b2b2b2
\def dkgray    #8d8d8d
\def ltgray    #ededed


  header: bg => yellow, fg => black, line => gray, accent => midyellow
	  shine => ltyellow, shadow => dkyellow,
	  gradient.top => ltyellow,
	  gradient.middle => yellow,
	  gradient.bottom => dkyellow,
	  gradient.bar => white -3 

   music: bar => yellow +2

  window: bg => gray, fg => black, border => ltgray

    menu: bg => gray, fg => black, choice => black, icon => ltgray,
          selbg => dkgray, selfg => black,
	  selchoice => white, 
	  icon0 => black, icon1 => dkgray, icon2 => gray, icon3 => ltgray

 battery: border => dkyellow, bg => <dkgray to gray>,
		fill.normal => <ltyellow to dkyellow>,
		fill.low => red, fill.charge => <ltyellow to yellow>,
		bg.low => <dkyellow to white>,
		bg.charging => <dkgray to gray>

    lock: border => dkyellow, fill => black

  scroll: box => black, bg => dkgray +1, bar => <horiz ltyellow to yellow>+1

  slider: border => black, bg => dkgray, full => <vert ltyellow to yellow>

 loadavg: bg => black, fg => blue, spike => white


  dialog: bg => gray, fg => black, line => yellow,
          title.fg => black,
          button.bg => ltgray, button.fg => black, button.border => black,
	  button.sel.bg => yellow, button.sel.fg => black, button.sel.border => dkyellow, button.sel.inner => ltyellow +1

  error:  bg => blue, fg => black, line => yellow,
          title.fg => white,
          button.bg => ltgray, button.fg => black, button.border => black,
	  button.sel.bg => yellow, button.sel.fg => black, button.sel.border => dkyellow, button.sel.inner => ltyellow +1

   input: bg => ltgray, fg => black, selbg => yellow, selfg => black,
	  border => dkyellow, cursor => white

textarea: bg => ltyellow, fg => black


# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => <vert ltgray to gray>,
	default.fg => black,
	default.border => dkgray,

	selected.bg => <vert yellow to dkyellow>,
	selected.fg => black,
	selected.border => dkyellow,

	special.bg => <horiz dkyellow to yellow>,
	special.fg => black,
	special.border => black

button:
	default.bg => <vert ltgray to gray>,
	default.fg => black,
	default.border => black,
	selected.bg => <vert yellow to dkyellow>,
	selected.fg => black,
	selected.border => dkyellow
