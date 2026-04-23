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

#include <stdio.h>
#include "ip.h"
#include "dsred.h"
#include "random.h"
#include "dsredq.h"


/*------------------------------------------------------------------------------
redQueue() Constructor.
     Initializes virtual queue parameters.
------------------------------------------------------------------------------*/
redQueue::redQueue() {

	numPrec = MAX_PREC;
	qlen=0;
	qMaxBur=0;
	mredMode = rio_c;
	//underlying physical queue
	q_ = new PacketQueue();	

}


/*------------------------------------------------------------------------------
void config(int prec, const char*const* argv)
    Configures a virtual queue according to values supplied by the user.
------------------------------------------------------------------------------*/
void redQueue::config(int prec, int argc, const char*const* argv) {

	if ( mredMode == dropTail ) {
		qParam_[prec].edp_.th_min =  (int)strtol(argv[4], NULL, 10);
		return;
	}

//	qParam_[prec].qlen = 0;
        if (argc > 4) {
		qParam_[prec].edp_.th_min = (int)strtod(argv[4], NULL);
		qParam_[prec].edp_.th_max = (int)strtod(argv[5], NULL);
		qParam_[prec].edp_.max_p_inv = 1.0 / atof(argv[6]);
        }
	qParam_[prec].edp_.q_w = 0.002;
  //      double init_v_ave = 0.0;
//	qParam_[prec].edv_.v_ave = init_v_ave;
//	qParam_[prec].idle_ = 1;
	if (&Scheduler::instance() != NULL)
		qParam_[prec].idletime_ = Scheduler::instance().clock();
	else
		qParam_[prec].idletime_ = 0.0;
}



/*------------------------------------------------------------------------------
void initREDStateVar(void)
    Initializes each virtual queue in one physical queue.
------------------------------------------------------------------------------*/
void redQueue::initREDStateVar(void) {
	for (int i = 0; i < numPrec; i++) {
		qParam_[i].idle_ = 1;

		if (&Scheduler::instance() != NULL)
			qParam_[i].idletime_ = Scheduler::instance().clock();
		else
			qParam_[i].idletime_ = 0.0;
	}
}


/*------------------------------------------------------------------------------
void updateREDStateVar(int prec)
    Updates a virtual queue's state variables after dequing.
------------------------------------------------------------------------------*/
void redQueue::updateREDStateVar(int prec) {
   int idle = 1;
   int i;

   double now = Scheduler::instance().clock();

   qParam_[prec].qlen--;		// decrement virtual queue length
			
   if (qParam_[prec].qlen == 0)
   {
      if (mredMode == rio_c) {
         for(i=0; i<prec; i++) if (qParam_[i].qlen != 0) idle = 0;
         if (idle) {
            for (i=prec;i<numPrec;i++) {
               if (qParam_[i].qlen == 0) {
                  qParam_[i].idle_ = 1;
                  qParam_[i].idletime_ = now;
               } else break;
            }			
         }
      } else if (mredMode == rio_d) {
         qParam_[prec].idle_ = 1;
         qParam_[prec].idletime_ = now;
      } else if (mredMode == wred) { //wred
         qParam_[0].idle_ = 1;
         qParam_[0].idletime_ = now;
      }			
   }
}


/*------------------------------------------------------------------------------
void enque(Packet *pkt, int prec, int ecn)
    Enques a packet associated with one of the precedence levels of the
physical queue.
------------------------------------------------------------------------------*/
int redQueue::enque(Packet *pkt, int prec, int ecn) {
   int m = 0;
   double now, u;
	double pa,pb;

   if ((mredMode == dropTail)&&(qParam_[prec].edp_.th_min==-1))  // to avoid in some case to get PKT_DROPPED
		return PKT_EDROPPED;  				 // it would be meaningless!
											

   if (q_->length() > (qlim-1)) return PKT_DROPPED;

   now = Scheduler::instance().clock();

   //now determining the avg for that queue
	if (mredMode == dropTail) {
		//printf("prec: %d limit: %d\n",prec,qParam_[prec].edp_.th_min);
		//if (qParam_[prec].edp_.th_min==-1) return PKT_EDROPPED;
		if (q_->length() >= qParam_[0].edp_.th_min) {
			return PKT_EDROPPED;
		} else {
		   q_->enque(pkt);
	      	   qlen++; 
		   qParam_[prec].qlen++;
		   qMaxBur=(qMaxBur>qlen?qMaxBur:qlen);
	      return PKT_ENQUEUED;		
		}
	} else if (mredMode == rio_c) {
      for (int i = prec; i < numPrec; i++) {	
         m = 0;
         if (qParam_[i].idle_) {
            qParam_[i].idle_ = 0;
            m = int(qParam_[i].edp_.ptc * (now - qParam_[i].idletime_));
         }
         calcAvg(i, m+1);
      }
   } else if (mredMode == rio_d) {
      if (qParam_[prec].idle_) {
         qParam_[prec].idle_ = 0;
         m = int(qParam_[prec].edp_.ptc * (now - qParam_[prec].idletime_));
      }	
      calcAvg(prec, m+1);
   } else { //wred
      if (qParam_[0].idle_) {
         qParam_[0].idle_ = 0;
         m = int(qParam_[0].edp_.ptc * (now - qParam_[0].idletime_));
      }	
      calcAvg(0, m+1);
   }


   // enqueu packet if we are using ecn
   if (ecn) {

      q_->enque(pkt);	
      qlen++;
      qMaxBur=(qMaxBur>qlen?qMaxBur:qlen);
      //virtually, this new packet is queued in one of the multiple queues,
      //thus increasing the length of that virtual queue
      qParam_[prec].qlen++;
   }


   //if the avg is greater than the min threshold,
   //there can be only two cases.....
   if (qParam_[prec].edv_.v_ave > qParam_[prec].edp_.th_min) {
      //either the avg is less than the max threshold
      if (qParam_[prec].edv_.v_ave <= qParam_[prec].edp_.th_max) {
         //in which case determine the probabilty for dropping the packet,
				
			qParam_[prec].edv_.count++;
         qParam_[prec].edv_.v_prob = (1/qParam_[prec].edp_.max_p_inv) *
                                     (qParam_[prec].edv_.v_ave-qParam_[prec].edp_.th_min) /
                                     (qParam_[prec].edp_.th_max-qParam_[prec].edp_.th_min);

         pb = qParam_[prec].edv_.v_prob;
			pa = pb/(1.0 - qParam_[prec].edv_.count*pb);
         //now determining whether to drop the packet or not
         u = Random::uniform(0.0, 1.0);

         //drop it
			if (u <= pa) {	
            if (ecn) return PKT_MARKED;
            return PKT_EDROPPED;
         }
      } else { //if avg queue is greater than max. threshold
			qParam_[prec].edv_.count = 0;
         if (ecn) return PKT_MARKED;
         		return PKT_EDROPPED;
      }
   }
	qParam_[prec].edv_.count = -1;

   // if ecn is on, then the packet has already been enqueued
   if(ecn) return PKT_ENQUEUED;

   //if the packet survives the above conditions it
   //is finally queued in the underlying queue
   q_->enque(pkt);

   //virtually, this new packet is queued in one of the multiple queues,
   //thus increasing the length of that virtual queue
   qParam_[prec].qlen++;
   qlen++;
   qMaxBur=(qMaxBur>qlen?qMaxBur:qlen);
   return PKT_ENQUEUED;
}


