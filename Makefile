# Makefile — TinyA2A reference C library
#
# Targets:
#   make           build shared lib (libtinya2a.{so|dylib}) and static lib
#   make pytest    run Python tests against the shared lib
#   make install   install lib + header to PREFIX (default /usr/local)
#   make clean
#
CC      ?= cc
AR      ?= ar
CFLAGS  ?= -O2 -Wall -Wextra -Wpedantic -std=c11 -fPIC -Iinclude
LDFLAGS ?=
PREFIX  ?= /usr/local

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
SHLIB_EXT := dylib
SHLIB_FLAGS := -dynamiclib -install_name @rpath/libtinya2a.dylib
else
SHLIB_EXT := so
SHLIB_FLAGS := -shared -Wl,-soname,libtinya2a.so
endif

SRCS := src/envelope.c src/journal.c src/queue.c src/frame.c
OBJS := $(SRCS:.c=.o)

BUILD_DIR := build
SHLIB := $(BUILD_DIR)/libtinya2a.$(SHLIB_EXT)
STLIB := $(BUILD_DIR)/libtinya2a.a

.PHONY: all clean pytest install dirs

all: dirs $(SHLIB) $(STLIB)

dirs:
	@mkdir -p $(BUILD_DIR)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(SHLIB): $(OBJS)
	$(CC) $(SHLIB_FLAGS) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Built $@"

$(STLIB): $(OBJS)
	$(AR) rcs $@ $(OBJS)
	@echo "Built $@"

pytest: $(SHLIB)
	cd python && python3 -m pytest tests -v

install: all
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include
	install -m 644 $(SHLIB) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 $(STLIB) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 include/tinya2a.h $(DESTDIR)$(PREFIX)/include/
	@echo "Installed to $(DESTDIR)$(PREFIX)"

clean:
	rm -rf $(BUILD_DIR) src/*.o
