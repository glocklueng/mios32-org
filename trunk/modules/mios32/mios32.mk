# $Id$
# defines additional rules for MIOS32

# enhance include path
C_INCLUDE +=	-I $(MIOS32_PATH)/modules/mios32 \
		-I $(DRIVER_LIB)/inc


# add modules to thumb sources
# TODO: provide makefile option to add code to ARM sources
# TODO: select driver library depending on PROCESSOR/FAMILY variable
THUMB_SOURCE += \
	$(MIOS32_PATH)/modules/mios32/mios32_sys.c \
	$(MIOS32_PATH)/modules/mios32/mios32_srio.c \
	$(MIOS32_PATH)/modules/mios32/mios32_din.c \
	$(MIOS32_PATH)/modules/mios32/mios32_dout.c \
	$(MIOS32_PATH)/modules/mios32/mios32_lcd.c \
	$(MIOS32_PATH)/modules/mios32/mios32_midi.c \
	$(MIOS32_PATH)/modules/mios32/mios32_usb.c \
	$(MIOS32_PATH)/modules/mios32/mios32_usb_desc.c \
	$(MIOS32_PATH)/modules/mios32/mios32_board.c \
	$(DRIVER_LIB)/src/stm32f10x_gpio.c \
	$(DRIVER_LIB)/src/stm32f10x_flash.c \
	$(DRIVER_LIB)/src/stm32f10x_spi.c \
	$(DRIVER_LIB)/src/stm32f10x_rcc.c \
	$(DRIVER_LIB)/src/stm32f10x_systick.c \
	$(DRIVER_LIB)/src/stm32f10x_nvic.c \
	$(DRIVER_LIB)/src/usb_core.c \
	$(DRIVER_LIB)/src/usb_init.c \
	$(DRIVER_LIB)/src/usb_int.c \
	$(DRIVER_LIB)/src/usb_mem.c \
	$(DRIVER_LIB)/src/usb_regs.c

# MEMO: the gcc linker is clever enough to exclude functions from the final memory image
# if they are not references from the main routine - accordingly we can savely include
# the USB drivers without the danger that this increases the project size of applications,
# which don't use the USB peripheral at all :-)


# directories and files that should be part of the distribution (release) package
DIST += $(MIOS32_PATH)/modules/mios32
