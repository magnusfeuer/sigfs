#
# Top-level makefile for sigfs
#

.PHONY: all clean install uninstall test install_test 

HDR=queue.hh subscriber.hh sigfs_common.hh log.h queue_impl.hh


INCLUDES=-I/usr/local/include

COMMON_SRC=log.cc queue.cc
COMMON_OBJ=${patsubst %.cc, %.o, ${COMMON_SRC}}

#
# Signal FS main process
#
SIGFS_SRC=sigfs.cc 
SIGFS_OBJ=${patsubst %.cc, %.o, ${SIGFS_SRC}}
SIGFS=sigfs

#
# Signal publish
#
SIGFS_PUBLISH_SRC=sigfs_publish.cc
SIGFS_PUBLISH_OBJ=${patsubst %.cc, %.o, ${SIGFS_PUBLISH_SRC}}
SIGFS_PUBLISH=sigfs_publish

#
# Signal subscribe
#
SIGFS_SUBSCRIBE_SRC=sigfs_subscribe.cc
SIGFS_SUBSCRIBE_OBJ=${patsubst %.cc, %.o, ${SIGFS_SUBSCRIBE_SRC}}
SIGFS_SUBSCRIBE=sigfs_subscribe

#
# Signal test queue
#
SIGFS_TEST_QUEUE_SRC=test/sigfs_test_queue.cc
SIGFS_TEST_QUEUE_OBJ=${patsubst %.cc, %.o, ${SIGFS_TEST_QUEUE_SRC}}
SIGFS_TEST_QUEUE=./test/sigfs_test_queue

#
# Test for speed
#
SIGFS_TEST_SPEED_SRC=test/sigfs_test_speed.cc
SIGFS_TEST_SPEED_OBJ=${patsubst %.cc, %.o, ${SIGFS_TEST_SPEED_SRC}}
SIGFS_TEST_SPEED=./test/sigfs_test_speed


DESTDIR ?= /usr/local
export DESTDIR


debug: CXXFLAGS ?=-DSIGFS_LOG -ggdb ${INCLUDES} -std=c++17 -Wall -pthread
CXXFLAGS ?=-O3 ${INCLUDES} -std=c++17 -Wall -pthread 

#
# Build the entire project.
#
all:  ${SIGFS} ${SIGFS_PUBLISH} ${SIGFS_SUBSCRIBE} test

debug: ${SIGFS} ${SIGFS_PUBLISH} ${SIGFS_SUBSCRIBE} test

#
#	Rebuild the static target library.
#
${SIGFS}: ${SIGFS_OBJ} ${COMMON_OBJ}
	${CXX} -o ${SIGFS} ${SIGFS_OBJ} ${COMMON_OBJ} ${CXXFLAGS} `pkg-config fuse3 --cflags --libs`


${SIGFS_OBJ}: ${HDR}

${SIGFS_PUBLISH}: ${SIGFS_PUBLISH_OBJ} ${COMMON_OBJ}
	${CXX} -o ${SIGFS_PUBLISH} ${SIGFS_PUBLISH_OBJ} ${COMMON_OBJ} ${CXXFLAGS} 

${SIGFS_PUBLISH_OBJ}: ${HDR}

${SIGFS_SUBSCRIBE}: ${SIGFS_SUBSCRIBE_OBJ} ${COMMON_OBJ}
	${CXX} -o ${SIGFS_SUBSCRIBE} ${SIGFS_SUBSCRIBE_OBJ} ${COMMON_OBJ} ${CXXFLAGS} 

${SIGFS_SUBSCRIBE_OBJ}: ${HDR}

#
# Test 
#
${SIGFS_TEST_QUEUE}: ${SIGFS_TEST_QUEUE_OBJ} ${COMMON_OBJ}
	${CXX} -o ${SIGFS_TEST_QUEUE} ${SIGFS_TEST_QUEUE_OBJ} ${COMMON_OBJ} ${CXXFLAGS} 

${SIGFS_TEST_QUEUE_OBJ}: ${HDR}

${SIGFS_TEST_SPEED}: ${SIGFS_TEST_SPEED_OBJ} ${COMMON_OBJ}
	${CXX} -o ${SIGFS_TEST_SPEED} ${SIGFS_TEST_SPEED_OBJ} ${COMMON_OBJ} ${CXXFLAGS} 

${SIGFS_TEST_SPEED_OBJ}: ${HDR}


#
#	Remove all the generated files in this project.  Note that this does NOT
#	remove the generated files in the submodules.  Use "make distclean" to
#	clean up the submodules.
#
clean:
	rm -f   ${SIGFS_PUBLISH_OBJ} \
		${SIGFS_SUBSCRIBE_OBJ} \
		${SIGFS_TEST_QUEUE_OBJ} \
		${SIGFS_TEST_SPEED_OBJ} \
		${SIGFS_OBJ} \
		${COMMON_OBJ} \
		${SIGFS} \
		${SIGFS_PUBLISH} \
		${SIGFS_SUBSCRIBE} \
		*~ 


#
#	Install the generated files.
#
install:  ${SIGFS}
	install -d ${DESTDIR}/bin; \
	install -m 0755 ${SIGFS}  ${DESTDIR}/bin; 
	install -m 0755 ${SIGFS_PUBLISH}  ${DESTDIR}/bin; 
	install -m 0755 ${SIGFS_SUBSCRIBE}  ${DESTDIR}/bin; 

#
#	Uninstall the generated files.
#
uninstall:
	rm -f ${DESTDIR}/bin/${SIGFS}; 
	rm -f ${DESTDIR}/bin/${SIGFS_PUBLISH}; 
	rm -f ${DESTDIR}/bin/${SIGFS_SUBSCRIBE}; 


#
# Build tests only
#
test:	${SIGFS_TEST_SPEED} ${SIGFS_TEST_QUEUE}


#
#	Install the generated example files.
#
install_test:
	install -d ${DESTDIR}/bin; \
	install -m 0755 ${SIGFS_TEST_QUEUE} ${DESTDIR}/bin; 
	install -m 0755 ${SIGFS_TEST_SPEED} ${DESTDIR}/bin; 
