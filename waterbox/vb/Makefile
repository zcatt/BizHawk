CXXFLAGS := -I. \
	-Wall -Werror=int-to-pointer-cast \
	-std=c++0x -fomit-frame-pointer -fno-exceptions -fno-rtti \
	-DLSB_FIRST

TARGET = vb.wbx

SRCS = $(shell find $(ROOT_DIR) -type f -name '*.cpp')

include ../common.mak
