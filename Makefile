# Specify the default compiler
CXX = g++

# Specify the -fpic flag
CXXFLAGS += -fpic

# Define source files
SRCS = fnirt fnirt_costfunctions fnirtfns intensity_mappers invwarp make_fnirt_field matching_points phase

# Additional LDFLAGS for znzlib library
ZNZLIB_LDFLAGS = -L${HOME}/fnirt/znzlib -lfsl-znz

# Define object files
OBJS = $(SRCS:.cc=.o)

# Define library source files and directories
LIB_DIRS = warpfns basisfield miscmaths newimage NewNifti utils cprob znzlib meshclass 
LIB_SRCS = $(foreach dir,$(LIB_DIRS),$(wildcard $(dir)/*.cc))
LIB_OBJS = $(LIB_SRCS:.cc=.o)

FNIRT_OBJS=fnirt_costfunctions.o intensity_mappers.o matching_points.o fnirtfns.o fnirt.o
INTMAP_OBJS=fnirt_costfunctions.o fnirtfns.o intensity_mappers.o matching_points.o mapintensities.o
INVWARP_OBJS=invwarp.o
MAKE_FNIRT_FIELD_OBJS=make_fnirt_field.o

XFILES = fnirt invwarp

# Define targets
all: ${XFILES}

# Compile the final executables
fnirt: libraries ${FNIRT_OBJS} ${LIB_OBJS}
	${CXX} ${CXXFLAGS} -o $@ ${FNIRT_OBJS} ${LIB_OBJS} ${LDFLAGS} ${ZNZLIB_LDFLAGS} -lblas -llapack -lz

invwarp: libraries ${INVWARP_OBJS} ${LIB_OBJS}
	${CXX} ${CXXFLAGS} -o $@ ${INVWARP_OBJS} ${LIB_OBJS} ${LDFLAGS} ${ZNZLIB_LDFLAGS} -lblas -llapack -lz

mapintensities: libraries ${INTMAP_OBJS} ${LIB_OBJS}
	${CXX} ${CXXFLAGS} -o $@ ${INTMAP_OBJS} ${LIB_OBJS} ${LDFLAGS} ${ZNZLIB_LDFLAGS} -lblas -llapack -lz

make_fnirt_field: libraries ${MAKE_FNIRT_FIELD_OBJS} ${LIB_OBJS}
	${CXX} ${CXXFLAGS} -o $@ ${MAKE_FNIRT_FIELD_OBJS} ${LIB_OBJS} ${LDFLAGS} ${ZNZLIB_LDFLAGS} -lblas -llapack -lz



# Rule to build object files
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Phony target to build all libraries
.PHONY: libraries
libraries:
	@for dir in $(LIB_DIRS); do \
	$(MAKE) -C $$dir CXX=$(CXX) CXXFLAGS='$(CXXFLAGS)' LDFLAGS='$(LDFLAGS)'; \
	done

# Clean rule
clean:
	rm -f fnirt invwarp $(OBJS) $(LIB_OBJS) $(shell find . -type f \( -name "*.o" -o -name "*.so" \))
