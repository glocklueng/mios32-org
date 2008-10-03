# $Id$
# defines rules building the programming model

# extend include path
C_INCLUDE += 	-I $(MIOS32_PATH)/programming_models/traditional \
		-I $(FREE_RTOS)/Source/include \
		-I $(FREE_RTOS)/Source/portable/GCC/ARM_CM3

# add modules to thumb sources
THUMB_SOURCE += \
		$(MIOS32_PATH)/programming_models/traditional/crt0_STM32x.c \
		$(MIOS32_PATH)/programming_models/traditional/main.c \
		$(MIOS32_PATH)/programming_models/traditional/printf-stdarg.c \
		$(FREE_RTOS)/Source/tasks.c \
		$(FREE_RTOS)/Source/list.c \
		$(FREE_RTOS)/Source/queue.c \
		$(FREE_RTOS)/Source/portable/GCC/ARM_CM3/port.c \
		$(FREE_RTOS)/Source/portable/MemMang/heap_2.c 

# add MIOS32 sources
include $(MIOS32_PATH)/modules/mios32/mios32.mk

# directories and files that should be part of the distribution (release) package
DIST += $(MIOS32_PATH)/programming_models/traditional
