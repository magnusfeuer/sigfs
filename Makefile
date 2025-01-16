#
# Top-level makefile for sigfs
#

.PHONY: all clean debug install install-examples install-test uninstall test examples

HDR=queue.hh subscriber.hh sigfs_common.h log.h queue_impl.hh fs.hh


INCLUDES=-I./json/include $(shell pkg-config fuse3 --cflags)

#
# Signal FS main process
#
SIGFS_SRC=fs_filesys.cc fs_dir.cc fs_file.cc fs_inode.cc sigfs.cc log.cc queue.cc fs_access.cc
SIGFS_OBJ=${patsubst %.cc, %.o, ${SIGFS_SRC}}
SIGFS=sigfs

SIGFS_TEST_SRC=fs_test.cc fs_filesys.cc fs_inode.cc fs_dir.cc fs_file.cc fs_access.cc log.cc queue.cc
SIGFS_TEST_OBJ=${patsubst %.cc, %.o, ${SIGFS_TEST_SRC}}
SIGFS_TEST=sigfs_test

DESTDIR ?= /usr/local
export DESTDIR


debug: CXXFLAGS ?=-DSIGFS_LOG -ggdb  ${INCLUDES} -std=c++20 -Wall -pthread # -pg
CXXFLAGS ?=-O3 -DSIGFS_LOG ${INCLUDES} -std=c++20 -Wall -pthread 

#
# Build the entire project.
#
all:  ${SIGFS} ${SIGFS_TEST}

debug: ${SIGFS} ${SIGFS_TEST}

#
#	Rebuild the static target library.
#
${SIGFS}: ${SIGFS_OBJ}
	${CXX} -o ${SIGFS} ${SIGFS_OBJ} ${CXXFLAGS} `pkg-config fuse3 --cflags --libs`


#
#	Temporary test
#
${SIGFS_TEST}: ${SIGFS_TEST_OBJ}
	${CXX} -o ${SIGFS_TEST} ${SIGFS_TEST_OBJ} ${CXXFLAGS} `pkg-config fuse3 --cflags --libs`


${SIGFS_OBJ}: ${HDR}


#
#	Remove all the generated files in this project.  Note that this does NOT
#	remove the generated files in the submodules.  Use "make distclean" to
#	clean up the submodules.
#
clean:  
	(cd test; make clean)
	(cd example; make clean)
	rm -f ${SIGFS_OBJ} ${SIGFS} *~ 


#
#	Install the generated files.
#
install:  ${SIGFS}
	install -d ${DESTDIR}/bin; \
	install -m 0755 ${SIGFS}  ${DESTDIR}/bin; 

install-test:
	(cd test; make install)

install-examples:
	(cd examples; make install)

#
#	Uninstall the generated files.
#
uninstall:
	rm -f ${DESTDIR}/bin/${SIGFS}; 

uninstall-test:
	(cd test; make uninstall)

uninstall-examples:
	(cd examples; make uninstall)
#
# Build tests 
#
test:
	(cd test; make)

#
# Build examples
#
examples:
	(cd examples; make)
