# Amiga 1.x color scheme
#  brought in from pz0 by bleullama
#  originally created by bleullama
\name Amiga 1.x

\def blue   #0055AA
\def black  #000022
\def white  #ffffff
\def orange #ff8800


  header: bg => white, fg => blue, line => blue -1, accent => blue

 battery: border => black, bg => white, fill.normal => blue +1, 
		fill.low => orange +1, fill.charge => orange +1

    lock: border => orange, fill => black

 loadavg: bg => blue, fg => white, spike => orange

  window: bg => blue, fg => white, border => orange -3

  dialog: bg => blue, fg => white, line => white,
          title.fg => white,
          button.bg => blue, button.fg => white, button.border => white,
	  button.sel.bg => white, button.sel.fg => black, button.sel.border => white, button.sel.inner => black +1

  error:  bg => blue, fg => white, line => orange,
          title.fg => white,
          button.bg => blue, button.fg => white, button.border => white,
	  button.sel.bg => white, button.sel.fg => black, button.sel.border => white, button.sel.inner => black +1

  scroll: box => white, bg => black +1, bar => orange +2

   input: bg => white, fg => black, selbg => orange, selfg => black,
	  border => black, cursor => orange

    menu: bg => blue, fg => white, choice => white, icon => orange,
          selbg => orange, selfg => black,
	  selchoice => white, selicon => black

  slider: border => white, bg => black, full => orange

textarea: bg => blue, fg => white
