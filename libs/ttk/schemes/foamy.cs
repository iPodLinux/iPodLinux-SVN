\name Foamy
# Sea Foam Green-ish color scheme
# based on the BeOS style color scheme
# thanks to c0nsumer for the name
# 2007-10-10 by BleuLlama

\def foamgreen    #22C8f9
\def dkfoamgreen  #147Cb5
\def ltfoamgreen  #0ff
\def midfoamgreen #1dA9eb
\def black     #000

\def white     #fff
\def blue      #00f
\def red       #f00
\def green     #0f0
\def yellow    #ff8

\def ltgray    #ccffff
\def gray      #00ffff
\def dkgray    #448888
\def dkrgray   #224444


  header: bg => <dkgray to dkgray to gray>, fg => yellow, 
	  line => gray, accent => midfoamgreen
	  shine => ltfoamgreen, shadow => dkfoamgreen,
	  gradient.top => ltfoamgreen,
	  gradient.middle => foamgreen,
	  gradient.bottom => dkfoamgreen,
	  gradient.bar => white -3 

   music: bar => foamgreen +2

  window: bg => gray, fg => black, border => ltgray

    menu: bg => gray, fg => black, choice => black, icon => ltgray,
	  hdrbg => <gray to ltgray to gray>, hdrfg => black,
          selbg => <gray to dkrgray to gray>, selfg => white,
	  selchoice => white, 
	  icon0 => black, icon1 => dkgray, icon2 => gray, icon3 => ltgray

 battery: border => dkfoamgreen, bg => <dkgray to gray>,
		fill.normal => <ltfoamgreen to dkfoamgreen>,
		fill.low => red, fill.charge => <ltfoamgreen to foamgreen>,
		bg.low => <dkfoamgreen to white>,
		bg.charging => <dkgray to gray>,
                chargingbolt => dkfoamgreen

    lock: border => white, fill => black

  scroll: box => gray,
	  bar => <horiz gray to black to gray>+1*2
	  bg => <horiz gray to dkgray to gray>

  slider: border => black, bg => <vert black to dkrgray to black>,
	  full => <black to gray to black>

 loadavg: bg => foamgreen, fg => dkfoamgreen, spike => ltfoamgreen


  dialog: bg => gray, fg => black, line => foamgreen,
          title.fg => black,
          button.bg => ltgray, button.fg => black, button.border => black,
	  button.sel.bg => foamgreen, button.sel.fg => black, button.sel.border => dkfoamgreen, button.sel.inner => ltfoamgreen +1

  error:  bg => foamgreen, fg => black, line => ltfoamgreen,
          title.fg => white,
          button.bg => ltgray, button.fg => black, button.border => black,
	  button.sel.bg => foamgreen, button.sel.fg => black, button.sel.border => dkfoamgreen, button.sel.inner => ltfoamgreen +1

   input: bg => ltgray, fg => black, selbg => foamgreen, selfg => black,
	  border => dkfoamgreen, cursor => white

textarea: bg => ltfoamgreen, fg => black


# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => <vert ltgray to gray>,
	default.fg => black,
	default.border => dkgray,

	selected.bg => <vert foamgreen to dkfoamgreen>,
	selected.fg => black,
	selected.border => dkfoamgreen,

	special.bg => <horiz dkfoamgreen to foamgreen>,
	special.fg => black,
	special.border => black

button:
	default.bg => <vert ltgray to gray>,
	default.fg => black,
	default.border => black,
	selected.bg => <vert foamgreen to dkfoamgreen>,
	selected.fg => black,
	selected.border => dkfoamgreen
