ifeq ($(findstring Rules.mk, $(wildcard *.mk)), Rules.mk)
   include Rules.mk
endif

CXX:=gcc -g

ifeq ($(CCSMPHOME),)
	CCSMPHOME:=..
endif

INCDIRS:=-I${CCSMPHOME} \
	 -I$(CCSMPHOME)/include

_OS=$(shell uname)
_ARCH := $(shell uname -m)
ifeq ($(_OS),HP-UX)
    _CPU=$(shell uname -m)
    INCDIRS+= -I.
else
    _CPU=$(shell uname -p)
endif
_REV=$(shell uname -r)
ARCHFLAGS:=
ifeq ($(SIXTY_FOUR_COMPAT),) 
ifeq ($(_ARCH),x86_64)
    SIXTY_FOUR_COMPAT:=
    LINUXLIBDIR=$(CCSMPHOME)/obj_Linux26-x86_64_debug/lib
    LLSYS:= -lrt -pthread
else
ifeq ($(_ARCH),armv6l)
    SIXTY_FOUR_COMPAT:=
    LINUXLIBDIR=$(CCSMPHOME)/obj_RaspberryPi-arm_debug/lib
else
    SIXTY_FOUR_COMPAT:=-m32
    LINUXLIBDIR=$(CCSMPHOME)/obj_Linux26-i386_debug/lib
    LLSYS:= -lrt -pthread
endif
endif
ifeq ($(findstring 2.4, $(_REV)), 2.4)
    LINUXLIBDIR=$(CCSMPHOME)/obj_Linux24-i386_debug/lib
endif
endif
ifeq ($(_ARCH),ppc64le)
    SIXTY_FOUR_COMPAT:= -pthread
    INCDIRS+= -I/usr/include/powerpc64le-linux-gnu
    LINUXLIBDIR=$(CCSMPHOME)/obj_Linux319-ppc64le_debug/lib
    LLSYS=$(SIXTY_FOUR_COMPAT) -lrt -pthread
endif
ifeq ($(_ARCH),ppc64)
    SIXTY_FOUR_COMPAT:= -pthread
    LINUXLIBDIR=$(CCSMPHOME)/obj_Linux30-ppc64_debug/lib
    LLSYS=$(SIXTY_FOUR_COMPAT) -lrt -pthread
endif
ifeq ($(_ARCH),ia64)
    LINUXLIBDIR:=$(CCSMPHOME)/obj_Linux26-i386_debug/lib
    LLSYS:= $(SIXTY_FOUR_COMPAT) -lrt -lxnet -pthread
endif
ifeq ($(_OS), AIX)
    LIBDIRS:=-L$(CCSMPHOME)/lib -L$(CCSMPHOME)/obj_AIX6-powerpc_64_debug/lib
    SIXTY_FOUR_COMPAT:=-maix64 -pthread -DSOLCLIENT_AIX_BUILD
    LINUXLIBDIR=$(CCSMPHOME)/obj_AIX6-powerpc_64_debug/lib
    LLSYS:= $(SIXTY_FOUR_COMPAT) -lrt -pthread
    LINKFLAGS:= -Xlinker -brtl 
endif
ifeq ($(_OS), SunOS)
ifeq ($(_CPU), sparc)
    LIBDIRS:=-L$(CCSMPHOME)/lib -L$(CCSMPHOME)/obj_SunOS10-sparc_debug/lib
else
    LIBDIRS:=-L$(CCSMPHOME)/lib -L$(CCSMPHOME)/obj_SunOS10-i386_debug/lib
endif
    LLSYS:= -lsocket $(SIXTY_FOUR_COMPAT) -lrt
else
    LIBDIRS:=-L$(CCSMPHOME)/lib -L$(LINUXLIBDIR)
    LLSYS:= $(SIXTY_FOUR_COMPAT) -lrt -pthread
endif
ifeq ($(_OS), Android)
    LIBDIRS:=-L$(CCSMPHOME)/lib -L$(LINUXLIBDIR)
    LLSYS:= $(SIXTY_FOUR_COMPAT)
endif

ifeq ($(_OS), HP-UX)
    DEPENDS:= common.o os.o getopt_long.o
