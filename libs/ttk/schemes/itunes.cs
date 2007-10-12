\name iTunes
#\def black  #000000
#\def white  #ffffff
#\def gray   #a0a0a0
#\def dkgray #505050
#\def plistsbg    #e7edf6
#\def plistsfg    #000000
#\def plistsselbg #3b79da
#\def plistsselfg #ffffff
#\def playerbg    #e8ebd2
#\def playerfg    #282828
#\def playertext  #000000
#\def playertrack #acb098
#\def playerwid   #747669
\def metalbg     #b0b0b0
\def metalfg     #282828
\def metalwid    #5d5d5d
#\def metalline   #666666
#\def metalacc    #d6d6d6
#\def plistbg1    #ffffff
#\def plistbg2    #edf3fe
#\def plistfg     #000000
#\def plistselbg  #3d80df
#\def plistselfg  #ffffff
#\def plistwid1   #a6a6a6
#\def plistwid2   #9a9ea5
#\def pliststars  #7f7f7f
\def aquabg      #eeeeee
\def aquafg      #000000
\def aquabtn     #ececec
\def aquadbtn    #6cabed
\def aquabtnbdr  #5f5f5f
\def aquadbtnbdr #272799
\def aquawinbdr  #b8b8b8
#\def aquasep     #8b8b8b
#\def aquascrbg   #ebebeb
#\def aquascrbox  #b8b8b8
#\def aquascrbar  #7abaff
#\def aquacrsr    #808080

# gradient for "apple", pulled from Stuart Clark (Decipher)'s gradient patch
\def grapbot    #E7EFFD
\def grapmid    #B5CED7
\def graptop    #B5CDE7
\def grapbar    #d5dbfb
\def red        #ff0000
\def blue       #0088ff

#TTK appearance only gives me 10 /defs - feh!

  header: bg => <#c5c5c5, #acacac, #969696 with #acacac @:1,0,50%,1>,
	  fg => metalfg, line => #666666 -1, accent => #d6d6d6
	  shadow => #333, shine => #ddd
   music: bar => <vert graptop to grapmid to grapbot with #d5dbfb @:1,1,5,1>
 battery: border => metalfg, bg => grapmid, fill.normal => metalfg +1, fill.low => metalwid +1, fill.charge => metalfg +1,
	  bg.low => red, bg.charging => blue, chargingbolt => metalfg
    lock: border => metalwid, fill => metalwid
 loadavg: bg => metalbg, fg => metalwid, spike => metalfg

  window: bg => aquabg, fg => aquafg, border => aquawinbdr -3
  dialog: bg => aquabg, fg => aquafg, line => #8b8b8b,
          title.fg => aquafg,
          button.bg => <vert #f9f9f9 to #e3e3e3 to #ffffff> *6, button.fg => aquafg, button.border => aquabtnbdr *6,
	  button.sel.bg => <vert #c7d0ea to #61a2e4 to #8de4ff> *6, button.sel.fg => aquafg, button.sel.border => aquadbtnbdr *6, button.sel.inner => aquadbtn +1 *5
   error: bg => aquabg, fg => aquafg, line => #8b8b8b,
          title.fg => aquafg,
          button.bg => <vert #f9f9f9 to #e3e3e3 to #ffffff> *6, button.fg => aquafg, button.border => aquabtnbdr *6,
	  button.sel.bg => <vert #c7d0ea to #61a2e4 to #8de4ff> *6, button.sel.fg => aquafg, button.sel.border => aquadbtnbdr *6, button.sel.inner => aquadbtn +1 *5
  scroll: box => #b8b8b8, bg => <horiz #b8b8b8 to #efefef> +1,
          bar => @familiar-(scroll.bar).png
   input: bg => aquabg, fg => aquafg, selbg => aquadbtn, selfg => aquafg, border => aquawinbdr, cursor => #808080

    menu: bg => #e7edf6, fg => #000000, choice => #000000, icon => #000000,
          hdrbg => <#859fbe, #6c87ab, #3a5887 with #52719a @:50%,0,1,0>, hdrfg => #ffffff,
          selbg => <vert #5999e5 to #1f5ccf>, selfg => #ffffff, selchoice => #ffffff,
          icon0 => #3b79da, icon1 => #28503c, icon2 => #50a078, icon3 => #ffffff
  slider: border => #282828, bg => #e8ebd2, full => #acb098
textarea: bg => #ffffff, fg => #000000

# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => #e6e6e6,
	default.fg => aquafg,
	default.border => #e6e6e6,
	selected.bg => #9e9e9e,
	selected.fg => aquafg,
	selected.border => #e6e6e6,
	special.bg => #5ea3ed,
	special.fg => aquafg,
	special.border => #5ea3ed

button:
	default.bg => <vert #f9f9f9 to #e3e3e3 to #ffffff> *5,
	default.fg => aquafg,
	default.border => aquabtnbdr *6,
	selected.bg => <vert #c7d0ea to #61a2e4 to #8de4ff> *5,
	selected.fg => aquafg,
	selected.border => aquadbtnbdr *6

