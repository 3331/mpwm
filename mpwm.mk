# mpwm version
VERSION = 1

# Compiler/linker
CC              := /usr/bin/clang-14
TARGET          := mpwm
PREFIX          := /usr/local

# Files
SRCS            = $(wildcard src/*.c)
HDRS            = $(wildcard src/*.h)

# Auto files
BUILD_TYPE      := Release
ROOT_OBJDIR     := .build
ROOT_DEPDIR     := .deps
OBJDIR          := ${ROOT_OBJDIR}/${BUILD_TYPE}
DEPDIR          := ${ROOT_DEPDIR}/${BUILD_TYPE}

OBJS            = $(SRCS:%.c=${OBJDIR}/%.o)
DEPS            = $(SRCS:%.c=${DEPDIR}/%.d)

OBJDIRS         = $(shell dirname ${OBJS} | sort -u)
DEPDIRS         = $(shell dirname ${DEPS} | sort -u)

# Verbose stuff
COL_GREEN       = \033[0;32m
COL_YELLOW      = \033[1;33m
COL_BLUE        = \033[0;34m
COL_DEF         = \033[0m

MSG_INFO        = [${COL_BLUE}*${COL_DEF}]
MSG_PREV_LINE   = \033[F

# Flags
INCLUDES        = -I/usr/include/freetype2
DEPFLAGS        = -MT $@ -MMD -MP -MF ${DEPDIR}/$*.d
CFLAGS          := -O3 -march=native -pedantic -Wall -Wextra -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_POSIX_C_SOURCE=2 -DVERSION=\"${VERSION}\" -DXINERAMA ${INCLUDES}
CFLAGS_WARNINGS := -Wno-unused-function -Wno-pointer-to-int-cast -Wno-unused-variable -Wno-division-by-zero
LDFLAGS         := -lX11 -lXi -lXinerama -lfontconfig -lXft

.PHONY: all clean

all: ${TARGET}

debug: CFLAGS += -DDEBUG -g
debug: ${TARGET}

# Build target
${TARGET}: ${OBJS}
	@echo "[${COL_YELLOW}LD${COL_DEF}] ${OBJS}"
	@${CC} -o $@ $^ ${LDFLAGS}
	@echo "${MSG_PREV_LINE}[${COL_GREEN}LD${COL_DEF}] ${OBJS} => $@"

# Generic source -> object target
${OBJDIR}/%.o: %.c | ${OBJDIRS} ${DEPDIRS}
	@echo "[${COL_YELLOW}CC${COL_DEF}] $<"
	@${CC} ${DEPFLAGS} -c ${CFLAGS} ${CFLAGS_WARNINGS} -o $@ $<
	@echo "${MSG_PREV_LINE}[${COL_GREEN}CC${COL_DEF}] $< => $@"

install: all
	sudo mkdir -p "${DESTDIR}${PREFIX}/bin"
	sudo cp -fa "${TARGET}" "${DESTDIR}${PREFIX}/bin"
	sudo chmod 755 "${DESTDIR}${PREFIX}/bin/${TARGET}"

uninstall:
	rm -f "${DESTDIR}${PREFIX}/bin/${TARGET}"

clean:
	rm -rf "${ROOT_OBJDIR}" "${ROOT_DEPDIR}"
	rm -f "${TARGET}"

${OBJDIRS} ${DEPDIRS}:
	@echo "${MSG_INFO} mkdir -p $@"
	@mkdir -p $@

DEPFILES = $(SRCS:%.c=${DEPDIR}/%.d)

${DEPFILES}:

include $(wildcard ${DEPFILES})
