\name Broccoli
# Broccoli Green color scheme
# based on the BeOS style color scheme
# 2007-10-10 by BleuLlama

\def green    #22f922
\def dkgreen  #117C11
\def ltgreen  #0f0
\def midgreen #11A911
\def black     #000

\def white     #fff
\def blue      #00f
\def red       #f00

\def gray      #b0b0b0
\def dkgray    #808080
\def dkrgray   #707070
\def ltgray    #c0c0c0


  header: bg => midgreen, fg => black, line => gray, accent => green
	  shine => ltgreen, shadow => dkgreen,
	  gradient.top => ltgreen,
	  gradient.middle => green,
	  gradient.bottom => dkgreen,
	  gradient.bar => white -3 

   music: bar => green +2

  window: bg => gray, fg => black, border => ltgray

    menu: bg => gray, fg => black, choice => green, icon => dkgreen,
	  hdrbg => <gray to black to gray>, hdrfg => ltgreen,
          selbg => <gray to dkgreen to gray>, selfg => black,
	  selchoice => green, 
	  icon0 => black, icon1 => dkgreen,
	  icon2 => midgreen , icon3 => ltgreen

 battery: border => dkgreen, bg => <dkgray to gray>,
		fill.normal => <ltgreen to dkgreen>,
		fill.low => red, fill.charge => <ltgreen to green>,
		bg.low => <dkgreen to white>,
		bg.charging => <dkgray to gray>,
                chargingbolt => dkgreen

    lock: border => dkgreen, fill => black

  scroll: box => black, bg => <horiz black to dkgray>+1,
	  bar => <horiz dkgray to ltgreen to dkgray>+1

  slider: border => black, bg => dkgray, full => <vert ltgreen to green>

 loadavg: bg => green, fg => dkgreen, spike => ltgreen


  dialog: bg => gray, fg => black, line => green,
          title.fg => black,
          button.bg => ltgray, button.fg => black, button.border => black,
	  button.sel.bg => green, button.sel.fg => black, button.sel.border => dkgreen, button.sel.inner => ltgreen +1

  error:  bg => green, fg => black, line => ltgreen,
          title.fg => white,
          button.bg => ltgray, button.fg => black, button.border => black,
	  button.sel.bg => green, button.sel.fg => black, button.sel.border => dkgreen, button.sel.inner => ltgreen +1

   input: bg => ltgray, fg => black, selbg => green, selfg => black,
	  border => dkgreen, cursor => white

textarea: bg => ltgreen, fg => black


# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => <vert ltgray to gray>,
	default.fg => black,
	default.border => dkgray,

	selected.bg => <vert green to dkgreen>,
	selected.fg => black,
	selected.border => dkgreen,

	special.bg => <horiz dkgreen to green>,
	special.fg => black,
	special.border => black

button:
	default.bg => <vert ltgray to gray>,
	default.fg => black,
	default.border => black,
	selected.bg => <vert green to dkgreen>,
	selected.fg => black,
	selected.border => dkgreen
