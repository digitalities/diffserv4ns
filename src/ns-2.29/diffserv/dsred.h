/* Copyright (C) 2001-2006  Sergio Andreozzi
 *
 * This file is part of DiffServ4NS, a set of improvements to
 * the Network Simulator 2 for DiffServ simulations.
 *
 * Project Homepage: http://diffserv4ns.sourceforge.net/
 *
 * DiffServ4NS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * DiffServ4NS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DiffServ4NS; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 * GNU License: http://www.gnu.org/licenses/gpl.txt
 * 
 * The above copyright applies to the following changes and additions to the official
 * NS2 (http://nsnam.cvs.sourceforge.net/nsnam/ns-2/):
 *
 *  - marking: possibility to define mark rules based on source node, 
 *             destination node, transport protocol type and application type
 *  - new schedulers: WFQ, WF2Q+, SCFQ, SFQ, LLQ
 *  - new policy: possibility to define a DSCP based rate limiter
 *  - new monitoring possibilities:
 *     - For UDP-based traffic
 *        + Average, instantaneous, minimum and frequency distributed OWD
 *        + Average, instantaneous, minimum and frequency distributed IPDV
 *     - For TCP-based traffic
 *        + TCP Goodput on a DSCP basis
 *        + TCP Round-Trip Time on a DSCP basis, both instantaneous value 
 *          and frequency distribution
 *        + TCP Window Size on a DSCP basis, both instantaneous value 
 *          and frequency distribution
 *     - per-hop parameters:
 *        + Instantaneous and average queue length on a queue basis 
 *          or on a queue and drop precedence level basis
 *        + Maximum burstiness for queue 0
 *        + Departure rate on a queue basis or on a queue and drop level 
 *          precedence basis
 *        + Received packets, transmitted packets, dropped packets due to droppers 
 *          and dropped packets due to buffer overflow, 
 *          all on a DSCP basis and for both absolute and percentage values
 *
 *************************************************************************************** 
 */
/*
 * Copyright (c) 2000 Nortel Networks
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Nortel Networks.
 * 4. The name of the Nortel Networks may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORTEL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NORTEL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Developed by: Farhan Shallwani, Jeremy Ethridge
 *               Peter Pieda, and Mandeep Baines
 * Maintainer: Peter Pieda <ppieda@nortelnetworks.com>
 */

/*
 * dsred.h
 *
 * The Positions of dsREDQueue, edgeQueue, and coreQueue in the Object Hierarchy.
 *
 * This class, i.e. "dsREDQueue", is positioned in the class hierarchy as follows:
 *
 *             Queue
 *               |
 *           dsREDQueue
 *
 *
 *   This class stands for "Differentiated Services RED Queue".  Since the
 * original RED does not support multiple parameters, and other functionality
 * needed by a RED gateway in a Diffserv architecture, this class was created to
 * support the desired functionality.  This class is then inherited by two more
 * classes, moulding the old hierarchy as follows:
 *
 *
 *             Queue
 *               |
 *           dsREDQueue
 *           |        |
 *     edgeQueue    coreQueue
 *
 *
 * These child classes correspond to the "edge" and "core" routers in a Diffserv
 * architecture.
 *
 */


#ifndef dsred_h
#define dsred_h

#include <vector>
#include "red.h"	// need RED class specs (edp definition, for example)
#include "queue.h"	// need Queue class specs
#include "dsredq.h"
#include "dsconsts.h"
#include "dsscheduler.h"



class dsFD {
public: 
        dsFD(double RL, char* FN);
        void occurency(double Value); 
	void flush(double norm, double threshold);
private:
  	vector<int> FDtable;		
	double RangeLenght;	
	char FileName[20];
	int counter;
};




/*------------------------------------------------------------------------------
struct phbParam
    This struct is used to maintain entries for the PHB parameter table, used 
to map a code point to a physical queue-virtual queue pair.
------------------------------------------------------------------------------*/
struct phbParam {
   int codePt_;
   int queue_;	// physical queue
   int prec_;	// virtual queue (drop precedence)
};

struct statType {
	int drops;             // per queue stats
	int edrops;
	int pkts;
	int valid_CP[MAX_CP];  // per CP stats
	int drops_CP[MAX_CP];
	int edrops_CP[MAX_CP];
	int pkts_CP[MAX_CP];
	int     TCPnReTX_CP[MAX_CP];
	double  TCPbReTX_CP[MAX_CP];
	double  TCPbGoTX_CP[MAX_CP];
	int     TCPcwnd_CP[MAX_CP];
	double  TCPrtt_CP[MAX_CP];
	dsFD*   TCPrttFD_CP[MAX_CP];
	dsFD*   TCPcwndFD_CP[MAX_CP];
};

class QueueScheduler;

/*------------------------------------------------------------------------------
class dsREDQueue 
    This class specifies the characteristics for a Diffserv RED router.
------------------------------------------------------------------------------*/
class dsREDQueue : public Queue {
public:	
	dsREDQueue();
	int command(int argc, const char*const* argv);	// interface to ns scripts

protected:
	dsScheduler* QScheduler;
	redQueue redq_[MAX_QUEUES];	// the physical queues at the router
	NsObject* de_drop_;		// drop_early target
	statType stats; // used for statistics gatherings
	int qToDq;			// current queue to be dequeued in a round robin manner
	int numQueues_;			// the number of physical queues at the router
	phbParam phb_[MAX_CP];		// PHB table
	int phbEntries;     		// the current number of entries in the PHB table
	int ecn_;			// used for ECN (Explicit Congestion Notification)
	LinkDelay* link_;		// outgoing link
	int phisQueueLimit[MAX_QUEUES];
	int PacketSize_;
	int DSCP_;
	void reset();
	void edrop(Packet* p); // used so flowmonitor can monitor early drops
	void enque(Packet *pkt); // enques a packet
	Packet *deque(void);	// deques a packet
	int getCodePt(Packet *p); // given a packet, extract the code point marking from its header field
	void lookupPHBTable(int codePt, int* queue, int* prec); // looks up queue and prec numbers corresponding to a code point
	void addPHBEntry(int codePt, int queue, int prec); // edits phb entry in the table
	void setNumPrec(int queue, int numPrec);
	void setMREDMode(const char* mode, const char* queue);
	void printStats(); // print various stats
	double getStat(int argc, const char*const* argv);
	void printPHBTable();  // print the PHB table
   	void setSchedularMode(int argc, const char*const* argv);
   	void addQueueWeight(int queueNum, double weight); 
   	void addQueueRate(int queueNum, int rate); 
	void setQSize(int queueNum, int qMaxSize) ;
	void FDinit(const char *const  DSCP);
};

#endif
