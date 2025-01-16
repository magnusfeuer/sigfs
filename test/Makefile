#
# Top-level makefile for PHONY
#

.sigfs: all clean install uninstall debug

HDR=../sigfs_common.h ../log.h ../queue_impl.hh ../queue.hh

INCLUDES=-I.. $(shell pkg-config fuse3 --cflags)

#
# Signal test queue
#
SIGFS_TEST_QUEUE_INTEGRITY_SRC=sigfs_test_queue_integrity.cc ../log.cc ../queue.cc
SIGFS_TEST_QUEUE_INTEGRITY_OBJ=${patsubst %.cc, %.o, ${SIGFS_TEST_QUEUE_INTEGRITY_SRC}}
SIGFS_TEST_QUEUE_INTEGRITY=./sigfs_test_queue_integrity

#
# Test for speed
#
SIGFS_TEST_QUEUE_SPEED_SRC=sigfs_test_queue_speed.cc ../log.cc ../queue.cc
SIGFS_TEST_QUEUE_SPEED_OBJ=${patsubst %.cc, %.o, ${SIGFS_TEST_QUEUE_SPEED_SRC}}
SIGFS_TEST_QUEUE_SPEED=./sigfs_test_queue_speed

#
# Signal test queue
#
SIGFS_TEST_FUSE_SRC=sigfs_test_fuse.cc ../log.cc 
SIGFS_TEST_FUSE_OBJ=${patsubst %.cc, %.o, ${SIGFS_TEST_FUSE_SRC}}
SIGFS_TEST_FUSE=./sigfs_test_fuse


DESTDIR ?= /usr/local
export DESTDIR

debug: CXXFLAGS ?=-DSIGFS_LOG -ggdb  ${INCLUDES} -std=c++17 -Wall -pthread # -pg
CXXFLAGS ?=-O3 ${INCLUDES} -DSIGFS_LOG -std=c++17 -Wall -pthread

#
# Build the entire project.
#
all:  ${SIGFS_TEST_QUEUE_INTEGRITY} ${SIGFS_TEST_QUEUE_SPEED} ${SIGFS_TEST_FUSE}

debug: ${SIGFS_TEST_QUEUE_INTEGRITY} ${SIGFS_TEST_QUEUE_SPEED} ${SIGFS_TEST_FUSE}


#
# Test queue integrity
#

${SIGFS_TEST_QUEUE_INTEGRITY}: ${COMMON_OBJ} ${SIGFS_TEST_QUEUE_INTEGRITY_OBJ} 
	${CXX} -o ${SIGFS_TEST_QUEUE_INTEGRITY} ${SIGFS_TEST_QUEUE_INTEGRITY_OBJ} ${COMMON_OBJ} ${CXXFLAGS} 

${SIGFS_TEST_QUEUE_INTEGRITY_OBJ}: ${HDR}

#
# Test pure queue speed
#

${SIGFS_TEST_QUEUE_SPEED}: ${COMMON_OBJ} ${SIGFS_TEST_QUEUE_SPEED_OBJ} 
	${CXX} -o ${SIGFS_TEST_QUEUE_SPEED} ${SIGFS_TEST_QUEUE_SPEED_OBJ} ${COMMON_OBJ} ${CXXFLAGS} 

${SIGFS_TEST_QUEUE_SPEED_OBJ}: ${HDR}

#
# Test FUSE
#
${SIGFS_TEST_FUSE}: ${COMMON_OBJ} ${SIGFS_TEST_FUSE_OBJ} 
	${CXX} -o ${SIGFS_TEST_FUSE} ${SIGFS_TEST_FUSE_OBJ} ${COMMON_OBJ} ${CXXFLAGS} 

${SIGFS_TEST_FUSE_OBJ}: ${HDR}

# Clean up
clean:
	rm -f  	${SIGFS_TEST_QUEUE_INTEGRITY_OBJ} \
		${SIGFS_TEST_QUEUE_SPEED_OBJ} \
		${SIGFS_TEST_FUSE_OBJ} \
		${SIGFS_TEST_QUEUE_INTEGRITY} \
		${SIGFS_TEST_QUEUE_SPEED} \
		${SIGFS_TEST_FUSE} \
		*~ 


