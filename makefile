# Hey Emacs, this is a -*- makefile -*-
#----------------------------------------------------------------------------
#
# Makefile for ChipWhisperer SimpleSerial-AES Program
#
#----------------------------------------------------------------------------
# On command line:
#
# make all = Make software.
#
# make clean = Clean out built project files.
#
# make coff = Convert ELF to AVR COFF.
#
# make extcoff = Convert ELF to AVR Extended COFF.
#
# make program = Download the hex file to the device, using avrdude.
#                Please customize the avrdude settings below first!
#
# make debug = Start either simulavr or avarice as specified for debugging,
#              with avr-gdb or avr-insight as the front end for debugging.
#
# make filename.s = Just compile filename.c into the assembler code only.
#
# make filename.i = Create a preprocessed source file for use in submitting
#                   bug reports to the GCC project.
#
# To rebuild project do "make clean" then "make all".
#---------------------------------------------------------------------------

# Replace this with the directory name of your implementation
TARGET_DIR = dummy
TARGET_PATH = targets/$(TARGET_DIR)

# Target file name (without extension).
# This is the base name of the compiled .hex file.
TARGET = $(TARGET_DIR)

# Define whether to do encryption, decryption and/or hashing
# 0: Don't include
# 1: Include
DO_ENCRYPT = 1
DO_DECRYPT = 1
DO_HASH = 0

CRYPTO_TARGET = NONE

CDEFS += -DDO_ENCRYPT=$(DO_ENCRYPT) \
		 -DDO_DECRYPT=$(DO_DECRYPT) \
		 -DDO_HASH=$(DO_HASH)

# Import all header files from target path
EXTRAINCDIRS += $(TARGET_PATH)

# Load in all C files in the target dir
SRC += $(wildcard $(TARGET_PATH)/*.c)
# Custom selection of C files
# SRC += $(TARGET_PATH)/encrypt.c $(TARGET_PATH)/...

# Load in the wrapper C files
SRC += buffer_control.c lwc_wrapper.c

# List additional C source files here.
# Header files (.h) are automatically pulled in.
# SRC +=

SS_VER=SS_VER_2_0

# Replace the default target with this target
# We need to do some housework before and after.
replacement_all: ready_objdir all move_resultfiles

ready_objdir:
	mkdir -p objdir/$(TARGET_PATH)

move_resultfiles:
	mv $(TARGET-PLAT).* $(TARGET_PATH)

# -----------------------------------------------------------------------------
#Add simpleserial project to build
include simpleserial/Makefile.simpleserial

FIRMWAREPATH = .
include $(FIRMWAREPATH)/Makefile.inc