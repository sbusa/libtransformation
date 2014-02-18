INCLUDES = -I../include

AM_LDFLAGS = -no-undefined -dlopen self @PREOPEN@ -static

bin_PROGRAMS =

# the final solution
bin_PROGRAMS     += universal
universal_SOURCES = universal.c
universal_LDADD   = ../library/libNovaLoader.la

# a utility to check a module
bin_PROGRAMS        += check-module
check_module_SOURCES = check-module.c
check_module_LDADD   = ../library/libNovaLoader.la

# a utility to play with transformation engine
bin_PROGRAMS        += transformer
transformer_SOURCES = transformer.c
transformer_LDADD   = ../library/libNovaLoader.la

if BUILD_TESTPROGS

if BUILD_WATCHDOG

bin_PROGRAMS       += watchdog
watchdog_SOURCES    = watchdog.c
watchdog_LDADD      = ../library/libNovaLoader.la

endif


bin_PROGRAMS += dummy
dummy_SOURCES = dummy.c
dummy_LDADD   = ../library/libNovaLoader.la

if BUILD_NTLM

bin_PROGRAMS += test_ntlm
test_ntlm_SOURCES = test_ntlm.c
test_ntlm_LDADD   = ../library/libNovaLoader.la

endif

if BUILD_ACTIVEMQ

bin_PROGRAMS += activemqtest
activemqtest_SOURCES = activemqtest.c
activemqtest_LDADD   = ../library/libNovaLoader.la


bin_PROGRAMS += activemq_producer
activemq_producer_SOURCES = activemq_producer.c
activemq_producer_LDADD   = ../library/libNovaLoader.la


bin_PROGRAMS += activemq_consumer
activemq_consumer_SOURCES = activemq_consumer.c
activemq_consumer_LDADD   = ../library/libNovaLoader.la

endif

bin_PROGRAMS += test_route
test_route_SOURCES = test_route.c
test_route_LDADD   = ../library/libNovaLoader.la

if BUILD_PCAP

bin_PROGRAMS += test_pcap
test_pcap_SOURCES = test_pcap.c
test_pcap_LDADD   = ../library/libNovaLoader.la

endif

bin_PROGRAMS += test_hmap
test_hmap_SOURCES = test_hmap.c
test_hmap_LDADD   = ../library/libNovaLoader.la

#bin_PROGRAMS += dummy
#dummy_SOURCES = dummy.c
#dummy_LDADD   = ../library/libNovaLoader.la

bin_PROGRAMS       += tcp-connect
tcp_connect_SOURCES = tcp-connect.c
tcp_connect_LDADD   = ../library/libNovaLoader.la

bin_PROGRAMS    += database
database_SOURCES = database.c
database_LDADD   = ../library/libNovaLoader.la

endif
