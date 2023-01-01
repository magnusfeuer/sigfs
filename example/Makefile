#
# Top-level makefile for sigfs
#

.PHONY: all clean debug install uninstall test examples

HDR=../log.h

#
# Signal publish
#
SIGFS_PUBLISH_SRC=sigfs_publish.cc ../log.cc
SIGFS_PUBLISH_OBJ=${patsubst %.cc, %.o, ${SIGFS_PUBLISH_SRC}}
SIGFS_PUBLISH=sigfs_publish

#
# Signal subscribe
#
SIGFS_SUBSCRIBE_SRC=sigfs_subscribe.cc ../log.cc
SIGFS_SUBSCRIBE_OBJ=${patsubst %.cc, %.o, ${SIGFS_SUBSCRIBE_SRC}}
SIGFS_SUBSCRIBE=sigfs_subscribe

DESTDIR ?= /usr/local
export DESTDIR

INCLUDES=-I..

debug: CXXFLAGS ?=-DSIGFS_LOG -ggdb  ${INCLUDES} -std=c++17 -Wall -pthread # -pg
CXXFLAGS ?=-O3 ${INCLUDES} -std=c++17 -Wall -pthread 

#
# Build the entire project.
#
all:  ${SIGFS_SUBSCRIBE} ${SIGFS_PUBLISH}

debug: ${SIGFS_SUBSCRIBE} ${SIGFS_PUBLISH}
${SIGFS_PUBLISH}: ${SIGFS_PUBLISH_OBJ} ${COMMON_OBJ}
	${CXX} -o ${SIGFS_PUBLISH} ${SIGFS_PUBLISH_OBJ} ${COMMON_OBJ} ${CXXFLAGS} 

${SIGFS_PUBLISH_OBJ}: ${HDR}

${SIGFS_SUBSCRIBE}: ${SIGFS_SUBSCRIBE_OBJ} ${COMMON_OBJ}
	${CXX} -o ${SIGFS_SUBSCRIBE} ${SIGFS_SUBSCRIBE_OBJ} ${COMMON_OBJ} ${CXXFLAGS} 

${SIGFS_SUBSCRIBE_OBJ}: ${HDR}

#
#	Remove all the generated files in this project.  Note that this does NOT
#	remove the generated files in the submodules.  Use "make distclean" to
#	clean up the submodules.
#
clean:
	rm -f   ${SIGFS_PUBLISH_OBJ} \
		${SIGFS_SUBSCRIBE_OBJ} \
		${SIGFS_PUBLISH} \
		${SIGFS_SUBSCRIBE} \
		*~ 


#
#	Install the generated files.
#
install:  ${SIGFS}
	install -d ${DESTDIR}/bin; \
	install -m 0755 ${SIGFS_PUBLISH}  ${DESTDIR}/bin; 
	install -m 0755 ${SIGFS_SUBSCRIBE}  ${DESTDIR}/bin; 

#
#	Uninstall the generated files.
#
uninstall:
	rm -f ${DESTDIR}/bin/${SIGFS_PUBLISH}; 
	rm -f ${DESTDIR}/bin/${SIGFS_SUBSCRIBE}; 

