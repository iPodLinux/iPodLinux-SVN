# Put definitions for platforms and boards in here.

ifdef CONFIG_BOARD_GENERIC
ifdef CONFIG_CPU_H8300H
PLATFORM := h8300h
endif
ifdef CONFIG_CPU_H8S
PLATFORM := h8s
endif
BOARD := generic
endif

ifdef CONFIG_BOARD_AKI3068NET
PLATFORM := h8300h
BOARD := aki3068net
endif

ifdef CONFIG_BOARD_H8MAX
PLATFORM := h8300h
BOARD := h8max
endif

ifdef CONFIG_BOARD_EDOSK2674
PLATFORM := h8s
BOARD := edosk2674
endif

export PLATFORM
export BOARD
