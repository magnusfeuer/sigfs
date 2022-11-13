#
# Sigfs test makefile
# Builds tests
#

.PHONY: all clean distclean install uninstall 

EXT_HDR=../sigfs.h 
HDR=${EXT_HDR} ../sigfs_internal.hh ../log.h


INCLUDES=-I/usr/local/include

COMMON_SRC=../log.cc ../queue.cc ../signal.cc
COMMON_OBJ=${patsubst %.cc, %.o, ${COMMON_SRC}}

#
# Signal FS main process
#
SIGFSTEST_QUEUE_SRC=sigfs_test_queue.cc
SIGFSTEST_QUEUE_OBJ=${patsubst %.cc, %.o, ${SIGFSTEST_QUEUE_SRC}}
SIGFS_TEST=sigfs_test_queue

DESTDIR ?= /usr/local
export DESTDIR


CXXFLAGS ?=-DSIGFS_LOG -ggdb ${INCLUDES} -std=c++17 -Wall -pthread

#
# Build the entire project.
#
all:  ${SIGFS_TEST} 

#
#	Rebuild the static target library.
#
${SIGFS_TEST}: ${SIGFSTEST_QUEUE_OBJ} ${COMMON_OBJ}
	${CXX} -o ${SIGFS_TEST} ${SIGFSTEST_QUEUE_OBJ} ${COMMON_OBJ} ${CXXFLAGS}


${SIGFSTEST_QUEUE_OBJ}: ${HDR}


#
#	Remove all the generated files in this project.  Note that this does NOT
#	remove the generated files in the submodules.  Use "make distclean" to
#	clean up the submodules.
#
clean:
	rm -f  ${SIGFSTEST_QUEUE_OBJ} *~ ${SIGFS_TEST} 


#
#	Remove all of the generated files including any in the submodules.
#
distclean: clean
	@${MAKE} clean

#
#	Install the generated files.
#
ninstall:  ${SIGFS}
	install -d ${DESTDIR}/bin; \
	install -m 0644 ${SIGFS_TEST}  ${DESTDIR}/bin; 

#
#	Uninstall the generated files.
#
uninstall:
	rm -f ${DESTDIR}/bin/${SIGFS_TEST}; 

