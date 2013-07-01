# debug build?
DEBUG  := 1

# static library?
STATIC := 1

# static binary? (not recommended)
STATIC_BIN := 0

# install prefix
PREFIX := /usr

# library name
TARGET := pndman

# CLI frontend name
CLI_BIN := milky

# mingw compilation
MINGW := 0

# x86 binary
X86 := 0

# install binary
INSTALL := install

# strip binary
STRIP := strip

# depencies for pndman
LIB_LIBS := -lexpat `pkg-config --libs libcurl jansson`

# git info
VERSION = `git rev-parse HEAD`
COMMIT  = `git log --format=%B -n 1 HEAD | head -n1`

ifeq (${DEBUG},1)
   CFLAGS := -std=gnu99 -D_GNU_SOURCE -Wall -O0 -g -Wno-switch -Wno-parentheses
else
   CFLAGS := -std=gnu99 -D_GNU_SOURCE -Wall -O2 -s -fomit-frame-pointer -Wno-switch -Wno-parentheses
endif

ifeq (${MINGW},1)
   X86 = 1
   CFLAGS += -DCURL_STATICLIB
   LIB_LIBS += -static -static-libgcc
   LIB_LIBS += -lssl -lcrypto -lz -lbz2 -lidn -lws2_32 -lmingw32 -lkernel32 -lgdi32
   EXT=.exe
else
   ifeq (${STATIC_BIN},0)
   	LIB_LIBS += `pkg-config --libs openssl gnutls` -lz -lbz2 -lpthread -lgcrypt -lgpg-error
     else
   	LIB_LIBS += -static `pkg-config --libs openssl libssh2` -lz -lbz2 -lpthread -ldl -lrt
   endif
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
   CFLAGS += -fPIC
endif

#all: ${LIB_TARGET} test
all: ${LIB_TARGET} ${CLI_BIN} test

version:
	@echo lib/version.h
	@echo "#ifndef VERSION_H" >  lib/version.h
	@echo "#define VERSION_H" >> lib/version.h
	@echo "#define VERSION \"${VERSION}\"" >> lib/version.h
	@echo "#define COMMIT  \"${COMMIT}\""  >> lib/version.h
	@echo "#endif /* VERSION_H */" >> lib/version.h

${LIB_OBJ}: version
${LIB_TARGET}: ${LIB_OBJ}
	@echo "Linking"
ifeq (${STATIC},1)
	@${AR} rcs ${LIB_TARGET} ${LIB_OBJ}
else
	@${CC} -s -shared -Wl,-soname,${LIB_TARGET} -o ${LIB_TARGET} ${LIB_OBJ} -lc
endif

${CLI_BIN}: ${LIB_TARGET}
	@echo "Compiling ${CLI_BIN}"
	@${CC} -Iinclude ${CFLAGS} helper/${CLI_BIN}.c -lm -L. -l$(basename ${TARGET}) ${LIB_LIBS} -o bin/${CLI_BIN}${EXT}

test: ${LIB_TARGET}
	@${MAKE} -C test CFLAGS="${CFLAGS}" LIBS="-L.. -l$(basename ${TARGET}) ${LIB_LIBS}"

clean:
	@${MAKE} -C test clean
	@echo "Cleaning"
	rm -f lib/version.h
	rm -f lib$(addsuffix .a, $(basename ${TARGET}))
	rm -f lib$(addsuffix .so, $(basename ${TARGET}))
	rm -f bin/${CLI_BIN}
	rm -f bin/${CLI_BIN}.exe
	@echo "Cleaning objects.."
	rm -f ${LIB_OBJ}

# NOTE: Install and uninstall rules are only for CLI helper
# 	Library can be found on root directory, and include files in include directory.
install:
	@echo "Installing ${CLI_BIN}"
	${INSTALL} -m 755 bin/${CLI_BIN} ${PREFIX}/bin
	${STRIP} ${PREFIX}/bin/${CLI_BIN}

uninstall:
	@echo "Uninstalling ${CLI_BIN}"
	rm -f ${PREFIX}/bin/${CLI_BIN}
