#(setq world "world" hi "Hello" comma "," exclamation "!")
#(princ (list hi comma world exclamation))

# startup is where we put things that get called before the window is set up
(defun OnStartup () (list
			(princ "Initialize" )
			(terpri)
			#(setq FullScreen 1)
			(setq TimerMSec 1000)
			(setq HeaderText "Example")
			(setq LithpVersion  "0.7")
		    )
)

# preflight gets called before the main loop gets run, but after the window is set up
(defun OnPreFlight () (list
			(princ "PreFlight" )
			(terpri)
			(setq xpos (/ Width 2)  ypos (/ Height 2))
		    )
)

# if a timer is defined, this gets called periodically
(defun OnTimer () (list 
			(setq DirtyScreen 1)
		    )
)

# user input callbacks
(defun OnLeft   () (list 
			(setq xpos (RangeWrap (1+ xpos) 0 Width) )
			(setq DirtyScreen 1)
		    )
)

(defun OnRight  () (list 
			(setq ypos (RangeWrap (1+ ypos) 0 Height) ) 
			(setq DirtyScreen 1)
		    )
)

(defun OnAction () (list
			(setq xpos (/ Width 2)  ypos (/ Height 2))
			(setq DirtyScreen 1)
		    )
)

#(defun OnPlay   () (list (princ "play")(terpri) ))
#(defun OnNext   () (list (princ "next")(terpri) ))
#(defun OnPrev   () (list (princ "prev")(terpri) ))

# when the screen is dirty, it gets redrawn using the following
(defun OnDraw   () (list 
			(DrawPen (Rand 255) (Rand 255) (Rand 255) (RandomOf BLACK WHITE) )
			(DrawClear)
			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawPen2 (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawHGradient (Rand Width) (Rand Height) (Rand Width) (Rand Height))
			(DrawVGradient (Rand Width) (Rand Height) (Rand Width) (Rand Height))

			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawEllipse (Rand Width) (Rand Height) (Rand Width) (Rand Height))
			(DrawAAEllipse (Rand Width) (Rand Height) (Rand Width) (Rand Height))
			(DrawFillEllipse (Rand Width) (Rand Height) (Rand Width) (Rand Height))
			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawFillRect (Rand Width) (Rand Height) (Rand Width) (Rand Height))
			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawLine (Rand Width) (Rand Height) (Rand Width) (Rand Height))
			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawAALine (Rand Width) (Rand Height) (Rand Width) (Rand Height))
			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawRect (Rand Width) (Rand Height) (Rand Width) (Rand Height))
			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawPixel (Rand Width) (Rand Height))

			(setq x (+ 4 (Rand 12)))
			(DrawVectorText (/ Width 2) (/ (* 3 Height) 4) x (* x 2) 
					(RandomOf Montana Burrito Cheese Bleu Llama) )

			(DrawVectorTextCentered (/ Width 2) (/ Height 4) x (* x 2) 
					(RandomOf Montana Burrito Cheese Bleu Llama) )

			# draw the 'cursor'
			(DrawPen 255 255 0 GRAY)
			(DrawFillRect xpos ypos (+ xpos 20) (+ ypos 20))
			(DrawPen 0 255 255 WHITE)
			(DrawFillRect (+ xpos 5) (+ ypos 5) (+ xpos 15) (+ ypos 15))
			(DrawPen 0 0 0 BLACK)
			(DrawRect (+ xpos 1) (+ ypos 1) (+ xpos 19) (+ ypos 19))

			# draw info text
			(DrawVectorText 1 1 5 9 Frames)
			(DrawVectorText 1 12 5 9 Ticks)

			#(DrawText (/ Width 2) (/ Height 2) Hello )
			#(princ "=== DRAW ===" Frames Ticks (Rand Width) ) 
			#(terpri)
		    )
)
