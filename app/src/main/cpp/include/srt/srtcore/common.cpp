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
Copyright (c) 2001 - 2016, The Board of Trustees of the University of Illinois.
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
   Yunhong Gu, last updated 07/25/2010
modified by
   Haivision Systems Inc.
*****************************************************************************/


#ifndef WIN32
   #include <cstring>
   #include <cerrno>
   #include <unistd.h>
   #if __APPLE__
      #include "TargetConditionals.h"
   #endif
   #if defined(OSX) || defined(TARGET_OS_IOS) || defined(TARGET_OS_TV)
      #include <mach/mach_time.h>
   #endif
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <win/wintime.h>
#endif

#include <string>
#include <sstream>
#include <cmath>
#include "md5.h"
#include "common.h"

#include <srt_compat.h> // SysStrError

bool CTimer::m_bUseMicroSecond = false;
uint64_t CTimer::s_ullCPUFrequency = CTimer::readCPUFrequency();

pthread_mutex_t CTimer::m_EventLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t CTimer::m_EventCond = PTHREAD_COND_INITIALIZER;

CTimer::CTimer():
m_ullSchedTime(),
m_TickCond(),
m_TickLock()
{
    pthread_mutex_init(&m_TickLock, NULL);
    pthread_cond_init(&m_TickCond, NULL);
}

CTimer::~CTimer()
{
    pthread_mutex_destroy(&m_TickLock);
    pthread_cond_destroy(&m_TickCond);
}

void CTimer::rdtsc(uint64_t &x)
{
   if (m_bUseMicroSecond)
   {
      x = getTime();
      return;
   }

   #ifdef IA32
      uint32_t lval, hval;
      //asm volatile ("push %eax; push %ebx; push %ecx; push %edx");
      //asm volatile ("xor %eax, %eax; cpuid");
      asm volatile ("rdtsc" : "=a" (lval), "=d" (hval));
      //asm volatile ("pop %edx; pop %ecx; pop %ebx; pop %eax");
      x = hval;
      x = (x << 32) | lval;
   #elif defined(IA64)
      asm ("mov %0=ar.itc" : "=r"(x) :: "memory");
   #elif defined(AMD64)
      uint32_t lval, hval;
      asm ("rdtsc" : "=a" (lval), "=d" (hval));
      x = hval;
      x = (x << 32) | lval;
   #elif defined(WIN32)
      //HANDLE hCurThread = ::GetCurrentThread(); 
      //DWORD_PTR dwOldMask = ::SetThreadAffinityMask(hCurThread, 1); 
      BOOL ret = QueryPerformanceCounter((LARGE_INTEGER *)&x);
      //SetThreadAffinityMask(hCurThread, dwOldMask);
      if (!ret)
         x = getTime() * s_ullCPUFrequency;
   #elif defined(OSX) || defined(TARGET_OS_IOS) || defined(TARGET_OS_TV)
      x = mach_absolute_time();
   #else
      // use system call to read time clock for other archs
      x = getTime();
   #endif
}

uint64_t CTimer::readCPUFrequency()
{
   uint64_t frequency = 1;  // 1 tick per microsecond.

   #if defined(IA32) || defined(IA64) || defined(AMD64)
      uint64_t t1, t2;

      rdtsc(t1);
      timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 100000000;
      nanosleep(&ts, NULL);
      rdtsc(t2);

      // CPU clocks per microsecond
      frequency = (t2 - t1) / 100000;
   #elif defined(WIN32)
      int64_t ccf;
      if (QueryPerformanceFrequency((LARGE_INTEGER *)&ccf))
         frequency = ccf / 1000000;
   #elif defined(OSX) || defined(TARGET_OS_IOS) || defined(TARGET_OS_TV)
      mach_timebase_info_data_t info;
      mach_timebase_info(&info);
      frequency = info.denom * 1000ULL / info.numer;
   #endif

   // Fall back to microsecond if the resolution is not high enough.
   if (frequency < 10)
   {
      frequency = 1;
      m_bUseMicroSecond = true;
   }
   return frequency;
}

uint64_t CTimer::getCPUFrequency()
{
   return s_ullCPUFrequency;
}

void CTimer::sleep(uint64_t interval)
{
   uint64_t t;
   rdtsc(t);

   // sleep next "interval" time
   sleepto(t + interval);
}

