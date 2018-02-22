/*****************************************************************************
 * SRT - Secure, Reliable, Transport
 * Copyright (c) 2017 Haivision Systems Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; If not, see <http://www.gnu.org/licenses/>
 * 
 * Based on UDT4 SDK version 4.11
 *****************************************************************************/

/*****************************************************************************
Copyright (c) 2001 - 2010, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu, last updated 09/28/2010
modified by
   Haivision Systems Inc.
*****************************************************************************/

#ifndef __UDT_API_H__
#define __UDT_API_H__


#include <map>
#include <vector>
#include <string>
#include "netinet_any.h"
#include "udt.h"
#include "packet.h"
#include "queue.h"
#include "cache.h"
#include "epoll.h"

class CUDT;

class CUDTSocket
{
public:
   CUDTSocket();
   ~CUDTSocket();

   UDTSTATUS m_Status;                       //< current socket state

   uint64_t m_TimeStamp;                     //< time when the socket is closed

   int m_iIPversion;                         //< IP version
   sockaddr* m_pSelfAddr;                    //< pointer to the local address of the socket
   sockaddr* m_pPeerAddr;                    //< pointer to the peer address of the socket

   UDTSOCKET m_SocketID;                     //< socket ID
   UDTSOCKET m_ListenSocket;                 //< ID of the listener socket; 0 means this is an independent socket

   UDTSOCKET m_PeerID;                       //< peer socket ID
   int32_t m_iISN;                           //< initial sequence number, used to tell different connection from same IP:port

   CUDT* m_pUDT;                             //< pointer to the UDT entity

   std::set<UDTSOCKET>* m_pQueuedSockets;    //< set of connections waiting for accept()
   std::set<UDTSOCKET>* m_pAcceptSockets;    //< set of accept()ed connections

   pthread_cond_t m_AcceptCond;              //< used to block "accept" call
   pthread_mutex_t m_AcceptLock;             //< mutex associated to m_AcceptCond

   unsigned int m_uiBackLog;                 //< maximum number of connections in queue

   int m_iMuxID;                             //< multiplexer ID

   pthread_mutex_t m_ControlLock;            //< lock this socket exclusively for control APIs: bind/listen/connect

   static int64_t getPeerSpec(UDTSOCKET id, int32_t isn)
   {
       return (id << 30) + isn;
   }
   int64_t getPeerSpec()
   {
       return getPeerSpec(m_PeerID, m_iISN);
   }

private:
   CUDTSocket(const CUDTSocket&);
   CUDTSocket& operator=(const CUDTSocket&);
};

////////////////////////////////////////////////////////////////////////////////

class CUDTUnited
{
friend class CUDT;
friend class CRendezvousQueue;

public:
   CUDTUnited();
   ~CUDTUnited();

public:

   std::string CONID(UDTSOCKET sock) const;

      /// initialize the UDT library.
      /// @return 0 if success, otherwise -1 is returned.

   int startup();

      /// release the UDT library.
      /// @return 0 if success, otherwise -1 is returned.

   int cleanup();

      /// Create a new UDT socket.
      /// @param [in] af IP version, IPv4 (AF_INET) or IPv6 (AF_INET6).
      /// @param [in] type socket type, SOCK_STREAM or SOCK_DGRAM
      /// @return The new UDT socket ID, or INVALID_SOCK.

   UDTSOCKET newSocket(int af, int type);

      /// Create a new UDT connection.
      /// @param [in] listen the listening UDT socket;
      /// @param [in] peer peer address.
      /// @param [in,out] hs handshake information from peer side (in), negotiated value (out);
      /// @return If the new connection is successfully created: 1 success, 0 already exist, -1 error.

   int newConnection(const UDTSOCKET listen, const sockaddr* peer, CHandShake* hs);

      /// look up the UDT entity according to its ID.
      /// @param [in] u the UDT socket ID.
      /// @return Pointer to the UDT entity.

   CUDT* lookup(const UDTSOCKET u);

      /// Check the status of the UDT socket.
      /// @param [in] u the UDT socket ID.
      /// @return UDT socket status, or NONEXIST if not found.

   UDTSTATUS getStatus(const UDTSOCKET u);

      // socket APIs

