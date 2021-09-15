MPWM = mpwm.mk

all: release

release:
	@${MAKE} -f ${MPWM} all BUILD_TYPE=Release

debug:
	@${MAKE} -f ${MPWM} debug BUILD_TYPE=Debug

clean:
	@${MAKE} -f ${MPWM} clean