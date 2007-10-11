\name Amiga 4.0
# Amiga OS 4.0 color scheme
# 2006-05-30 by BleuLlama

\def black     #000

\def blue4     #4774b3
\def ltblue4   #8fbcf5
\def bluebar   #4888b8
\def hltcol    #e2e2e2
\def purpledk  #322c78
\def purplelt  #7970c0

\def white     #fff
\def blue      #00f
\def red       #f00
\def green     #0f0

\def gray      #b2b2b2
\def dkgray    #8d8d8d
\def ltgray    #ededed


  header: bg => bluebar, fg => black, line => blue4-1, accent => hltcol,
	  shine => ltblue4, shadow => blue4,
	  gradient.top => ltblue4,
	  gradient.middle => blue4,
	  gradient.bottom => purplelt,
	  gradient.bar => white -3

   music: bar => <purpledk to purplelt>

  window: bg => gray, fg => black, border => ltgray

    menu: bg => gray, fg => black, choice => black, icon => ltgray,
	  hdrbg => purpledk+5, hdrfg => hltcol,
          selbg => bluebar, selfg => black,
	  selchoice => white, 
	  icon0 => black, icon1 => blue4, icon2 => ltblue4, icon3 => white

 battery: border => black, bg => <ltgray to gray>,
		fill.normal => <purplelt to purpledk>,
		fill.low => red, fill.charge => red,
		bg.low => <ltgray to gray>,
		bg.charging => <dkgray to gray>,
                chargingbolt => black

    lock: border => black, fill => purpledk

  scroll: box => black, bg => <horiz dkgray to ltgray>,
	    bar => <horiz purpledk to purplelt>+1

  slider: border => black, bg => <vert dkgray to ltgray>,
		full => <vert purpledk to purplelt>

 loadavg: bg => ltblue4, fg => purpledk, spike => white


  dialog: bg => gray, fg => black, line => blue4,
          title.fg => black,
          button.bg => ltgray, button.fg => black, button.border => black,
	  button.sel.bg => bluebar, button.sel.fg => black,
	  button.sel.border => red, button.sel.inner => white +1

  error:  bg => ltblue4, fg => black, line => blue4,
          title.fg => black,
          button.bg => ltgray, button.fg => black, button.border => black,
	  button.sel.bg => bluebar, button.sel.fg => black,
	  button.sel.border => red, button.sel.inner => white +1

   input: bg => ltgray, fg => black, selbg => ltblue4, selfg => black,
	  border => purpledk, cursor => purpledk

textarea: bg => ltgray, fg => black


# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => <vert ltgray to gray>,
	default.fg => black,
	default.border => dkgray,

	selected.bg => <vert ltgray to bluebar>,
	selected.fg => black,
	selected.border => blue4,

	special.bg => <horiz ltblue4 to purplelt>,
	special.fg => black,
	special.border => purpledk

button:
	default.bg => <vert ltgray to gray>,
	default.fg => black,
	default.border => black,
	selected.bg => <vert purplelt to ltblue4>,
	selected.fg => black,
	selected.border => purplelt
