# Makefile to build class 'pdprojectm' for Pure Data.
# Needs Makefile.pdlibbuilder as helper makefile for platform-dependent build
# settings and rules.

# library name
lib.name = pdprojectm

# input source file (class name == source file basename)
class.sources = \
 tex_test.c\
 tex_gradient.cpp

# all extra files to be included in binary distribution of the library
datafiles = pdprojectm-help.pd pdprojectm-meta.pd README.md

cflags+= -I/usr/include/Gem

# include Makefile.pdlibbuilder from submodule directory 'pd-lib-builder'
PDLIBBUILDER_DIR=pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