void CTimer::sleepto(uint64_t nexttime)
{
   // Use class member such that the method can be interrupted by others
   m_ullSchedTime = nexttime;

   uint64_t t;
   rdtsc(t);

   while (t < m_ullSchedTime)
   {
#ifndef NO_BUSY_WAITING
#ifdef IA32
       __asm__ volatile ("pause; rep; nop; nop; nop; nop; nop;");
#elif IA64
       __asm__ volatile ("nop 0; nop 0; nop 0; nop 0; nop 0;");
#elif AMD64
       __asm__ volatile ("nop; nop; nop; nop; nop;");
#endif
#else
       timeval now;
       timespec timeout;
       gettimeofday(&now, 0);
       if (now.tv_usec < 990000)
       {
           timeout.tv_sec = now.tv_sec;
           timeout.tv_nsec = (now.tv_usec + 10000) * 1000;
       }
       else
       {
           timeout.tv_sec = now.tv_sec + 1;
           timeout.tv_nsec = (now.tv_usec + 10000 - 1000000) * 1000;
       }
       THREAD_PAUSED();
       pthread_mutex_lock(&m_TickLock);
       pthread_cond_timedwait(&m_TickCond, &m_TickLock, &timeout);
       pthread_mutex_unlock(&m_TickLock);
       THREAD_RESUMED();
#endif

       rdtsc(t);
   }
}

void CTimer::interrupt()
{
   // schedule the sleepto time to the current CCs, so that it will stop
   rdtsc(m_ullSchedTime);
   tick();
}

void CTimer::tick()
{
    pthread_cond_signal(&m_TickCond);
}

uint64_t CTimer::getTime()
{
   //For Cygwin and other systems without microsecond level resolution, uncomment the following three lines
   //uint64_t x;
   //rdtsc(x);
   //return x / s_ullCPUFrequency;
   //Specific fix may be necessary if rdtsc is not available either.

	timeval t;
	gettimeofday(&t, 0);
	return t.tv_sec * 1000000ULL + t.tv_usec;
}

void CTimer::triggerEvent()
{
    pthread_cond_signal(&m_EventCond);
}

void CTimer::waitForEvent()
{
    timeval now;
    timespec timeout;
    gettimeofday(&now, 0);
    if (now.tv_usec < 990000)
    {
        timeout.tv_sec = now.tv_sec;
        timeout.tv_nsec = (now.tv_usec + 10000) * 1000;
    }
    else
    {
        timeout.tv_sec = now.tv_sec + 1;
        timeout.tv_nsec = (now.tv_usec + 10000 - 1000000) * 1000;
    }
    pthread_mutex_lock(&m_EventLock);
    pthread_cond_timedwait(&m_EventCond, &m_EventLock, &timeout);
    pthread_mutex_unlock(&m_EventLock);
}

void CTimer::sleep()
{
   #ifndef WIN32
      usleep(10);
   #else
      Sleep(1);
   #endif
}


//
// Automatically lock in constructor
CGuard::CGuard(pthread_mutex_t& lock):
m_Mutex(lock),
m_iLocked()
{
    m_iLocked = pthread_mutex_lock(&m_Mutex);
}

// Automatically unlock in destructor
CGuard::~CGuard()
{
    if (0 == m_iLocked)
        pthread_mutex_unlock(&m_Mutex);
}

int CGuard::enterCS(pthread_mutex_t& lock)
{
    return pthread_mutex_lock(&lock);
}

int CGuard::leaveCS(pthread_mutex_t& lock)
{
    return pthread_mutex_unlock(&lock);
}

void CGuard::createMutex(pthread_mutex_t& lock)
{
    pthread_mutex_init(&lock, NULL);
}

void CGuard::releaseMutex(pthread_mutex_t& lock)
{
    pthread_mutex_destroy(&lock);
}

void CGuard::createCond(pthread_cond_t& cond)
{
    pthread_cond_init(&cond, NULL);
}

void CGuard::releaseCond(pthread_cond_t& cond)
{
    pthread_cond_destroy(&cond);
}

//
CUDTException::CUDTException(CodeMajor major, CodeMinor minor, int err):
m_iMajor(major),
m_iMinor(minor)
{
   if (-1 == err)
      #ifndef WIN32
         m_iErrno = errno;
      #else
         m_iErrno = GetLastError();
      #endif
   else
      m_iErrno = err;
}

CUDTException::CUDTException(const CUDTException& e):
m_iMajor(e.m_iMajor),
m_iMinor(e.m_iMinor),
m_iErrno(e.m_iErrno),
m_strMsg()
{
}

