#
# Sigfs test makefile
# Builds tests
#

.PHONY: all clean distclean install uninstall 

HDR=../sigfs.hh ../log.h ../queue_impl.hh


INCLUDES=-I/usr/local/include

COMMON_SRC=../log.cc ../queue.cc ../signal.cc
COMMON_OBJ=${patsubst %.cc, %.o, ${COMMON_SRC}}

#
# Queue integrity and stress test
#
SIGFS_TEST_QUEUE_SRC=sigfs_test_queue.cc
SIGFS_TEST_QUEUE_OBJ=${patsubst %.cc, %.o, ${SIGFS_TEST_QUEUE_SRC}}
SIGFS_TEST_QUEUE=sigfs_test_queue

#
# Speed test
#
SIGFS_TEST_SPEED_SRC=sigfs_test_speed.cc
SIGFS_TEST_SPEED_OBJ=${patsubst %.cc, %.o, ${SIGFS_TEST_SPEED_SRC}}
SIGFS_TEST_SPEED=sigfs_test_speed

DESTDIR ?= /usr/local
export DESTDIR


debug: CXXFLAGS ?=-DSIGFS_LOG -ggdb ${INCLUDES} -std=c++17 -Wall -pthread -pg
CXXFLAGS ?=-O3 ${INCLUDES} -std=c++17 -Wall -pthread 

#
# Build the entire project.
#
all:  ${SIGFS_TEST_QUEUE}  ${SIGFS_TEST_SPEED} 
debug: ${SIGFS_TEST_QUEUE} ${SIGFS_TEST_SPEED} 

#
#	Rebuild the static target library.
#
${SIGFS_TEST_QUEUE}: ${SIGFS_TEST_QUEUE_OBJ} ${COMMON_OBJ}
	${CXX} -o ${SIGFS_TEST_QUEUE} ${SIGFS_TEST_QUEUE_OBJ} ${COMMON_OBJ} ${CXXFLAGS}

${SIGFS_TEST_SPEED}: ${SIGFS_TEST_SPEED_OBJ} ${COMMON_OBJ}
	${CXX} -o ${SIGFS_TEST_SPEED} ${SIGFS_TEST_SPEED_OBJ} ${COMMON_OBJ} ${CXXFLAGS}


${SIGFS_TEST_QUEUE_OBJ}: ${HDR}

${SIGFS_TEST_SPEED_OBJ}: ${HDR}


#
#	Remove all the generated files in this project.  Note that this does NOT
#	remove the generated files in the submodules.  Use "make distclean" to
#	clean up the submodules.
#
clean:
	rm -f  ${SIGFS_TEST_QUEUE_OBJ} ${SIGFS_TEST_QUEUE} ${SIGFS_TEST_SPEED_OBJ} ${SIGFS_TEST_SPEED}  *~


#
#	Remove all of the generated files including any in the submodules.
#
distclean: clean
	@${MAKE} clean

#
#	Install the generated files.
#
install:  ${SIGFS}
	install -d ${DESTDIR}/bin; \
	install -m 0644 ${SIGFS_TEST_QUEUE}  ${DESTDIR}/bin; 
	install -m 0644 ${SIGFS_TEST_SPEED}  ${DESTDIR}/bin; 

#
#	Uninstall the generated files.
#
uninstall:
	rm -f ${DESTDIR}/bin/${SIGFS_TEST_QUEUE}; 
	rm -f ${DESTDIR}/bin/${SIGFS_TEST_SPEED}; 

