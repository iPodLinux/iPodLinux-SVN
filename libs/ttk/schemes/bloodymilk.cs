\name Bloody Milk
\def black   #000000
\def unwhite #ebebeb
\def gray    #a0a0a0
\def dkgray  #505050

  header: bg => <#d22 to #c22 to #d33 with #f99 @:1,1,59%,1> *1,
	  fg => unwhite,
	  line => #a22 -1,
	  accent => gray
	  shadow => dkgray,
	  shine => gray

  music:  bar.bg => <vert #555 to #555 to #666 with #bbb @:1,1,59%,1> *1 -2,
	  bar => <vert #d22 to #c22 to #d33 with #f99 @:1,1,59%,1> *1 -2,
	  bar.border =>

 battery: border => #86898f,
	  bg => #f7f7f7,
	  bg.low => <vert #fdcece to #ec6868>,
	  bg.charging => <vert #ceedfc to #52b9ef>,
	  fill.normal => <horiz #d22 to #c22 to #d33 with #f99 @:1,57%,1,1>,
	  fill.low => dkgray +1,
	  fill.charge => <vert #69777f to black> +1,
          chargingbolt => #ff0

    lock: border => black,
	  fill => black

 loadavg: bg => gray,
	  fg => dkgray,
	  spike => unwhite

  window: bg => unwhite,
	  fg => black,
	  border => gray -3

  dialog: bg => unwhite,
	  fg => black,
	  line => #9fa2a7,
	  title.fg => black,	
	  button.bg => <#e0e0e0, #e0e0e0, #d6d6d6 with #f2f2f2 @:1,0,50%,0>,
	  button.fg => black,
	  button.border => #9fa2a7,
	  button.sel.bg => <#c0c0c0, #c0c0c0, #b6b6b6 with #d2d2d2 @:1,0,50%,0>,
	  button.sel.fg => black,
	  button.sel.border => #86898f,
	  button.sel.inner => #86898f

   error: bg => unwhite,
	  fg => black,
	  line => #9fa2a7,
	  title.fg => black,
	  button.bg => <#e0e0e0, #e0e0e0, #d6d6d6 with #f2f2f2 @:1,0,50%,0>,
	  button.fg => black,
	  button.border => #9fa2a7,
	  button.sel.bg => <#c0c0c0, #c0c0c0, #b6b6b6 with #d2d2d2 @:1,0,50%,0>,
	  button.sel.fg => black,
	  button.sel.border => #86898f,
	  button.sel.inner => #86898f

  scroll: box => #a22,
	  bg => #f7f7f7,
	  bar => <horiz #d22 to #c22 to #d33 with #f99 @:1,59%,1,1>

   input: bg => unwhite,
	  fg => black,
	  selbg => dkgray,
	  selfg => unwhite,
	  border => gray,
	  cursor => dkgray

    menu: bg => unwhite,
	  fg => #000,
	  choice => unwhite,
	  icon => unwhite,
	  hdrfg => black,
	  hdrbg => <#e0e0e0, #e0e0e0, #d6d6d6 with #f2f2f2 @:1,0,50%,0>,
	  selbg => <vert #d22 to #c22 to #d33 with #f99 @:1,1,59%,1> *1,
	  selfg => unwhite,
	  selchoice => unwhite,
	  icon0 => unwhite -3,
	  icon1 => unwhite -2,
	  icon2 => unwhite,
	  icon3 => unwhite +1

  slider: border => <vert #555 to #555 to #666 with #bbb @:1,1,59%,1> *1 -2,
	  bg => #f7f7f7 -2,
	  full => <vert #d22 to #c22 to #d33 with #f99 @:1,1,59%,1> *1 -3


textarea: bg => unwhite,
	  fg => black

     box: default.bg => <#e0e0e0, #e0e0e0, #d6d6d6 with #f2f2f2 @:1,0,56%,0>,
	  default.fg => black,
	  default.border => #9fa2a7,
	  selected.bg => <#c0c0c0, #c0c0c0, #b6b6b6 with #d2d2d2 @:1,0,56%,0>,
	  selected.fg => black,
	  selected.border => #86898f,
	  special.bg =>  <#d0d0d0, #d0d0d0, #c6c6c6 with #e2e2e2 @:1,0,56%,0>,
	  special.fg => black,
	  special.border => #333

  button: default.bg => <#e0e0e0, #e0e0e0, #d6d6d6 with #f2f2f2 @:1,0,68%,0>,
	  default.fg => black,
	  default.border => #9fa2a7,
	  selected.bg => <#c0c0c0, #c0c0c0, #b6b6b6 with #d2d2d2 @:1,0,68%,0>,
	  selected.fg => black,
	  selected.border => #86898f
