LIBS  :=
MINGW :=
EXT    =
OUT   := ../bin
TESTS = $(wildcard *.c)

ifeq (${MINGW},1)
   EXT=.exe
endif

all : ${TESTS}
	for c in ${TESTS}; do \
	   ${CC} -Wno-unused-function -I../include ${CFLAGS} $${c} ${LIBS} -o ${OUT}/test/$${c%.c}${EXT}; \
	done

clean:
	@echo "Cleaning"
	for c in ${TESTS}; do \
	   rm -f ${OUT}/test/$${c%.c}; \
 	   rm -f ${OUT}/test/$${c%.c}.exe; \
	done
