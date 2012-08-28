
INCLUDES = $(DECAF_CXXFLAGS) -I../include $(ACTIVEMQ_CFLAGS)

AM_LDFLAGS = -no-undefined -dlopen self @PREOPEN@ -static

bin_PROGRAMS =

lib_LIBRARIES =


if BUILD_ACTIVEMQ

# the final solution
lib_LIBRARIES += libmq.a
libmq_a_SOURCES = mq.cpp
libmq_a_CXXFLAGS = -ansi -fPIC

bin_PROGRAMS += SimpleAsyncConsumer
SimpleAsyncConsumer_SOURCES  = SimpleAsyncConsumer.cpp

SimpleAsyncConsumer_LDFLAGS = @LIBACTIVEMQ@ $(DECAF_LIBS)

SimpleAsyncConsumer_CXXFLAGS = -ansi

endif


