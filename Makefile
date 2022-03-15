MPWM = mpwm.mk

all: release

release:
	@${MAKE} -f ${MPWM} all BUILD_TYPE=Release

debug:
	@${MAKE} -f ${MPWM} debug BUILD_TYPE=Debug

debug-install:
	@${MAKE} -f ${MPWM} debug install BUILD_TYPE=Debug

install:
	@${MAKE} -f ${MPWM} install

clean:
	@${MAKE} -f ${MPWM} clean