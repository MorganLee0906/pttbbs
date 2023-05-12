# $Id$

SRCROOT=	..
.include "$(SRCROOT)/pttbbs.mk"

CFLAGS+= -DPTTBBS_UTIL

BBSBASE=	$(SRCROOT)/include/var.h

UTIL_OBJS=	util_var.o

MBBSD_OBJS= 	record	var

# �U���o�ǵ{��, �|�Q compile �åB�M $(UTIL_OBJS) �p��
CPROG_WITH_UTIL= \
	itoc_trans \
	sob_trans \
	transman

# �U���O C++ ���{��
CPP_WITH_UTIL= \


# �U���o�ǵ{��, �|�����Q compile
CPROG_WITHOUT_UTIL= \

# �U���o�ǵ{���|�Q install
PROGS=	${CPROG_WITH_UTIL}	${CPROG_WITHOUT_UTIL}	${CPP_WITH_UTIL}\

LDLIBS:= -L$(SRCROOT)/common/bbs -lcmbbs \
	 -L$(SRCROOT)/common/sys -lcmsys \
	 -L$(SRCROOT)/common/osdep -losdep \
	 $(LDLIBS)


all: $(SRCROOT)/include/var.h ${CPROG_WITH_UTIL} ${CPROG_WITHOUT_UTIL} ${CPP_WITH_UTIL} ${PROGS}

.for fn in ${CPROG_WITH_UTIL}
${fn}: ${BBSBASE} ${fn}.c ${UTIL_OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${fn} ${UTIL_OBJS} ${fn}.c $(LDLIBS)
.endfor

.for fn in ${CPP_WITH_UTIL}
${fn}: ${BBSBASE} ${fn}.cc ${UTIL_OBJS}
	${CXX} ${CXXFLAGS} ${LDFLAGS} -o ${fn} ${UTIL_OBJS} ${fn}.cc $(LDLIBS)
.endfor

.for fn in ${MBBSD_OBJS}
util_${fn}.o: ${BBSBASE} $(SRCROOT)/mbbsd/${fn}.c
	${CC} ${CFLAGS} -D_BBS_UTIL_C_ -c -o $@ $(SRCROOT)/mbbsd/${fn}.c
.endfor

install: $(PROGS)
	install -d $(BBSHOME)/bin/
	install -c -m 755 $(PROGS) $(BBSHOME)/bin/

clean:
	rm -f *.o $(CPROGS) $(CPROG_WITH_UTIL) $(CPROG_WITHOUT_UTIL) $(CPP_WITH_UTIL)
