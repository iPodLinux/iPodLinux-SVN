\name Familiar
# The colour scheme you all know.
\def black  #000000
\def white  #ffffff
\def nearblack #111111

\def aquabtn     #ececec
\def aquadbtn    #6cabed
\def aquabtnbdr  #5f5f5f
\def aquadbtnbdr #272799
\def aquawinbdr  #b8b8b8

\def grapbot    #D0D8D8
\def grapmid    #F0F4F8
\def graptop    #F0F4F8

  header: bg => <graptop, grapmid, grapbot with #f0f4f8 @:1,0,50%,0>,
          fg => black, line => #808888, accent => #6ae,
          shadow => #0039b3, shine => #87d0ff
   music: bar => @familiar-(music.bar).png -2,
          bar.bg => @familiar-(music.bar.bg).png -2,
	  bar.border =>
 battery: border => #606C78,
          bg => <vert #a0adb8 to #d8e2e6>,
          fill.normal => <#80e141, #6dd433, #258208 with #aefa73 @:1,0,5,0>,
          fill.low => #C03020,
          fill.charge => <#80e141, #6dd433, #258208 with #aefa73 @:1,0,5,0>,
          bg.low =>  <vert #a0adb8 to #d8e2e6>,
          bg.charging => <vert #a0adb8 to #d8e2e6>,
          chargingbolt => #606C78
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
  scroll: box => #56585a -1,
          bg => <horiz #c0c8c8 to #d8e2e6 to #e8f2f3>,
          bar => @familiar-(scroll.bar).png
   input: bg => white, fg => black, selbg => aquadbtn, selfg => black, border => aquawinbdr, cursor => #808080

    menu: bg => white, fg => black, choice => nearblack, icon => nearblack,
          hdrbg => <vert #148fc8 to #0066af>, hdrfg => white,
          selbg => <vert #209bd6 to #0073bc>,
          selfg => white, selchoice => white,
# WTF are these used for?
          icon0 => #3b79da, icon1 => #28503c, icon2 => #50a078, icon3 => #ffffff
  slider: border => @familiar-(music.bar.bg).png -1,
          full => @familiar-(music.bar).png -2, bg =>
textarea: bg => #ffffff, fg => nearblack

box:
	default.bg => <vert #c3d6ff to #b4caf6 to #9dbaf6>,
	default.fg => black,
	default.border => #7f95db,
	selected.bg => <vert #3f80de to #2f63d5 to #1e41cd>,
	selected.fg => white,
	selected.border => #16a,
	special.bg => <vert #d5d6d5 to #d1cfd1 to #c5c6c5>,
	special.fg => black,
	special.border => #939393

button:
	default.bg => aquabtn,
	default.fg => black,
	default.border => aquabtnbdr,
	selected.bg => aquadbtn,
	selected.fg => black,
	selected.border => aquadbtnbdr