CUDTException::~CUDTException()
{
}

const char* CUDTException::getErrorMessage()
{
   // translate "Major:Minor" code into text message.

   switch (m_iMajor)
   {
      case MJ_SUCCESS:
        m_strMsg = "Success";
        break;

      case MJ_SETUP:
        m_strMsg = "Connection setup failure";

        switch (m_iMinor)
        {
        case MN_TIMEOUT:
           m_strMsg += ": connection time out";
           break;

        case MN_REJECTED:
           m_strMsg += ": connection rejected";
           break;

        case MN_NORES:
#ifdef HAI_PATCH
           m_strMsg += ": unable to create/configure SRT socket";
#else  /* HAI_PATCH */
           m_strMsg += ": unable to create/configure UDP socket";
#endif /* HAI_PATCH */
           break;

        case MN_SECURITY:
           m_strMsg += ": abort for security reasons";
           break;

        default:
           break;
        }

        break;

      case MJ_CONNECTION:
        switch (m_iMinor)
        {
        case MN_CONNLOST:
           m_strMsg = "Connection was broken";
           break;

        case MN_NOCONN:
           m_strMsg = "Connection does not exist";
           break;

        default:
           break;
        }

        break;

      case MJ_SYSTEMRES:
        m_strMsg = "System resource failure";

        switch (m_iMinor)
        {
        case MN_THREAD:
           m_strMsg += ": unable to create new threads";
           break;

        case MN_MEMORY:
           m_strMsg += ": unable to allocate buffers";
           break;

        default:
           break;
        }

        break;

      case MJ_FILESYSTEM:
        m_strMsg = "File system failure";

        switch (m_iMinor)
        {
        case MN_SEEKGFAIL:
           m_strMsg += ": cannot seek read position";
           break;

        case MN_READFAIL:
           m_strMsg += ": failure in read";
           break;

        case MN_SEEKPFAIL:
           m_strMsg += ": cannot seek write position";
           break;

        case MN_WRITEFAIL:
           m_strMsg += ": failure in write";
           break;

        default:
           break;
        }

        break;

      case MJ_NOTSUP:
        m_strMsg = "Operation not supported";
 
        switch (m_iMinor)
        {
        case MN_ISBOUND:
           m_strMsg += ": Cannot do this operation on a BOUND socket";
           break;

        case MN_ISCONNECTED:
           m_strMsg += ": Cannot do this operation on a CONNECTED socket";
           break;

        case MN_INVAL:
           m_strMsg += ": Bad parameters";
           break;

        case MN_SIDINVAL:
           m_strMsg += ": Invalid socket ID";
           break;

        case MN_ISUNBOUND:
           m_strMsg += ": Cannot do this operation on an UNBOUND socket";
           break;

        case MN_NOLISTEN:
           m_strMsg += ": Socket is not in listening state";
           break;

        case MN_ISRENDEZVOUS:
           m_strMsg += ": Listen/accept is not supported in rendezous connection setup";
           break;

        case MN_ISRENDUNBOUND:
           m_strMsg += ": Cannot call connect on UNBOUND socket in rendezvous connection setup";
           break;

        case MN_ISSTREAM:
           m_strMsg += ": This operation is not supported in SOCK_STREAM mode";
           break;

        case MN_ISDGRAM:
           m_strMsg += ": This operation is not supported in SOCK_DGRAM mode";
           break;

        case MN_BUSY:
           m_strMsg += ": Another socket is already listening on the same port";
           break;

        case MN_XSIZE:
#ifdef HAI_PATCH
           m_strMsg += ": Message is too large to send (it must be less than the SRT send buffer size)";
#else  /* HAI_PATCH */
           m_strMsg += ": Message is too large to send (it must be less than the UDT send buffer size)";
#endif /* HAI_PATCH */
           break;

        case MN_EIDINVAL:
           m_strMsg += ": Invalid epoll ID";
           break;

        default:
           break;
        }

        break;

     case MJ_AGAIN:
        m_strMsg = "Non-blocking call failure";

        switch (m_iMinor)
        {
        case MN_WRAVAIL:
           m_strMsg += ": no buffer available for sending";
           break;

        case MN_RDAVAIL:
           m_strMsg += ": no data available for reading";
           break;

        case MN_XMTIMEOUT:
           m_strMsg += ": transmission timed out";
           break;

#ifdef SRT_ENABLE_ECN
        case MN_CONGESTION:
           m_strMsg += ": early congestion notification";
           break;
#endif /* SRT_ENABLE_ECN */
        default:
           break;
        }

        break;

     case MJ_PEERERROR:
        m_strMsg = "The peer side has signalled an error";

        break;

      default:
        m_strMsg = "Unknown error";
   }

   // Adding "errno" information
   if ((MJ_SUCCESS != m_iMajor) && (0 < m_iErrno))
   {
      m_strMsg += ": " + SysStrError(m_iErrno);
   }

   // period
   #ifndef WIN32
   m_strMsg += ".";
   #endif

   return m_strMsg.c_str();
}

