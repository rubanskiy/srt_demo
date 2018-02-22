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
Copyright (c) 2001 - 2009, The Board of Trustees of the University of Illinois.
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
   Yunhong Gu, last updated 05/05/2009
modified by
   Haivision Systems Inc.
*****************************************************************************/

#ifndef __UDT_BUFFER_H__
#define __UDT_BUFFER_H__


#include "udt.h"
#include "list.h"
#include "queue.h"
#include "utilities.h"
#include <fstream>

class CSndBuffer
{
public:

   // XXX There's currently no way to access the socket ID set for
   // whatever the buffer is currently working for. Required to find
   // some way to do this, possibly by having a "reverse pointer".
   // Currently just "unimplemented".
   std::string CONID() const { return ""; }

   CSndBuffer(int size = 32, int mss = 1500);
   ~CSndBuffer();

      /// Insert a user buffer into the sending list.
      /// @param [in] data pointer to the user data block.
      /// @param [in] len size of the block.
      /// @param [in] ttl time to live in milliseconds
      /// @param [in] order if the block should be delivered in order, for DGRAM only

#ifdef SRT_ENABLE_SRCTIMESTAMP
   void addBuffer(const char* data, int len, int ttl = -1, bool order = false, uint64_t srctime = 0);
#else
   void addBuffer(const char* data, int len, int ttl = -1, bool order = false);
#endif

      /// Read a block of data from file and insert it into the sending list.
      /// @param [in] ifs input file stream.
      /// @param [in] len size of the block.
      /// @return actual size of data added from the file.

   int addBufferFromFile(std::fstream& ifs, int len);

      /// Find data position to pack a DATA packet from the furthest reading point.
      /// @param [out] data the pointer to the data position.
      /// @param [out] msgno message number of the packet.
      /// @param [out] origintime origin time stamp of the message
      /// @param [in] kflags Odd|Even crypto key flag
      /// @return Actual length of data read.

   #if defined(SRT_ENABLE_TSBPD)
   int readData(char** data, int32_t& msgno, uint64_t& origintime, unsigned kflgs);
   #else //defined(SRT_ENABLE_TSBPD)
   int readData(char** data, int32_t& msgno, unsigned kflgs);
   #endif


#if defined(SRT_ENABLE_TSBPD)
      /// Find data position to pack a DATA packet for a retransmission.
      /// @param [out] data the pointer to the data position.
      /// @param [in] offset offset from the last ACK point.
      /// @param [out] msgno message number of the packet.
      /// @param [out] origintime origin time stamp of the message
      /// @param [out] msglen length of the message
      /// @return Actual length of data read.

   int readData(char** data, const int offset, int32_t& msgno, uint64_t& origintime, int& msglen);

#else  /* SRT_ENABLE_TSBPD */
      /// Find data position to pack a DATA packet from the furthest reading point.
      /// @param [out] data the pointer to the data position.
      /// @param [out] msgno message number of the packet.
      /// @return Actual length of data read.

   int readData(char** data, int32_t& msgno);

      /// Find data position to pack a DATA packet for a retransmission.
      /// @param [out] data the pointer to the data position.
      /// @param [in] offset offset from the last ACK point.
      /// @param [out] msgno message number of the packet.
      /// @param [out] msglen length of the message
      /// @return Actual length of data read.

   int readData(char** data, const int offset, int32_t& msgno, int& msglen);

#endif /* SRT_ENABLE_TSBPD */
      /// Update the ACK point and may release/unmap/return the user data according to the flag.
      /// @param [in] offset number of packets acknowledged.

   void ackData(int offset);

      /// Read size of data still in the sending list.
      /// @return Current size of the data in the sending list.

   int getCurrBufSize() const;

#ifdef SRT_ENABLE_TLPKTDROP
   int dropLateData(int &bytes, uint64_t latetime);
#endif

#ifdef SRT_ENABLE_BSTATS
#ifdef SRT_ENABLE_SNDBUFSZ_MAVG
   void updAvgBufSize(uint64_t time);
   int getAvgBufSize(int &bytes, int &timespan);
#endif /* SRT_ENABLE_SNDBUFSZ_MAVG */
   int getCurrBufSize(int &bytes, int &timespan);
#endif /* SRT_ENABLE_BSTATS */

#ifdef SRT_ENABLE_INPUTRATE
   int getInputRate(int& payloadtsz, int& period);
   void updInputRate(uint64_t time, int pkts, int bytes);
   void setInputRateSmpPeriod(int period);
#endif /* SRT_ENABLE_INPUTRATE */ 

private:
   void increase();

private:
   pthread_mutex_t m_BufLock;           // used to synchronize buffer operation

