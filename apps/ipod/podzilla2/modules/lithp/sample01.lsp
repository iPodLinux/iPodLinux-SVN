#(setq world "world" hi "Hello" comma "," exclamation "!")

(require 'pz2:0.8)

(setq pz2:Startup myStartup)
(setq pz2:PreFlight myPreFlight)
(setq pz2:Timer myTimer)
(setq pz2:Left myLeft)
(setq pz2:Right myRight)
(setq pz2:Action myAction)
(setq pz2:Draw myDraw)


# startup is where we put things that get called before the window is set up
(defun myStartup () (list
			(princ "Initialize" )
			(terpri)
			#(setq pz2:FullScreen 1)
			(setq pz2:TimerMSec 1000)
			(setq pz2:HeaderText "Example")
		    )
)

# preflight gets called before the main loop gets run, but after the window is set up
(defun myPreFlight () (list
			(princ "PreFlight" )
			(terpri)
			(setq xpos (/ pz2:Width 2)  ypos (/ pz2:Height 2))
		    )
)

# if a timer is defined, this gets called periodically
(defun myTimer () (list 
			(setq pz2:DirtyScreen 1)
		    )
)

# user input callbacks
(defun myLeft   () (list 
			(setq xpos (RangeWrap (1+ xpos) 0 pz2:Width) )
			(setq pz2:DirtyScreen 1)
		    )
)

(defun myRight  () (list 
			(setq ypos (RangeWrap (1+ ypos) 0 pz2:Height) ) 
			(setq pz2:DirtyScreen 1)
		    )
)

(defun myAction () (list
			(setq xpos (/ pz2:Width 2)  ypos (/ pz2:Height 2))
			(setq pz2:DirtyScreen 1)
		    )
)

#(defun myPlay   () (list (princ "play")(terpri) ))
#(defun myNext   () (list (princ "next")(terpri) ))
#(defun myPrev   () (list (princ "prev")(terpri) ))

# when the screen is dirty, it gets redrawn using the following
(defun myDraw   () (list 
			(DrawPen (Rand 255) (Rand 255) (Rand 255) (RandomOf BLACK WHITE) )
			(DrawClear)
			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawPen2 (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawHGradient (Rand pz2:Width) (Rand pz2:Height) (Rand pz2:Width) (Rand pz2:Height))
			(DrawVGradient (Rand pz2:Width) (Rand pz2:Height) (Rand pz2:Width) (Rand pz2:Height))

			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawEllipse (Rand pz2:Width) (Rand pz2:Height) (Rand pz2:Width) (Rand pz2:Height))
			(DrawAAEllipse (Rand pz2:Width) (Rand pz2:Height) (Rand pz2:Width) (Rand pz2:Height))
			(DrawFillEllipse (Rand pz2:Width) (Rand pz2:Height) (Rand pz2:Width) (Rand pz2:Height))
			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawFillRect (Rand pz2:Width) (Rand pz2:Height) (Rand pz2:Width) (Rand pz2:Height))
			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawLine (Rand pz2:Width) (Rand pz2:Height) (Rand pz2:Width) (Rand pz2:Height))
			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawAALine (Rand pz2:Width) (Rand pz2:Height) (Rand pz2:Width) (Rand pz2:Height))
			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawRect (Rand pz2:Width) (Rand pz2:Height) (Rand pz2:Width) (Rand pz2:Height))
			(DrawPen (Rand 255) (Rand 255) (Rand 255) BLACK)
			(DrawPixel (Rand pz2:Width) (Rand pz2:Height))

			(setq x (+ 4 (Rand 12)))
			(DrawVectorText (/ pz2:Width 2) (/ (* 3 pz2:Height) 4) x (* x 2) 
					(RandomOf Montana Burrito Cheese Bleu Llama) )

			(DrawVectorTextCentered (/ pz2:Width 2) (/ pz2:Height 4) x (* x 2) 
					(RandomOf Montana Burrito Cheese Bleu Llama) )

			# draw the 'cursor'
			(DrawPen 255 255 0 GRAY)
			(DrawFillRect xpos ypos (+ xpos 20) (+ ypos 20))
			(DrawPen 0 255 255 WHITE)
			(DrawFillRect (+ xpos 5) (+ ypos 5) (+ xpos 15) (+ ypos 15))
			(DrawPen 0 0 0 BLACK)
			(DrawRect (+ xpos 1) (+ ypos 1) (+ xpos 19) (+ ypos 19))

			# draw info text
			(DrawVectorText 1 1 5 9 pz2:Frames)
			(DrawVectorText 1 12 5 9 pz2:Ticks)

			#(DrawText (/ pz2:Width 2) (/ pz2:Height 2) Hello )
			#(princ "=== DRAW ===" pz2:Frames pz2:Ticks (Rand pz2:Width) ) 
			#(terpri)
		    )
)