#define UDT_XCODE(mj, mn) (int(mj)*1000)+int(mn)

int CUDTException::getErrorCode() const
{
    return UDT_XCODE(m_iMajor, m_iMinor);
}

int CUDTException::getErrno() const
{
   return m_iErrno;
}


void CUDTException::clear()
{
   m_iMajor = MJ_SUCCESS;
   m_iMinor = MN_NONE;
   m_iErrno = 0;
}

// XXX Move these into udt.h
const int CUDTException::SUCCESS = 0;
const int CUDTException::ECONNSETUP = UDT_XCODE(MJ_SETUP, 0);
const int CUDTException::ENOSERVER =  UDT_XCODE(MJ_SETUP, MN_TIMEOUT);
const int CUDTException::ECONNREJ =   UDT_XCODE(MJ_SETUP, MN_REJECTED);
const int CUDTException::ESOCKFAIL =  UDT_XCODE(MJ_SETUP, MN_NORES);
const int CUDTException::ESECFAIL =   UDT_XCODE(MJ_SETUP, MN_SECURITY);
const int CUDTException::ECONNFAIL = 2000;
const int CUDTException::ECONNLOST = 2001;
const int CUDTException::ENOCONN = 2002;
const int CUDTException::ERESOURCE = 3000;
const int CUDTException::ETHREAD = 3001;
const int CUDTException::ENOBUF = 3002;
const int CUDTException::EFILE = 4000;
const int CUDTException::EINVRDOFF = 4001;
const int CUDTException::ERDPERM = 4002;
const int CUDTException::EINVWROFF = 4003;
const int CUDTException::EWRPERM = 4004;
const int CUDTException::EINVOP = 5000;
const int CUDTException::EBOUNDSOCK = 5001;
const int CUDTException::ECONNSOCK = 5002;
const int CUDTException::EINVPARAM = 5003;
const int CUDTException::EINVSOCK = 5004;
const int CUDTException::EUNBOUNDSOCK = 5005;
const int CUDTException::ENOLISTEN = 5006;
const int CUDTException::ERDVNOSERV = 5007;
const int CUDTException::ERDVUNBOUND = 5008;
const int CUDTException::ESTREAMILL = 5009;
const int CUDTException::EDGRAMILL = 5010;
const int CUDTException::EDUPLISTEN = 5011;
const int CUDTException::ELARGEMSG = 5012;
const int CUDTException::EINVPOLLID = 5013;
const int CUDTException::EASYNCFAIL = 6000;
const int CUDTException::EASYNCSND = 6001;
const int CUDTException::EASYNCRCV = 6002;
const int CUDTException::ETIMEOUT = 6003;
#ifdef SRT_ENABLE_ECN
const int CUDTException::ECONGEST = 6004;
#endif /* SRT_ENABLE_ECN */ 
const int CUDTException::EPEERERR = 7000;
const int CUDTException::EUNKNOWN = -1;

#undef UDT_XCODE

//
bool CIPAddress::ipcmp(const sockaddr* addr1, const sockaddr* addr2, int ver)
{
   if (AF_INET == ver)
   {
      sockaddr_in* a1 = (sockaddr_in*)addr1;
      sockaddr_in* a2 = (sockaddr_in*)addr2;

      if ((a1->sin_port == a2->sin_port) && (a1->sin_addr.s_addr == a2->sin_addr.s_addr))
         return true;
   }
   else
   {
      sockaddr_in6* a1 = (sockaddr_in6*)addr1;
      sockaddr_in6* a2 = (sockaddr_in6*)addr2;

      if (a1->sin6_port == a2->sin6_port)
      {
         for (int i = 0; i < 16; ++ i)
            if (*((char*)&(a1->sin6_addr) + i) != *((char*)&(a2->sin6_addr) + i))
               return false;

         return true;
      }
   }

   return false;
}