   struct Block
   {
      char* m_pcData;                   // pointer to the data block
      int m_iLength;                    // length of the block

      int32_t m_iMsgNoBitset;                 // message number
      uint64_t m_OriginTime;            // original request time
#ifdef SRT_ENABLE_SRCTIMESTAMP
      uint64_t m_SourceTime;
#endif
      int m_iTTL;                       // time to live (milliseconds)

      Block* m_pNext;                   // next block

      int32_t getMsgSeq()
      {
          // NOTE: this extracts message ID with regard to REXMIT flag.
          // This is valid only for message ID that IS GENERATED in this instance,
          // not provided by the peer. This can be otherwise sent to the peer - it doesn't matter
          // for the peer that it uses LESS bits to represent the message.
          return m_iMsgNoBitset & MSGNO_SEQ::mask;
      }

   } *m_pBlock, *m_pFirstBlock, *m_pCurrBlock, *m_pLastBlock;

   // m_pBlock:         The head pointer
   // m_pFirstBlock:    The first block
   // m_pCurrBlock:	The current block
   // m_pLastBlock:     The last block (if first == last, buffer is empty)

   struct Buffer
   {
      char* m_pcData;                   // buffer
      int m_iSize;                      // size
      Buffer* m_pNext;                  // next buffer
   } *m_pBuffer;                        // physical buffer

   int32_t m_iNextMsgNo;                // next message number

   int m_iSize;                         // buffer size (number of packets)
   int m_iMSS;                          // maximum seqment/packet size

   int m_iCount;                        // number of used blocks

#ifdef SRT_ENABLE_BSTATS
   int m_iBytesCount;                   // number of payload bytes in queue
   uint64_t m_LastOriginTime;

#ifdef SRT_ENABLE_SNDBUFSZ_MAVG
   uint64_t m_LastSamplingTime;
   int m_iCountMAvg;
   int m_iBytesCountMAvg;
   int m_TimespanMAvg;
#endif /* SRT_ENABLE_SNDBUFSZ_MAVG */
#endif /* SRT_ENABLE_BSTATS */

#ifdef SRT_ENABLE_INPUTRATE
   int m_iInRatePktsCount;  // number of payload bytes added since InRateStartTime
   int m_iInRateBytesCount;  // number of payload bytes added since InRateStartTime
   uint64_t m_InRateStartTime;
   uint64_t m_InRatePeriod; // usec
   int m_iInRateBps;        // Input Rate in Bytes/sec
   int m_iAvgPayloadSz;     // Average packet payload size
#endif /* SRT_ENABLE_INPUTRATE */ 

private:
   CSndBuffer(const CSndBuffer&);
   CSndBuffer& operator=(const CSndBuffer&);
};

////////////////////////////////////////////////////////////////////////////////


class CRcvBuffer
{
public:

   // XXX There's currently no way to access the socket ID set for
   // whatever the queue is currently working for. Required to find
   // some way to do this, possibly by having a "reverse pointer".
   // Currently just "unimplemented".
   std::string CONID() const { return ""; }

   CRcvBuffer(CUnitQueue* queue, int bufsize = 65536);
   ~CRcvBuffer();

      /// Write data into the buffer.
      /// @param [in] unit pointer to a data unit containing new packet
      /// @param [in] offset offset from last ACK point.
      /// @return 0 is success, -1 if data is repeated.

   int addData(CUnit* unit, int offset);

      /// Read data into a user buffer.
      /// @param [in] data pointer to user buffer.
      /// @param [in] len length of user buffer.
      /// @return size of data read.

   int readBuffer(char* data, int len);

      /// Read data directly into file.
      /// @param [in] file C++ file stream.
      /// @param [in] len expected length of data to write into the file.
      /// @return size of data read.

