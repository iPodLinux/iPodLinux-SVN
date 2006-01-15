\name Inverted
\def white  #000000
\def black  #ffffff
\def dkgray #a0a0a0
\def gray   #505050

  header: bg => white, fg => black, line => black -1, accent => gray
	  shadow => gray, shine => dkgray,
	  gradient.top => dkgray,
	  gradient.middle => white,
	  gradient.bottom => white,
	  gradient.bar => gray
   music: bar => dkgray
 battery: border => black, bg => white, fill.normal => black +1, fill.low => dkgray +1, fill.charge => black +1,
	  bg.low => black, bg.charging =>white

    lock: border => black, fill => black
 loadavg: bg => white, fg => dkgray, spike => black

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
          selbg => black, selfg => white, selchoice => white,
          icon0 => black, icon1 => dkgray, icon2 => gray, icon3 => white
  slider: border => black, bg => white, full => black
textarea: bg => white, fg => black

calendar: 	bg.selected => black,
		bg.normal => white,
		bg.today => dkgray,
		border.sides.selected => gray,
		border.sides.normal => gray,
		border.sides.today => gray,
		border.top.selected => gray, 
		border.top.normal => gray, 
		border.top.today => gray,
		border.bottom.selected => gray, 
		border.bottom.normal => gray, 
		border.bottom.today => gray,
		corner.selected => white, 
		corner.normal => white, 
		corner.today => white,
		text.selected => white,
		text.normal => black,
		text.today => white
