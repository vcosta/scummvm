MODULE := engines/dgds

MODULE_OBJS := \
	decompress.o \
	detection.o \
	dgds.o

# This module can be built as a plugin
ifeq ($(ENABLE_DGDS), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk
