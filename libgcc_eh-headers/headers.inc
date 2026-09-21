# vim:set ft=make:

include $(L4DIR)/mk/Makeconf

# Headers that are installed by this package. The architecture specific
# unwind.h may include auxiliary headers from the libgcc sources. They have to
# be provided too in Clang builds because the libgcc headers are not on the
# include path there.
#
# This does not apply when the libgcc of the toolchain is used. There, unwind.h
# is taken from the toolchain and the headers it includes are found in the
# toolchain's include directory.
ifeq ($(BID_COMPILER_TYPE),clang)
ifeq ($(CONFIG_COMPILER_RT_USE_TOOLCHAIN_LIBGCC),)
UNWIND_AUX_HEADERS_arm = unwind-arm-common.h
endif
endif

UNWIND_HEADERS = unwind.h $(UNWIND_AUX_HEADERS_$(BUILD_ARCH))
