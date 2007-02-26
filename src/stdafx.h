#pragma once

#ifndef STDAFX_H
  #define STDAFX_H

  #ifdef HAVE_CONFIG_H
    #include <config.h>
  #elif defined(__WIN32__) || defined(WIN32)
    // borland/msvs win32 specifics..
    #define HAVE_WINSOCK
    #define HAVE_WSNWLINK_H
  #endif

  #if defined(HAVE_UNISTD_H)
    # include <unistd.h>
  #endif

  #if defined(HAVE_WINSOCK)
    #define WIN32_LEAN_AND_MEAN
    #include <Winsock2.h>
    #include <Wsipx.h>
    #include <ws2tcpip.h>

    #ifdef HAVE_WSNWLINK_H
      # include <wsnwlink.h>
    #endif

    #define NetGetLastError() WSAGetLastError()
    #define sipx_family sa_family
  #else
    // non-win32 (unix?) stuff
    #if HAVE_NETINET_IN_H
      # include <netinet/in.h>
    #endif
    #if HAVE_ARPA_INET_H
      # include <arpa/inet.h>
    #endif
    #if HAVE_ERRNO_H
      # include <errno.h> // for errno in NetGetLastError() macro
    #endif

    /* ripped from google code search somewhere */
    #ifdef HAVE_NETIPX_IPX_H
      # include <netipx/ipx.h>
    #else
      # include <linux/types.h>
      # include <linux/ipx.h>
      # ifndef IPX_TYPE
        #  define IPX_TYPE 1
      # endif
    #endif

    #define closesocket close
    #define NetGetLastError() errno

    typedef int SOCKET;

    #define INVALID_SOCKET  (SOCKET)(~0)
    #define SOCKET_ERROR            (-1)
    #define NSPROTO_IPX PF_IPX
    #define SOCKADDR_IPX sockaddr_ipx
    #define sa_nodenum sipx_node
    #define sa_socket sipx_port
    #define sa_netnum sipx_network
  #endif

  /* Gooey crap */
  #include <gtk/gtk.h>
  #include <glade/glade.h>

  /* general headers */
  #include "types.h"
  #include <string>
  #include <iostream>
  #include <stdio.h>
  #include <assert.h>
  #include <vector>

  /* fix for MSVS (C++ 2005 express edition) */
  #if defined(_MSC_VER)
    #define snprintf _snprintf
  #endif

  /* Crossplatform Thread handling */
  #if defined(__WIN32__) || defined(WIN32)
    #error FIXME: Win32 Thread handling
  #else
    #include <pthread.h>
    #define THREAD pthread_t
    #define THREAD_CREATE_OK 0
  #endif
#endif //STDAFX_H