   int readBufferToFile(std::fstream& ofs, int len);

      /// Update the ACK point of the buffer.
      /// @param [in] len size of data to be acknowledged.
      /// @return 1 if a user buffer is fulfilled, otherwise 0.

   void ackData(int len);

      /// Query how many buffer space left for data receiving.
      /// @return size of available buffer space (including user buffer) for data receiving.

   int getAvailBufSize() const;

      /// Query how many data has been continuously received (for reading) and ready to play (tsbpdtime < now).
      /// @param [out] tsbpdtime localtime-based (uSec) packet time stamp including buffering delay
      /// @return size of valid (continous) data for reading.

   int getRcvDataSize() const;

#ifdef SRT_ENABLE_BSTATS
      /// Query how many data was received and acknowledged.
      /// @param bytes [out] bytes
      /// @param spantime [out] spantime
      /// @return size in pkts of acked data.

   int getRcvDataSize(int &bytes, int &spantime);
#if SRT_ENABLE_RCVBUFSZ_MAVG

      /// Query a 1 sec moving average of how many data was received and acknowledged.
      /// @param bytes [out] bytes
      /// @param spantime [out] spantime
      /// @return size in pkts of acked data.

   int getRcvAvgDataSize(int &bytes, int &spantime);

      /// Query how many data of the receive buffer is acknowledged.
      /// @param now [in] current time in us.
      /// @return none.

   void updRcvAvgDataSize(uint64_t now);
#endif /* SRT_ENABLE_RCVBUFSZ_MAVG */

      /// Query the received average payload size.
      /// @return size (bytes) of payload size

   int getRcvAvgPayloadSize() const;
#endif /* SRT_ENABLE_BSTATS */


      /// Mark the message to be dropped from the message list.
      /// @param [in] msgno message number.
      /// @param [in] using_rexmit_flag whether the MSGNO field uses rexmit flag (if not, one more bit is part of the msgno value)

   void dropMsg(int32_t msgno, bool using_rexmit_flag);

      /// read a message.
      /// @param [out] data buffer to write the message into.
      /// @param [in] len size of the buffer.
      /// @return actuall size of data read.

   int readMsg(char* data, int len);

#ifdef SRT_ENABLE_TSBPD
      /// read a message.
      /// @param [out] data buffer to write the message into.
      /// @param [in] len size of the buffer.
      /// @param [out] tsbpdtime localtime-based (uSec) packet time stamp including buffering delay
      /// @return actuall size of data read.

   int readMsg(char* data, int len, uint64_t& tsbpdtime);

      /// Query how many messages are available now.
      /// @param [out] tsbpdtime localtime-based (uSec) packet time stamp including buffering delay
      /// @return number of messages available for recvmsg.

      // XXX This seems to be a little bit kind-of dissonance
      // between the intended code and the reality. This function
      // had to return the number of messages available for processing.
      // However it turned out that actually:
      //  - no one is interested with how many messages are available
      //  - stating that "none" or "any" suffices for all cases
      //  - so getRcvMsgNum has more-less always the same definition as below
      // Which means that there's no difference between
      // (getRcvMsgNum() > 0) and (isRcvDataReady()) expression.
      // These functions are now blocked as it doesn't make sense completely.

      // int getRcvMsgNum(uint64_t& tsbpdtime) { return isRcvDataReady(tsbpdtime)? 1 : 0; }
      // int getRcvMsgNum() { return isRcvDataReady()? 1 : 0; }

      /// Query if data is ready to read (tsbpdtime <= now if TsbPD is active).
      /// @param [out] tsbpdtime localtime-based (uSec) packet time stamp including buffering delay
      ///                        of next packet in recv buffer, ready or not.
      /// @return true if ready to play, false otherwise (tsbpdtime may be !0 in
      /// both cases).

   bool isRcvDataReady(uint64_t& tsbpdtime, CPacket** pppkt = 0);
   bool isRcvDataReady();
   bool isRcvDataAvailable()
   {
       return m_iLastAckPos != m_iStartPos;
   }
   CPacket* getRcvReadyPacket();
   bool isReadyToPlay(const CPacket* p, uint64_t& tsbpdtime);

