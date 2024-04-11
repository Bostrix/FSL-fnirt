# $Id$
#include ${FSLCONFDIR}/default.mk

PROJNAME = meshclass

XFILES=drawmesh selfintersection

LIBS=-lfsl-newimage -lfsl-miscmaths -lfsl-NewNifti -lfsl-cprob -lfsl-znz -lfsl-utils

MESHOBJS=point.o mpoint.o triangle.o mesh.o pt_special.o profile.o
DRAWOBJS=drawmesh.o mpoint.o triangle.o mesh.o point.o pt_special.o
INTEROBJS=selfintersection.o mpoint.o triangle.o mesh.o point.o pt_special.o

all: libfsl-meshclass.so ${XFILES}

libfsl-meshclass.so: ${MESHOBJS}
	$(CXX) $(CXXFLAGS) -shared -o $@ $^ $(LDFLAGS)

drawmesh: ${DRAWOBJS}
	${CXX} ${CXXFLAGS} -o $@ $^ -L${HOME}/fnirt/newimage -L${HOME}/fnirt/miscmaths -L${HOME}/fnirt/NewNifti \
        -L${HOME}/fnirt/cprob -L${HOME}/fnirt/znzlib -L${HOME}/fnirt/utils -lfsl-newimage -lfsl-miscmaths -lfsl-NewNifti -lfsl-cprob -lfsl-znz -lfsl-utils -llapack -lblas -lz

selfintersection: ${INTEROBJS}
	${CXX} ${CXXFLAGS} -o $@ $^ ${LDFLAGS}

