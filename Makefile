# debug build?
DEBUG  := 1

# use colors?
COLOR  := 1

# static library?
STATIC := 1

# install prefix
PREFIX := /usr

# library name
TARGET := pndman

# mingw compilation
MINGW := 0

# x86 binary
X86 := 0

# depencies for pndman
LIB_LIBS := -lexpat

# git info
VERSION = `git rev-parse HEAD`
COMMIT  = `git log --format=%B -n 1 HEAD | head -n1`

ifeq (${DEBUG},1)
   CFLAGS := -std=c99 -D_GNU_SOURCE -Wall -O0 -g -Wno-switch -Wno-parentheses
else
   CFLAGS := -std=c99 -D_GNU_SOURCE -Wall -O2 -s -fomit-frame-pointer -Wno-switch -Wno-parentheses
endif

ifeq (${MINGW},1)
   X86 = 1
   LIB_LIBS += -static-libgcc -mwindows
   LIB_LIBS += -lmingw32 -lkernel32
endif

ifeq (${X86},1)
   CFLAGS += -m32
endif

LIB_INC	= -Ilib
LIB_SRC	= $(wildcard lib/*.c)
LIB_OBJ	= ${LIB_SRC:.c=.o}
ifeq (${STATIC},1)
   LIB_TARGET = lib$(addsuffix .a, $(basename ${TARGET}))
else
   LIB_TARGET = lib$(addsuffix .so, $(basename ${TARGET}))
endif

ifeq (${COLOR},1)
   CFLAGS += -DCOLOR=1
endif

all: ${LIB_TARGET} test

version:
	@echo lib/version.h
	@echo "#ifndef VERSION_H" >  lib/version.h
	@echo "#define VERSION_H" >> lib/version.h
	@echo "#define VERSION \"${VERSION}\"" >> lib/version.h
	@echo "#define COMMIT  \"${COMMIT}\""  >> lib/version.h
	@echo "#endif /* VERSION_H */" >> lib/version.h

milkyhelper: ${LIB_TARGET}
	@echo "Compiling milkyhelper"
	@echo "Milkyhelper not yet implented"

test: ${LIB_TARGET}
	@${MAKE} -C test CFLAGS="${CFLAGS}" LIBS="-L.. -l$(basename ${TARGET}) ${LIB_LIBS}"

%.o : %.c
	@${CC} -fPIC ${CFLAGS} ${LIB_INC} ${LIB_LIBS} -c $^ -o $@

${LIB_TARGET}: version ${LIB_OBJ}
	@echo "Linking"
ifeq (${STATIC},1)
	@${AR} rcs ${LIB_TARGET} ${LIB_OBJ}
else
	@${CC} -s -shared -Wl,-soname,${LIB_TARGET} -o ${LIB_TARGET} ${LIB_OBJ} -lc
endif

clean:
	@${MAKE} -C test clean
	@echo "Cleaning"
	rm -f lib/version.h
	rm -f lib$(addsuffix .a, $(basename ${TARGET}))
	rm -f lib$(addsuffix .so, $(basename ${TARGET}))
	@echo "Cleaning objects.."
	@rm -f ${LIB_OBJ}

install:
	@echo "Install not yet implented"

uninstall:
	@echo "Uninstall not yet implented"
