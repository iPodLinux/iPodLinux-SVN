\name Lilac

\def pink #FFCCFF
\def light #CC66FF
\def lightish #CC99FF
\def medium #9933CC
\def darkish #9966CC
\def dark #660099
\def gray #777777
\def white #FFFFFF
\def ltgray #E0E0E0
\def creme #F0F0F0
\def black #000000

  header: bg => <light, medium, lightish with medium @:1,0,50%,0>,
	  fg => black,
	  line => gray,
	  accent => medium,
	  shadow => dark,
	  shine => pink

   music: bar.bg => <vert white to ltgray to white>,
	  bar => <vert light to medium to lightish> +1

 battery: border => black,
	  bg => <vert white to ltgray to white>,
	  bg.low => <vert white to ltgray to white>,
	  bg.charging => <vert white to ltgray to white>,
	  fill.normal => <vert lightish to light>,
	  fill.low => <vert #FF8080 to #FF4040>,
	  fill.charge => <vert #80FF80 to #40FF40>,
	  chargingbolt => dark

    lock: border => dark,
	  fill => darkish

 loadavg: bg => pink,
	  fg => darkish,
	  spike => lightish

  window: bg => creme,
	  fg => black,
	  border => gray -3

  dialog: bg => white,
	  fg => black,
	  line => gray,
	  title.fg => black,	
	  button.bg => <vert white to ltgray to white>,
	  button.fg => black,
	  button.border => darkish,
	  button.sel.bg => <vert light to medium to lightish>,
	  button.sel.fg => black,
	  button.sel.border => darkish,
	  button.sel.inner => medium

  dialog: bg => white,
	  fg => black,
	  line => gray,
	  title.fg => black,	
	  button.bg => <vert white to ltgray to white>,
	  button.fg => black,
	  button.border => darkish,
	  button.sel.bg => <vert light to medium to lightish>,
	  button.sel.fg => black,
	  button.sel.border => darkish,
	  button.sel.inner => medium

  scroll: box => darkish -1,
	  bg => <horiz white to ltgray to white>,
	  bar => <horiz light to medium to lightish> +1

   input: bg => white,
	  fg => black,
	  selbg => dark,
	  selfg => white,
	  border => gray,
	  cursor => dark

    menu: bg => creme,
	  fg => black,
	  hdrbg => <light, medium, lightish with medium @:1,0,50%,0>,
	  hdrfg => white,
	  choice => black,
	  icon => black,
	  selbg => <vert light to lightish>,
	  selfg => black,
	  selchoice => black,
	  icon0 => medium,
	  icon1 => darkish,
	  icon2 => dark,
	  icon3 => black

  slider: border => darkish -2,
	  bg => <vert white to ltgray to white> -2,
	  full => <vert light to medium to lightish> -1

textarea: bg => white,
	  fg => black

     box: default.bg => <vert white to ltgray to white>,
	  default.fg => black,
	  default.border => gray,
	  selected.bg => <vert light to medium to lightish>,
	  selected.fg => black,
	  selected.border => gray,
	  special.bg =>  <vert #FF8080 to #FF4040 to #FF8080>,
	  special.fg => black,
	  special.border => gray

  button: default.bg => <vert white to ltgray to white>,
	  default.fg => black,
	  default.border => gray,
	  selected.bg => <vert light to medium to lightish>,
	  selected.fg => black,
	  selected.border => gray
