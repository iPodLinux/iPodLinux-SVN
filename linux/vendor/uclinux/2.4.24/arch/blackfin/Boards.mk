#   Changes made by Akbar Hussain & Tony Kou April 04, 2001


# Put definitions for platforms and boards in here.

ifdef CONFIG_BLACKFIN
  ifdef CONFIG_EAGLE
    PLATFORM := 535
    BOARD := ADI
  endif
  ifdef CONFIG_HAWK
    PLATFORM := 535
    BOARD := ADI
  endif
  ifdef CONFIG_PUB
    PLATFORM := 535
    BOARD := ADI
  endif
  ifdef CONFIG_BLACKFIN_EZKIT
    PLATFORM := 533
    BOARD := ADI
  endif
endif

export BOARD
export PLATFORM
