\name Black Glass
\def black  #000
\def white  #fff
\def gray   #a0a0a0
\def dkgray #505050

  header: bg => <#222 to #111 to #333 with #555 @:1,0,55%,0>,
	  fg => #777,
	  line => #666 -1,
	  accent => dkgray,
	  shine => gray,
	  shadow => dkgray,
	  accent => dkgray

   music: bar => <vert #333 to black to #222 with #333 @:1,1,6,1>
	  
 battery: border => #666,
	  bg => black,
	  bg.low => #777,
	  bg.charging => #444,
	  fill.normal => gray +1,
	  fill.low => gray +1,
	  fill.charge => black +1,
          chargingbolt => #666

    lock: border => #666,
	  fill => white
	  
 loadavg: bg => black,
	  fg => dkgray,
	  spike => gray
	  
  window: bg => black,
	  fg => #777,
	  border => dkgray -3

  dialog: bg => black,
	  fg => #777,
	  line => #666,
          title.fg => #777,
          button.bg => black,
	  button.fg => #777,
	  button.border => #666,
          button.sel.bg => #333,
	  button.sel.fg => white,
	  button.sel.border => #333,
	  button.sel.inner => #222 +1

   error: bg => black,
	  fg => #777,
	  line => #666,
          title.fg => #700,
          button.bg => black,
	  button.fg => #777,
	  button.border => #666,
          button.sel.bg => #333,
	  button.sel.fg => white,
	  button.sel.border => #333,
	  button.sel.inner => #222 +1

  scroll: box => #222 +3,
	  bg => black +1,
	  bar => #333 +2

   input: bg => black,
	  fg => #999,
	  selbg => gray,
	  selfg => black,
	  border => dkgray,
	  cursor => gray

    menu: bg => black,
	  fg => #999,
	  choice => #aaa,
	  icon => #222,
	  hdrbg => <#222 to #111 to #333 with #555 @:1,0,55%,0> +2 *1,
	  hdrfg => #999,
          selbg => #333,
	  selfg => white,
	  selchoice => black,
          icon0 => #222,
	  icon1 => #222,
	  icon2 => #111,
	  icon3 => black

  slider: border => #333 +3,
	  bg => black -1,
	  full => #333

textarea: bg => black,
	  fg => white

box:
	default.bg => <#222 to #333 to #000>,
	default.fg => #fff,
	default.border => #444,
	selected.bg => <#fff to #ccc to #000>,
	selected.fg => #000,
	selected.border => #fff,
	special.bg => <#005 to #006 to #002>,
	special.fg => #ccf,
	special.border => #008

button:
	default.bg => <#222 to #111 to #333 with #555 @:0,0,55%,0>,
	default.fg => #777,
	default.border => #333,
	selected.bg => <#333 to #222 to #444 with #666 @:0,0,55%,0>,
	selected.fg => white,
	selected.border => #444,