   int bind(const UDTSOCKET u, const sockaddr* name, int namelen);
   int bind(const UDTSOCKET u, UDPSOCKET udpsock);
   int listen(const UDTSOCKET u, int backlog);
   UDTSOCKET accept(const UDTSOCKET listen, sockaddr* addr, int* addrlen);
   int connect(const UDTSOCKET u, const sockaddr* name, int namelen, int32_t forced_isn);
   int close(const UDTSOCKET u);
   int getpeername(const UDTSOCKET u, sockaddr* name, int* namelen);
   int getsockname(const UDTSOCKET u, sockaddr* name, int* namelen);
   int select(ud_set* readfds, ud_set* writefds, ud_set* exceptfds, const timeval* timeout);
   int selectEx(const std::vector<UDTSOCKET>& fds, std::vector<UDTSOCKET>* readfds, std::vector<UDTSOCKET>* writefds, std::vector<UDTSOCKET>* exceptfds, int64_t msTimeOut);
   int epoll_create();
   int epoll_add_usock(const int eid, const UDTSOCKET u, const int* events = NULL);
   int epoll_add_ssock(const int eid, const SYSSOCKET s, const int* events = NULL);
   int epoll_remove_usock(const int eid, const UDTSOCKET u);
   int epoll_remove_ssock(const int eid, const SYSSOCKET s);
#ifdef HAI_PATCH
   int epoll_update_usock(const int eid, const UDTSOCKET u, const int* events = NULL);
   int epoll_update_ssock(const int eid, const SYSSOCKET s, const int* events = NULL);
#endif /* HAI_PATCH */
   int epoll_wait(const int eid, std::set<UDTSOCKET>* readfds, std::set<UDTSOCKET>* writefds, int64_t msTimeOut, std::set<SYSSOCKET>* lrfds = NULL, std::set<SYSSOCKET>* lwfds = NULL);
   int epoll_release(const int eid);

      /// record the UDT exception.
      /// @param [in] e pointer to a UDT exception instance.

   void setError(CUDTException* e);

      /// look up the most recent UDT exception.
      /// @return pointer to a UDT exception instance.

   CUDTException* getError();

private:
//   void init();

private:
   std::map<UDTSOCKET, CUDTSocket*> m_Sockets;       // stores all the socket structures

   pthread_mutex_t m_ControlLock;                    // used to synchronize UDT API

   pthread_mutex_t m_IDLock;                         // used to synchronize ID generation
   UDTSOCKET m_SocketIDGenerator;                             // seed to generate a new unique socket ID

   std::map<int64_t, std::set<UDTSOCKET> > m_PeerRec;// record sockets from peers to avoid repeated connection request, int64_t = (socker_id << 30) + isn

private:
   pthread_key_t m_TLSError;                         // thread local error record (last error)
   static void TLSDestroy(void* e) {if (NULL != e) delete (CUDTException*)e;}

private:
   void connect_complete(const UDTSOCKET u);
   CUDTSocket* locate(const UDTSOCKET u);
   CUDTSocket* locate(const sockaddr* peer, const UDTSOCKET id, int32_t isn);
   void updateMux(CUDTSocket* s, const sockaddr* addr = NULL, const UDPSOCKET* = NULL);
   void updateMux(CUDTSocket* s, const CUDTSocket* ls);

private:
   std::map<int, CMultiplexer> m_mMultiplexer;		// UDP multiplexer
   pthread_mutex_t m_MultiplexerLock;

private:
   CCache<CInfoBlock>* m_pCache;			// UDT network information cache

private:
   volatile bool m_bClosing;
   pthread_mutex_t m_GCStopLock;
   pthread_cond_t m_GCStopCond;

   pthread_mutex_t m_InitLock;
   int m_iInstanceCount;				// number of startup() called by application
   bool m_bGCStatus;					// if the GC thread is working (true)

   pthread_t m_GCThread;
   static void* garbageCollect(void*);

   std::map<UDTSOCKET, CUDTSocket*> m_ClosedSockets;   // temporarily store closed sockets

   void checkBrokenSockets();
   void removeSocket(const UDTSOCKET u);

private:
   CEPoll m_EPoll;                                     // handling epoll data structures and events

private:
   CUDTUnited(const CUDTUnited&);
   CUDTUnited& operator=(const CUDTUnited&);
};

// Debug support
inline std::string SockaddrToString(const sockaddr* sadr)
{
	void* addr =
        sadr->sa_family == AF_INET ?
            (void*)&((sockaddr_in*)sadr)->sin_addr
        : sadr->sa_family == AF_INET6 ?
            (void*)&((sockaddr_in6*)sadr)->sin6_addr
        : 0;
	// (cast to (void*) is required because otherwise the 2-3 arguments
	// of ?: operator would have different types, which isn't allowed in C++.
        if ( !addr )
            return "unknown:0";

	std::ostringstream output;
	char hostbuf[1024];
	if (!getnameinfo(sadr, sizeof(*sadr), hostbuf, 1024, NULL, 0, NI_NAMEREQD))
	{
		output << hostbuf;
	}
	else
	{
		output << "unknown";
	}

	output << ":" << ntohs(((sockaddr_in*)sadr)->sin_port); // TRICK: sin_port and sin6_port have the same offset and size
	return output.str();
}


#endif
