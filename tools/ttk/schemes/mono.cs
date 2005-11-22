\name Standard (B&W)
\def black  #000000
\def white  #ffffff
\def gray   #a0a0a0
\def dkgray #505050

  header: bg => white, fg => black, line => black -1, accent => gray
 battery: border => black, bg => white, fill.normal => black +1, fill.low => dkgray +1, fill.charge => black +1
    lock: border => black, fill => black
 loadavg: bg => gray, fg => dkgray, spike => black

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
          selbg => black, selfg => white, selchoice => white, icon1 => dkgray, icon2 => gray, icon3 => white
  slider: border => black, bg => white, full => black
textarea: bg => white, fg => black