      ///    Set TimeStamp-Based Packet Delivery Rx Mode
      ///    @param [in] timebase localtime base (uSec) of packet time stamps including buffering delay
      ///    @param [in] delay aggreed TsbPD delay
      /// @return 0

   int setRcvTsbPdMode(uint64_t timebase, uint32_t delay);

      /// Add packet timestamp for drift caclculation and compensation
      /// @param [in] timestamp packet time stamp

   void addRcvTsbPdDriftSample(uint32_t timestamp);

#ifdef SRT_DEBUG_TSBPD_DRIFT
   void printDriftHistogram(int64_t iDrift);
   void printDriftOffset(int tsbPdOffset, int tsbPdDriftAvg);
#endif

#ifdef SRT_ENABLE_TLPKTDROP
      /// Get information on the 1st message in queue.
      // Parameters (of the 1st packet queue, ready to play or not):
      /// @param tsbpdtime [out] localtime-based (uSec) packet time stamp including buffering delay of 1st packet or 0 if none
      /// @param passack [out] true if 1st ready packet is not yet acknowleged
      /// @param skipseqno [out] -1 or seq number of 1st unacknowledged pkt ready to play preceeded by missing packets.
      /// @retval true 1st packet ready to play (tsbpdtime <= now). Not yet acknowledged if passack == true
      /// @retval false IF tsbpdtime = 0: rcv buffer empty; ELSE:
      ///                   IF skipseqno != -1, packet ready to play preceeded by missing packets.;
      ///                   IF skipseqno == -1, no missing packet but 1st not ready to play.


   bool getRcvFirstMsg(uint64_t& tsbpdtime, bool& passack, int32_t& skipseqno, CPacket** pppkt=0);

      /// Update the ACK point of the buffer.
      /// @param len [in] size of data to be skip & acknowledged.

   void skipData(int len);

#endif /* SRT_ENABLE_TLPKTDROP */

private:
      /// Adjust receive queue to 1st ready to play message (tsbpdtime < now).
      // Parameters (of the 1st packet queue, ready to play or not):
      /// @param tsbpdtime [out] localtime-based (uSec) packet time stamp including buffering delay of 1st packet or 0 if none
      /// @retval true 1st packet ready to play without discontinuity (no hole)
      /// @retval false tsbpdtime = 0: no packet ready to play


   bool getRcvReadyMsg(uint64_t& tsbpdtime, CPacket** pppkt = 0);

      /// Get packet delivery local time base (adjusted for wrap around)
      /// @param timestamp [in] packet timestamp (relative to peer StartTime), wrapping around every ~72 min
      /// @return local delivery time (usec)

   uint64_t getTsbPdTimeBase(uint32_t timestamp);

      /// Get packet local delivery time
      /// @param [in] timestamp packet timestamp (relative to peer StartTime), wrapping around every ~72 min
      /// @return local delivery time (usec)

public:
   uint64_t getPktTsbPdTime(uint32_t timestamp);
private:

#else  /* SRT_ENABLE_TSBPD */

      /// Query how many messages are available now.
      /// @return number of messages available for recvmsg.

   int getRcvMsgNum()
   {
       int p, q;
       bool passack;

       return scanMsg(p, q, passack) ? 1 : 0;
   }

   bool isRcvDataReady() const;
#endif /* SRT_ENABLE_TSBPD */

#ifdef SRT_ENABLE_BSTATS
   /// thread safe bytes counter
   /// @param bytes [in] number of bytes added/delete (if negative) to/from rcv buffer.
   // XXX Please document.

   void countBytes(int pkts, int bytes, bool acked = false);
#endif /* SRT_ENABLE_BSTATS */

private:
   bool scanMsg(int& start, int& end, bool& passack);

private:
   CUnit** m_pUnit;                     // pointer to the protocol buffer
   int m_iSize;                         // size of the protocol buffer
   CUnitQueue* m_pUnitQueue;		// the shared unit queue

   int m_iStartPos;                     // the head position for I/O (inclusive)
   int m_iLastAckPos;                   // the last ACKed position (exclusive)
					// EMPTY: m_iStartPos = m_iLastAckPos   FULL: m_iStartPos = m_iLastAckPos + 1
   int m_iMaxPos;			// the furthest data position

