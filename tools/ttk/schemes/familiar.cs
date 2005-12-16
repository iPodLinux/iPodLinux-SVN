\name Familiar
# The colour scheme you all know
\def black  #000000
\def white  #ffffff
\def nearblack #111111

\def aquabtn     #ececec
\def aquadbtn    #6cabed
\def aquabtnbdr  #5f5f5f
\def aquadbtnbdr #272799
\def aquawinbdr  #b8b8b8

\def grapbot    #D0D8D8
\def grapmid    #E8F4E8
\def graptop    #F0F4F8
\def grapbar    #F0F4F8

  header: bg => white, fg => black, line => #808888 -1, accent => white
	  gradient.top => graptop,
	  gradient.middle => grapmid,
	  gradient.bottom => grapbot,
	  gradient.bar => grapbar +1
 battery: border => #606C78, bg => #C0D0D8, fill.normal => #68D028, fill.low => #C03020, fill.charge => #68D028,
	  bg.low => #C0D0D8, bg.charging => #C0D0D8
    lock: border => #282C28, fill => #383C40
 loadavg: bg => #E8F4E8, fg => #68D028, spike => #C0D0D8

  window: bg => white, fg => black, border => #808888 -1
  dialog: bg => white, fg => black, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
	      button.sel.bg => aquadbtn, button.sel.fg => black, button.sel.border => aquadbtnbdr, button.sel.inner => aquadbtn +1
   error: bg => white, fg => black, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
	  button.sel.bg => aquadbtn, button.sel.fg => black, button.sel.border => aquadbtnbdr, button.sel.inner => aquadbtn +1
  scroll: box => #b8b8b8, bg => #ebebeb +1, bar => #6cabed
   input: bg => white, fg => black, selbg => aquadbtn, selfg => black, border => aquawinbdr, cursor => #808080

    menu: bg => white, fg => black, choice => nearblack, icon => nearblack,
          selbg => #3675d2, selfg => white, selchoice => white,
# WTF are these used for?
          icon0 => #3b79da, icon1 => #28503c, icon2 => #50a078, icon3 => #ffffff
  slider: border => #282828, bg => #e8ebd2, full => #acb098
textarea: bg => #ffffff, fg => nearblack