void CIPAddress::ntop(const sockaddr* addr, uint32_t ip[4], int ver)
{
   if (AF_INET == ver)
   {
      sockaddr_in* a = (sockaddr_in*)addr;
      ip[0] = a->sin_addr.s_addr;
   }
   else
   {
      sockaddr_in6* a = (sockaddr_in6*)addr;
      ip[3] = (a->sin6_addr.s6_addr[15] << 24) + (a->sin6_addr.s6_addr[14] << 16) + (a->sin6_addr.s6_addr[13] << 8) + a->sin6_addr.s6_addr[12];
      ip[2] = (a->sin6_addr.s6_addr[11] << 24) + (a->sin6_addr.s6_addr[10] << 16) + (a->sin6_addr.s6_addr[9] << 8) + a->sin6_addr.s6_addr[8];
      ip[1] = (a->sin6_addr.s6_addr[7] << 24) + (a->sin6_addr.s6_addr[6] << 16) + (a->sin6_addr.s6_addr[5] << 8) + a->sin6_addr.s6_addr[4];
      ip[0] = (a->sin6_addr.s6_addr[3] << 24) + (a->sin6_addr.s6_addr[2] << 16) + (a->sin6_addr.s6_addr[1] << 8) + a->sin6_addr.s6_addr[0];
   }
}

void CIPAddress::pton(sockaddr* addr, const uint32_t ip[4], int ver)
{
   if (AF_INET == ver)
   {
      sockaddr_in* a = (sockaddr_in*)addr;
      a->sin_addr.s_addr = ip[0];
   }
   else
   {
      sockaddr_in6* a = (sockaddr_in6*)addr;
      for (int i = 0; i < 4; ++ i)
      {
         a->sin6_addr.s6_addr[i * 4] = ip[i] & 0xFF;
         a->sin6_addr.s6_addr[i * 4 + 1] = (unsigned char)((ip[i] & 0xFF00) >> 8);
         a->sin6_addr.s6_addr[i * 4 + 2] = (unsigned char)((ip[i] & 0xFF0000) >> 16);
         a->sin6_addr.s6_addr[i * 4 + 3] = (unsigned char)((ip[i] & 0xFF000000) >> 24);
      }
   }
}

using namespace std;


static string ShowIP4(const sockaddr_in* sin)
{
    ostringstream os;
    union
    {
        in_addr sinaddr;
        unsigned char ip[4];
    };
    sinaddr = sin->sin_addr;

    os << int(ip[0]);
    os << ".";
    os << int(ip[1]);
    os << ".";
    os << int(ip[2]);
    os << ".";
    os << int(ip[3]);
    return os.str();
}

static string ShowIP6(const sockaddr_in6* sin)
{
    ostringstream os;
    os.setf(ios::uppercase);

    bool sep = false;
    for (size_t i = 0; i < 16; ++i)
    {
        int v = sin->sin6_addr.s6_addr[i];
        if ( v )
        {
            if ( sep )
                os << ":";

            os << hex << v;
            sep = true;
        }
    }

    return os.str();
}

string CIPAddress::show(const sockaddr* adr)
{
    if ( adr->sa_family == AF_INET )
        return ShowIP4((const sockaddr_in*)adr);
    else if ( adr->sa_family == AF_INET6 )
        return ShowIP6((const sockaddr_in6*)adr);
    else
        return "(unsupported sockaddr type)";
}

//
void CMD5::compute(const char* input, unsigned char result[16])
{
   md5_state_t state;

   md5_init(&state);
   md5_append(&state, (const md5_byte_t *)input, strlen(input));
   md5_finish(&state, result);
}

std::string MessageTypeStr(UDTMessageType mt, uint32_t extt)
{
    using std::string;

    static string udt_types [] = {
        "handshake",
        "keepalive",
        "ack",
        "lossreport",
        "cgwarning", //4
        "shutdown",
        "ackack",
        "dropreq",
        "peererror", //8
    };

    static string srt_types [] = {
        "",
        "hsreq",
        "hsrsp",
        "kmreq",
        "kmrsp",
    };

#define LEN(arr) (sizeof (arr)/(sizeof ((arr)[0])))

    if ( mt == UMSG_EXT )
    {
        // Returrn "EXT:" with srt message name
        string val = "SRT:";
        if ( extt <= 0 || extt > LEN(srt_types) )
            return "EXT:unknown";

        return val + srt_types[extt];
    }

    if ( size_t(mt) > LEN(udt_types) )
        return "unknown";

#undef LEN

    return udt_types[mt];
}