else 
ifeq ($(_OS), AIX)
    DEPENDS:= common.o os.o getopt_long.o
else
    DEPENDS:= common.o os.o
endif
endif

COMPILEFLAG:= $(COMPILEFLAG) $(INCDIRS) $(ARCHFLAGS) -DSOLCLIENT_EXCLUDE_DEPRECATED -DPROVIDE_LOG_UTILITIES -DSOLCLIENT_CONST_PROPERTIES -g
LINKFLAGS+= $(LIBDIRS) -lsolclient $(LLSYS)

%.o:	%.c
	$(CXX) $(COMPILEFLAG)  $(SIXTY_FOUR_COMPAT) -c $< -o $@

EXECS:= directPubSub perfADPub perfADSub perfTest \
        messageSelectorsOnQueue asyncCacheRequest syncCacheRequest sempGetOverMb topicToQueueMapping \
        messageTTLAndDeadMessageQueue topicDispatch eventMonitor adPubAck simpleFlowToQueue \
        simpleFlowToTopic subscribeOnBehalfOfClient queueProvision redirectLogs sdtPubSubMsgDep sdtPubSubMsgIndep \
        messageReplay noLocalPubSub flowControlQueue simpleBrowserFlow cutThroughFlowToQueue replication \
        activeFlowIndication secureSession RRGuaranteedRequester RRGuaranteedReplier RRDirectRequester RRDirectReplier transactions

all: $(EXECS)

clean:
	 rm -f $(EXECS) directPubSub.o perfTest.o perfADPub.o perfADSub.o \
         os.o getopt_long.o common.o messageSelectorsOnQueue.o asyncCacheRequest.o \
	 syncCacheRequest.o sempGetOverMb.o topicToQueueMapping.o messageTTLAndDeadMessageQueue.o topicDispatch.o \
	 eventMonitor.o adPubAck.o simpleFlowToQueue.o simpleFlowToTopic.o subscribeOnBehalfOfClient.o \
	 queueProvision.o redirectLogs.o sdtPubSubMsgDep.o sdtPubSubMsgIndep.o messageReplay.o noLocalPubSub.o \
         flowControlQueue.o simpleBrowserFlow.o cutThroughFlowToQueue.o replication.o activeFlowIndication.o secureSession.o \
         RRGuaranteedRequester.o RRGuaranteedReplier.o RRDirectRequester.o RRDirectReplier.o transactions.o

directPubSub : directPubSub.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

perfADPub : perfADPub.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

perfADSub : perfADSub.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

perfTest : perfTest.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

messageSelectorsOnQueue : messageSelectorsOnQueue.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

asyncCacheRequest : asyncCacheRequest.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

syncCacheRequest : syncCacheRequest.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

sempGetOverMb : sempGetOverMb.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

topicToQueueMapping : topicToQueueMapping.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

messageTTLAndDeadMessageQueue : messageTTLAndDeadMessageQueue.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

topicDispatch : topicDispatch.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

eventMonitor : eventMonitor.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

adPubAck : adPubAck.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

simpleFlowToQueue : simpleFlowToQueue.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

simpleFlowToTopic : simpleFlowToTopic.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

subscribeOnBehalfOfClient : subscribeOnBehalfOfClient.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

queueProvision : queueProvision.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

redirectLogs : redirectLogs.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

sdtPubSubMsgDep : sdtPubSubMsgDep.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

sdtPubSubMsgIndep : sdtPubSubMsgIndep.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

messageReplay : messageReplay.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

noLocalPubSub : noLocalPubSub.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

flowControlQueue : flowControlQueue.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

simpleBrowserFlow : simpleBrowserFlow.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

cutThroughFlowToQueue : cutThroughFlowToQueue.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

replication : replication.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

activeFlowIndication : activeFlowIndication.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

secureSession : secureSession.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

RRGuaranteedRequester : RRGuaranteedRequester.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

RRGuaranteedReplier : RRGuaranteedReplier.o  $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)


RRDirectRequester : RRDirectRequester.o $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

RRDirectReplier : RRDirectReplier.o  $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

transactions : transactions.o  $(DEPENDS)
	$(CXX) -o $@ $^ $(LINKFLAGS)

