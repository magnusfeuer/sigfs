#
# Top-level makefile for sigfs
#

.PHONY: all clean install uninstall debug

HDR=../sigfs_common.h ../log.h ../queue_impl.hh ../queue.hh

INCLUDES=-I.. $(shell pkg-config fuse3 --cflags)

#
# Signal test queue
#
SIGFS_TEST_QUEUE_SRC=sigfs_test_queue.cc ../log.cc ../queue.cc
SIGFS_TEST_QUEUE_OBJ=${patsubst %.cc, %.o, ${SIGFS_TEST_QUEUE_SRC}}
SIGFS_TEST_QUEUE=./sigfs_test_queue

#
# Test for speed
#
SIGFS_TEST_SPEED_SRC=sigfs_test_speed.cc ../log.cc ../queue.cc
SIGFS_TEST_SPEED_OBJ=${patsubst %.cc, %.o, ${SIGFS_TEST_SPEED_SRC}}
SIGFS_TEST_SPEED=./sigfs_test_speed

#
# Signal test queue
#
SIGFS_TEST_FUSE_SRC=sigfs_test_fuse.cc ../log.cc 
SIGFS_TEST_FUSE_OBJ=${patsubst %.cc, %.o, ${SIGFS_TEST_FUSE_SRC}}
SIGFS_TEST_FUSE=./sigfs_test_fuse


DESTDIR ?= /usr/local
export DESTDIR

debug: CXXFLAGS ?=-DSIGFS_LOG -ggdb  ${INCLUDES} -std=c++17 -Wall -pthread # -pg
CXXFLAGS ?=-O3 ${INCLUDES} -std=c++17 -Wall -pthread 

#
# Build the entire project.
#
all:  ${SIGFS_TEST_QUEUE} ${SIGFS_TEST_SPEED} ${SIGFS_TEST_FUSE}

debug: ${SIGFS_TEST_QUEUE} ${SIGFS_TEST_SPEED} ${SIGFS_TEST_FUSE}


#
# Test queue integrity
#

${SIGFS_TEST_QUEUE}: ${COMMON_OBJ} ${SIGFS_TEST_QUEUE_OBJ} 
	${CXX} -o ${SIGFS_TEST_QUEUE} ${SIGFS_TEST_QUEUE_OBJ} ${COMMON_OBJ} ${CXXFLAGS} 

${SIGFS_TEST_QUEUE_OBJ}: ${HDR}

#
# Test pure queue speed
#

${SIGFS_TEST_SPEED}: ${COMMON_OBJ} ${SIGFS_TEST_SPEED_OBJ} 
	${CXX} -o ${SIGFS_TEST_SPEED} ${SIGFS_TEST_SPEED_OBJ} ${COMMON_OBJ} ${CXXFLAGS} 

${SIGFS_TEST_SPEED_OBJ}: ${HDR}

#
# Test FUSE
#
${SIGFS_TEST_FUSE}: ${COMMON_OBJ} ${SIGFS_TEST_FUSE_OBJ} 
	${CXX} -o ${SIGFS_TEST_FUSE} ${SIGFS_TEST_FUSE_OBJ} ${COMMON_OBJ} ${CXXFLAGS} 

${SIGFS_TEST_FUSE_OBJ}: ${HDR}

# Clean up
clean:
	rm -f  	${SIGFS_TEST_QUEUE_OBJ} \
		${SIGFS_TEST_SPEED_OBJ} \
		${SIGFS_TEST_FUSE_OBJ} \
		${SIGFS_TEST_QUEUE} \
		${SIGFS_TEST_SPEED} \
		${SIGFS_TEST_FUSE} \
		*~ 


#
#	Install the generated files.
#
install:  ${SIGFS}
	install -d ${DESTDIR}/bin; \
	install -d ${DESTDIR}/bin; \
	install -m 0755 ${SIGFS_TEST_QUEUE} ${DESTDIR}/bin; 
	install -m 0755 ${SIGFS_TEST_FUSE} ${DESTDIR}/bin; 
	install -m 0755 ${SIGFS_TEST_SPEED} ${DESTDIR}/bin; 

#
#	Uninstall the generated files.
#
uninstall:
	rm -f ${DESTDIR}/bin/${SIGFS_TEST_QUEUE}; 
	rm -f ${DESTDIR}/bin/${SIGFS_TEST_FUSE; 
	rm -f ${DESTDIR}/bin/${SIGFS_TEST_SPEED}; 