/*------------------------------------------------------------------------------
]Packet* deque()
    Deques a packet from the physical queue.
------------------------------------------------------------------------------*/
Packet* redQueue::deque() {
	qlen--;
	return(q_->deque());
}


/*------------------------------------------------------------------------------
void calcAvg(int prec, int m)
    This method calculates avg queue length, given the prec number, m (a value
used to adjust the queue size appropriately during idle times).
    If mredMode is rio_c, each virtual queue size is calculated
independently.  If it is true, the calculated size of queue n includes the sizes
of all virtual queues up to and including n.
------------------------------------------------------------------------------*/
void redQueue::calcAvg(int prec, int m) {
	float f;
	int i;

	f = qParam_[prec].edv_.v_ave;

	while (--m >= 1) {
		f *= 1.0 - qParam_[prec].edp_.q_w;
	}
	f *= 1.0 - qParam_[prec].edp_.q_w;

	if (mredMode == rio_c)
		for (i = 0; i <= prec; i ++)
			f += qParam_[i].edp_.q_w * qParam_[i].qlen;
	else if (mredMode == rio_d)
		f += qParam_[prec].edp_.q_w * qParam_[prec].qlen;
   else //wred
      f += qParam_[prec].edp_.q_w * q_->length();
		
   if (mredMode == wred)
      for (i = 0; i < numPrec; i ++)
			qParam_[i].edv_.v_ave = f;
   else //rio_c, rio_d
      qParam_[prec].edv_.v_ave = f;
}


/*------------------------------------------------------------------------------
double getWeightedLength()
    Returns the weighted RED queue length for the entire physical queue, in
packets.
------------------------------------------------------------------------------*/
double redQueue::getWeightedLength() {
	double sum = 0.0;

	if (mredMode == rio_c)
		return qParam_[numPrec-1].edv_.v_ave;
	else {
		for (int prec = 0; prec < numPrec; prec++)
			sum += qParam_[prec].edv_.v_ave;
		return(sum);
	}
}


/*------------------------------------------------------------------------------
int getRealLength(void)
    Returns the length of the physical queue, in packets.
------------------------------------------------------------------------------*/
int redQueue::getRealLength(void) {
	return(q_->length());
}


/* only for debugging*/
void redQueue::getPTC() {
	printf("PTC %f\n",qParam_[0].edp_.ptc);
	printf("PTC %f\n",qParam_[1].edp_.ptc);
	printf("PTC %f\n",qParam_[2].edp_.ptc);

}


/*------------------------------------------------------------------------------
void setPTC(int outLinkBW)
    Sets the packet time constant, given the outgoing link bandwidth from the
router.
------------------------------------------------------------------------------*/
void redQueue::setPTC(double outLinkBW) {
	for (int i = 0; i < MAX_PREC; i++)
	{	qParam_[i].edp_.ptc = outLinkBW/(8.0*qParam_[i].edp_.mean_pktsize);
		//printf("PTC per %d = %f\n",i,qParam_[i].edp_.ptc);
}
}


/*------------------------------------------------------------------------------
void setMPS(int mps)
    Sets the mean packet size for each of the virtual queues.
------------------------------------------------------------------------------*/
void redQueue::setMPS(int mps) {
	for (int i = 0; i < MAX_PREC; i++)
		qParam_[i].edp_.mean_pktsize = mps;
}

