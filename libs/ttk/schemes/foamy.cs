\name Foamy
# Sea Foam Green-ish color scheme
# based on the BeOS style color scheme
# thanks to c0nsumer for the name
# 2007-10-10 by BleuLlama

\def yellow    #22C8f9
\def dkyellow  #147Cb5
\def ltyellow  #0ff
\def midyellow #1dA9eb
\def black     #000

\def white     #fff
\def blue      #00f
\def red       #f00
\def green     #0f0

\def gray      #90b2b2
\def dkgray    #608d8d
\def dkrgray   #507070
\def ltgray    #c0eded


  header: bg => yellow, fg => black, line => gray, accent => midyellow
	  shine => ltyellow, shadow => dkyellow,
	  gradient.top => ltyellow,
	  gradient.middle => yellow,
	  gradient.bottom => dkyellow,
	  gradient.bar => white -3 

   music: bar => yellow +2

  window: bg => gray, fg => black, border => ltgray

    menu: bg => gray, fg => black, choice => black, icon => ltgray,
	  hdrbg => <gray to black to gray>, hdrfg => ltyellow,
          selbg => <gray to dkyellow to gray>, selfg => black,
	  selchoice => white, 
	  icon0 => black, icon1 => dkgray, icon2 => gray, icon3 => ltgray

 battery: border => dkyellow, bg => <dkgray to gray>,
		fill.normal => <ltyellow to dkyellow>,
		fill.low => red, fill.charge => <ltyellow to yellow>,
		bg.low => <dkyellow to white>,
		bg.charging => <dkgray to gray>,
                chargingbolt => dkyellow

    lock: border => dkyellow, fill => black

  scroll: box => black, bg => <horiz black to dkgray>+1,
	  bar => <horiz dkgray to ltyellow to dkgray>+1

  slider: border => black, bg => dkgray, full => <vert ltyellow to yellow>

 loadavg: bg => yellow, fg => dkyellow, spike => ltyellow


  dialog: bg => gray, fg => black, line => yellow,
          title.fg => black,
          button.bg => ltgray, button.fg => black, button.border => black,
	  button.sel.bg => yellow, button.sel.fg => black, button.sel.border => dkyellow, button.sel.inner => ltyellow +1

  error:  bg => yellow, fg => black, line => ltyellow,
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