   int m_iNotch;			// the starting read point of the first unit

#ifdef SRT_ENABLE_BSTATS
   pthread_mutex_t m_BytesCountLock;    // used to protect counters operations
   int m_iBytesCount;                   // Number of payload bytes in the buffer
   int m_iAckedPktsCount;               // Number of acknowledged pkts in the buffer
   int m_iAckedBytesCount;              // Number of acknowledged payload bytes in the buffer
   int m_iAvgPayloadSz;                 // Average payload size for dropped bytes estimation
#endif /* SRT_ENABLE_BSTATS */

#ifdef SRT_ENABLE_TSBPD
   bool m_bTsbPdMode;                   // true: apply TimeStamp-Based Rx Mode
   uint32_t m_uTsbPdDelay;              // aggreed delay
   uint64_t m_ullTsbPdTimeBase;         // localtime base for TsbPd mode
   // Note: m_ullTsbPdTimeBase cumulates values from:
   // 1. Initial SRT_CMD_HSREQ packet returned value diff to current time:
   //    == (NOW - PACKET_TIMESTAMP), at the time of HSREQ reception
   // 2. Timestamp overflow (@c CRcvBuffer::getTsbPdTimeBase), when overflow on packet detected
   //    += CPacket::MAX_TIMESTAMP+1 (it's a hex round value, usually 0x1*e8).
   // 3. Time drift (CRcvBuffer::addRcvTsbPdDriftSample, executed exclusively
   //    from UMSG_ACKACK handler). This is updated with (positive or negative) TSBPD_DRIFT_MAX_VALUE
   //    once the value of average drift exceeds this value in whatever direction.
   //    += (+/-)CRcvBuffer::TSBPD_DRIFT_MAX_VALUE
   //
   // XXX Application-supplied timestamps won't work therefore. This requires separate
   // calculation of all these things above.

   bool m_bTsbPdWrapCheck;              // true: check packet time stamp wrap around
   static const uint32_t TSBPD_WRAP_PERIOD = (30*1000000);    //30 seconds (in usec)

   static const int TSBPD_DRIFT_MAX_VALUE = 5000;   // Max drift (usec) above which TsbPD Time Offset is adjusted
   static const int TSBPD_DRIFT_MAX_SAMPLES = 1000;  // Number of samples (UMSG_ACKACK packets) to perform drift caclulation and compensation
   //int m_iTsbPdDrift;                           // recent drift in the packet time stamp
   //int64_t m_TsbPdDriftSum;                     // Sum of sampled drift
   //int m_iTsbPdDriftNbSamples;                  // Number of samples in sum and histogram
   DriftTracer<TSBPD_DRIFT_MAX_SAMPLES, TSBPD_DRIFT_MAX_VALUE> m_DriftTracer;
#ifdef SRT_ENABLE_RCVBUFSZ_MAVG
   uint64_t m_LastSamplingTime;
   int m_TimespanMAvg;
   int m_iCountMAvg;
   int m_iBytesCountMAvg;
#endif /* SRT_ENABLE_RCVBUFSZ_MAVG */
#ifdef SRT_DEBUG_TSBPD_DRIFT
   int m_TsbPdDriftHisto100us[22];              // Histogram of 100us TsbPD drift (-1.0 .. +1.0 ms in 0.1ms increment)
   int m_TsbPdDriftHisto1ms[22];                // Histogram of TsbPD drift (-10.0 .. +10.0 ms, in 1.0 ms increment)
   static const int TSBPD_DRIFT_PRT_SAMPLES = 200;    // Number of samples (UMSG_ACKACK packets) to print hostogram
#endif /* SRT_DEBUG_TSBPD_DRIFT */

#ifdef SRT_DEBUG_TSBPD_OUTJITTER
   unsigned long m_ulPdHisto[4][10];
#endif /* SRT_DEBUG_TSBPD_OUTJITTER */
#endif /* SRT_ENABLE_TSBPD */

private:
   CRcvBuffer();
   CRcvBuffer(const CRcvBuffer&);
   CRcvBuffer& operator=(const CRcvBuffer&);
};


#endif
