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

  header: bg => white, fg => black, line => #808888 -1, accent => #ff0000
          shadow => #0039b3, shine => #87d0ff,
          gradient.top => graptop,
          gradient.middle => grapmid,
          gradient.bottom => grapbot,
          gradient.bar => grapbar +1
   music: bar => @familiar-(music.bar).png ,
          bar.bg => @familiar-(music.bar.bg).png
 battery: border => #606C78,
          bg => <vert #a0adb8 to #d8e2e6>,
          fill.normal => <#80e141, #6dd433, #258208 with #aefa73 @:1,0,5,0>,
          fill.low => #C03020,
          fill.charge => <#80e141, #6dd433, #258208 with #aefa73 @:1,0,5,0>,
          bg.low =>  <vert #a0adb8 to #d8e2e6>,
          bg.charging => <vert #a0adb8 to #d8e2e6>
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
          selbg => <vert #209bd6 to #0073bc>,
          selfg => white, selchoice => white,
# WTF are these used for?
          icon0 => #3b79da, icon1 => #28503c, icon2 => #50a078, icon3 => #ffffff
  slider: border => #282828, bg => #e8ebd2, full => #acb098
textarea: bg => #ffffff, fg => nearblack

calendar:       
	bg.selected => <vert #3f80de to #2f63d5 to #1e41cd>,
	bg.normal => <vert #c3d6ff to #b4caf6 to #9dbaf6>,
	bg.today => <vert #d5d6d5 to #d1cfd1 to #c5c6c5>,
	border.sides.selected => <vert #3c77cc to #2d5fc7 to #1e3fbf>,
	border.sides.normal => <vert #9cb3e4 to #98aee2 to #839edc>,
	border.sides.today => <vert #bebdbe to #bbbabb to #b4b4b4>,
	border.top.selected => #3d7bd2,
	border.top.normal => #7f95db,
	border.top.today => #939393,
	border.bottom.selected => #183abe,
	border.bottom.normal => #89a7e7,
	border.bottom.today => #b9b9b9,
	corner.selected => #9ab2da,
	corner.normal => #c3cbe8,
	corner.today => #cecece,
	text.selected => white,
	text.normal => black,
	text.today => black
