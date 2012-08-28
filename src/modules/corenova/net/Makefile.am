SUBDIRS = proto 

INCLUDES = -I$(top_builddir)/src/include

# all modules built here should have this 
AM_LDFLAGS = -avoid-version -module -no-undefined

pkglibdir = $(libdir)/corenova/net
pkglib_LTLIBRARIES =


##
# Networking Modules

pkglib_LTLIBRARIES += socket.la
socket_la_SOURCES   = socket.c
socket_la_CFLAGS    = -DMODULE=corenova_net_socket

pkglib_LTLIBRARIES += resolve.la
resolve_la_SOURCES  = resolve.c
resolve_la_CFLAGS   = -DMODULE=corenova_net_resolve

pkglib_LTLIBRARIES += tcp.la
tcp_la_SOURCES      = tcp.c
tcp_la_CFLAGS       = -DMODULE=corenova_net_tcp

pkglib_LTLIBRARIES += udp.la
udp_la_SOURCES      = udp.c
udp_la_CFLAGS       = -DMODULE=corenova_net_udp

pkglib_LTLIBRARIES += route.la
route_la_SOURCES      = route.c
route_la_CFLAGS       = -DMODULE=corenova_net_route

if BUILD_SSL

pkglib_LTLIBRARIES += ssl.la
ssl_la_SOURCES      = ssl.c
ssl_la_CFLAGS       = -DMODULE=corenova_net_ssl @SSL_CFLAGS@
ssl_la_LIBADD       = @LIBSSL@

endif

pkglib_LTLIBRARIES   += transport.la
transport_la_SOURCES  = transport.c
transport_la_CFLAGS   = -DMODULE=corenova_net_transport

pkglib_LTLIBRARIES += session.la
session_la_SOURCES  = session.c
session_la_CFLAGS   = -DMODULE=corenova_net_session

pkglib_LTLIBRARIES += protocol.la
protocol_la_SOURCES = protocol.c
protocol_la_CFLAGS  = -DMODULE=corenova_net_protocol

pkglib_LTLIBRARIES += neticmp.la
neticmp_la_SOURCES = neticmp.c
neticmp_la_CFLAGS  = -DMODULE=corenova_net_neticmp

if BUILD_NTLM

pkglib_LTLIBRARIES += netntlm.la
netntlm_la_SOURCES = netntlm.c
netntlm_la_CFLAGS = -DMODULE=corenova_net_netntlm
netntlm_la_LIBADD = @LIBNTLM@

endif

if BUILD_ACTIVEMQ

# keen
pkglib_LTLIBRARIES += activemq.la
activemq_la_SOURCES = activemq.c
activemq_la_CFLAGS  = -DMODULE=corenova_net_activemq

activemq_la_LDFLAGS	= -module -L../../../contrib/ActiveMQ/ -lmq -lstdc++ 
activemq_la_LIBADD	= @LIBACTIVEMQ@ $(DECAF_LIBS)
activemq_la_DEPENDENCIES = ../../../contrib/ActiveMQ/libmq.a

endif

pkglib_LTLIBRARIES += server.la
server_la_SOURCES   = server.c
server_la_CFLAGS    = -DMODULE=corenova_net_server

pkglib_LTLIBRARIES += filter.la
filter_la_SOURCES   = filter.c
filter_la_CFLAGS    = -DMODULE=corenova_net_filter

pkglib_LTLIBRARIES += packet.la
packet_la_SOURCES   = packet.c
packet_la_CFLAGS    = -DMODULE=corenova_net_packet

pkglib_LTLIBRARIES += pcap.la
pcap_la_SOURCES   = pcap.c
pcap_la_CFLAGS    = -DMODULE=corenova_net_pcap @PCAP_CFLAGS@
pcap_la_LIBADD    = @PCAP_LDFLAGS@


