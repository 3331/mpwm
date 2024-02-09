# mpwm version
VERSION = 1.2

# Compiler/linker
CC              := /usr/bin/cc
TARGET          := mpwm
PREFIX          := /usr/local

# Files
SRCS            = $(wildcard src/*.c)

# Auto files
BUILD_TYPE      := Unknown
ROOT_OBJDIR     := .build
ROOT_DEPDIR     := .deps
OBJDIR          := ${ROOT_OBJDIR}/${BUILD_TYPE}
DEPDIR          := ${ROOT_DEPDIR}/${BUILD_TYPE}

OBJS            = $(SRCS:%.c=${OBJDIR}/%.o)
DEPS            = $(SRCS:%.c=${DEPDIR}/%.d)

OBJDIRS         = $(shell dirname ${OBJS} | sort -u)
DEPDIRS         = $(shell dirname ${DEPS} | sort -u)

# Flags
INCLUDES        = -I/usr/include/freetype2
DEPFLAGS        = -MT $@ -MMD -MP -MF ${DEPDIR}/$*.d
CFLAGS          := -O3 -march=native -pedantic -Wall -Wextra -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_POSIX_C_SOURCE=2 -DVERSION=\"${VERSION}\" -DXINERAMA ${INCLUDES}
LDFLAGS         := -flto=full -lX11 -lXi -lXinerama -lfontconfig -lXft -lXrender

.PHONY: all clean

all: ${TARGET}

debug: CFLAGS += -DDEBUG -g
debug: ${TARGET}

# Build target
${TARGET}: ${OBJS}
	${CC} -o $@ $^ ${LDFLAGS}

# Generic source -> object target
${OBJDIR}/%.o: %.c | ${OBJDIRS} ${DEPDIRS}
	${CC} ${DEPFLAGS} -c ${CFLAGS} -o $@ $<

install: all
	sudo mkdir -p "${DESTDIR}${PREFIX}/bin"
	sudo cp -fa "${TARGET}" "${DESTDIR}${PREFIX}/bin"
	sudo chmod 755 "${DESTDIR}${PREFIX}/bin/${TARGET}"

uninstall:
	sudo rm -f "${DESTDIR}${PREFIX}/bin/${TARGET}"

clean:
	rm -rf "${ROOT_OBJDIR}" "${ROOT_DEPDIR}"
	rm -f "${TARGET}"

${OBJDIRS} ${DEPDIRS}:
	mkdir -p $@

DEPFILES = $(SRCS:%.c=${DEPDIR}/%.d)

${DEPFILES}:

include $(wildcard ${DEPFILES})
