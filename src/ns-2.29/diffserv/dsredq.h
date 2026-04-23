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

#ifndef dsredq_h
#define dsredq_h

#include "dsconsts.h"

enum mredModeType {rio_c, rio_d, wred, dropTail};


/*------------------------------------------------------------------------------
struct qParam
    This structure specifies the parameters needed to be maintained for each
RED queue.
------------------------------------------------------------------------------*/
struct qParam {
	edp edp_;		// early drop parameters (see red.h)
	edv edv_;		// early drop variables (see red.h)
	int qlen;		// actual (not weighted) queue length in packets
	double idletime_;	// needed to calculate avg queue
	bool idle_;		// needed to calculate avg queue
        qParam() : qlen(0), idle_(1){
       		qlen  = 0;
        	idle_ = 1;
		edv_.v_ave = 0.0;

 		 
        }
};

/*------------------------------------------------------------------------------
class redQueue
    This class provides specs for one physical queue.
------------------------------------------------------------------------------*/
class redQueue {
public:
	int numPrec;	// the current number of precedence levels (or virtual queues)
	int qlim;
	int qlen;
	int qMaxBur;    // Maximum Burstiness: maximum experienced number of packets stored in the queue 
	mredModeType mredMode;
	redQueue();
	void config(int prec, int argc, const char*const* argv);	// configures one virtual RED queue
	void initREDStateVar(void);		// initializes RED state variables
	void updateREDStateVar(int prec);	// updates RED variables after dequing a packet
        // enques packets into a physical queue
	int enque(Packet *pkt, int prec, int ecn);
	Packet* deque(void);			// deques packets
	double getWeightedLength();
	int getRealLength(void);		// queries length of a physical queue
	int getVirtQueueLen(int prec) {return qParam_[prec].qlen;};	// queries length of a virtual queue
        // sets packet time constant values
        //(needed for calc. avgQSize) for each virtual queue
	void setPTC(double outLinkBW);
	void getPTC();
        // sets mean packet size (needed to calculate avg. queue size)
	void setMPS(int mps);


private:
	PacketQueue *q_;		// underlying FIFO queue
        // used to maintain parameters for each of the virtual queues
	qParam qParam_[MAX_PREC];
	void calcAvg(int prec, int m); // to calculate avg. queue size of a virtual queue

};
#endif

