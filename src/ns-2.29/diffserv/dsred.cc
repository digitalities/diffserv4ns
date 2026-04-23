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
#include <stdlib.h>
#include <fstream>
#include "ip.h"
#include "dsred.h"
#include "delay.h"
#include "random.h"
#include "flags.h"
#include "tcp.h"
#include "dsredq.h"


/*------------------------------------------------------------------------------
dsREDClass declaration. 
    Links the new class in the TCL heirarchy.  See "Notes And Documentation for 
ns-2."
------------------------------------------------------------------------------*/
static class dsREDClass : public TclClass {
public:
	dsREDClass() : TclClass("Queue/dsRED") {}
	TclObject* create(int, const char*const*) {
		return (new dsREDQueue);
	}
} class_dsred;


/*------------------------------------------------------------------------------
dsREDQueue() Constructor.
    Initializes the queue.  Note that the default value assigned to numQueues 
in tcl/lib/ns-default.tcl must be no greater than MAX_QUEUES (the physical 
queue array size).
------------------------------------------------------------------------------*/
dsREDQueue::dsREDQueue() : de_drop_(NULL), link_(NULL)   {

	
	bind("numQueues_", &numQueues_);
	bind_bool("ecn_", &ecn_);

	bind("PacketSize_",   &PacketSize_);
	bind("DSCP_",         &DSCP_);
	bind("MBS0_",         &redq_[0].qMaxBur); 

	int i;

	for(i=0;i<MAX_CP;i++){
		stats.TCPrttFD_CP[i]	= NULL;
		stats.TCPcwndFD_CP[i]	= NULL;
   }
		
	

	numQueues_=1; 
   	QScheduler = new dsRR(numQueues_);
   	for(i=0;i<MAX_QUEUES;i++) phisQueueLimit[i]=0;
   	phbEntries = 0;		// Number of entries in PHB table
	if (link_)
		for (int i = 0; i < MAX_QUEUES; i++)
			redq_[i].setPTC(link_->bandwidth());
	reset();
   	
}


void dsREDQueue::reset() {
	int i;

	QScheduler->Reset();
	stats.drops = 0;
	stats.edrops = 0;
	stats.pkts = 0;

	for(i=0;i<MAX_CP;i++){
		stats.drops_CP[i]	= 0;
		stats.edrops_CP[i]	= 0;
		stats.pkts_CP[i]	= 0;
		stats.TCPnReTX_CP[i]	= 0;
		stats.TCPbReTX_CP[i]	= 0;
		stats.TCPbGoTX_CP[i]	= 0;
		stats.TCPcwnd_CP[i]	= 0;
//		stats.TCPrttFD_CP[i]	= NULL;
//		stats.TCPcwndFD_CP[i]	= NULL;
	}

	for (i = 0; i < MAX_QUEUES; i++)
		redq_[i].qlim = ((phisQueueLimit[i]==0)?limit():phisQueueLimit[i]);
	
	// Compute the "packet time constant" if we know the
	// link bandwidth.  The ptc is the max number of (avg sized)
	// pkts per second which can be placed on the link.
	Queue::reset();
}


/*------------------------------------------------------------------------------
void edrop(Packet* pkt)
    This method is used so that flowmonitor can monitor early drops.
------------------------------------------------------------------------------*/
void dsREDQueue::edrop(Packet* p)
{
	if (de_drop_ != 0) de_drop_->recv(p);
	else drop(p);
}


/*------------------------------------------------------------------------------
void enque(Packet* pkt) 
    The following method outlines the enquing mechanism for a Diffserv router.
This method is not used by the inheriting classes; it only serves as an 
outline.
------------------------------------------------------------------------------*/
void dsREDQueue::enque(Packet* pkt) {
   int codePt, queue, prec;
   hdr_ip* iph = hdr_ip::access(pkt);
   hdr_cmn* cmn = hdr_cmn::access(pkt); 
   //packet_t ptype=cmn->ptype(), aptype=cmn->app_type();
   codePt = iph->prio();	//extracting the marking done by the edge router
   int ecn = 0;
   int enqueued=0;	

   //looking up queue and prec numbers for that codept
   lookupPHBTable(codePt, &queue, &prec);	

   // code added for ECN support
   //hdr_flags* hf = (hdr_flags*)(pkt->access(off_flags_));
   // Changed for the latest version instead of 2.1b6
   hdr_flags* hf = hdr_flags::access(pkt);

   if (ecn_ && hf->ect()) ecn = 1;

	stats.pkts_CP[codePt]++;
	stats.pkts++;

   switch(redq_[queue].enque(pkt, prec, ecn)) {
      case PKT_ENQUEUED:
	 enqueued=1;
         break;
      case PKT_DROPPED:
         stats.drops_CP[codePt]++;
	 stats.drops++;
         drop(pkt);
         break;
      case PKT_EDROPPED:
	 stats.edrops_CP[codePt]++;
	 stats.edrops++;
         edrop(pkt);
         break;
      case PKT_MARKED:
	 enqueued=1;
         hf->ce() = 1; 	// mark Congestion Experienced bit		
         break;			
      default:
         break;
   }

  if (enqueued==1) {QScheduler->EnqueEvent(pkt, queue); 
  	PacketSize_ = cmn->size();
	DSCP_= codePt;
	if (cmn->ptype()==PT_TCP) {		
		hdr_tcp *tcp=hdr_tcp::access(pkt);
		stats.TCPcwnd_CP[DSCP_]=tcp->cwnd();
		stats.TCPrtt_CP[DSCP_]=tcp->t_rtt();

		if (stats.TCPrttFD_CP[DSCP_]!=NULL) {
			if (stats.TCPrtt_CP[DSCP_]>0) 
				 stats.TCPrttFD_CP[DSCP_]->occurency(stats.TCPrtt_CP[DSCP_]);
				 stats.TCPcwndFD_CP[DSCP_]->occurency(stats.TCPcwnd_CP[DSCP_]);
			}

			if (tcp->reason()!=0) { 
				stats.TCPnReTX_CP[DSCP_]++;
				stats.TCPbReTX_CP[DSCP_]+=cmn->size()/1024.0;
			} else stats.TCPbGoTX_CP[DSCP_]+=cmn->size()/1024.0;	
		}			

   }

	
}


/*------------------------------------------------------------------------------
Packet* deque() 
    This method implements the dequing mechanism for a Diffserv router.
------------------------------------------------------------------------------*/
Packet* dsREDQueue::deque() {
	Packet *p=NULL;
	int queue, prec;
   	hdr_ip* iph;
   	int fid;

	int qToDq=QScheduler->DequeEvent();

	// Dequeue a packet from the underlying queue:
	if (qToDq >= 0) {
		p = redq_[qToDq].deque();
      		iph = hdr_ip::access(p);
      		fid = iph->flowid()/32;

      		/* There was a packet to be dequed;
         	   find the precedence level (or virtual queue)
         	   to which this packet was attached:
      		*/
      		lookupPHBTable(getCodePt(p), &queue, &prec);

      		// update state variables for that "virtual" queue
      		redq_[qToDq].updateREDStateVar(prec);

      		QScheduler->UpdateDepartureRate(qToDq, prec, hdr_cmn::access(p)->size());
		//printf("dep rate %d %d %f\n",qToDq,prec,QScheduler->GetDepartureRate(qToDq,prec));		
	}

	// Return the dequed packet:	
	return(p);
}


/*------------------------------------------------------------------------------
int getCodePt(Packet *p) 
    This method, when given a packet, extracts the code point marking from its 
header.
------------------------------------------------------------------------------*/
int dsREDQueue::getCodePt(Packet *p) {
	hdr_ip* iph = hdr_ip::access(p);
	return(iph->prio());
}


/*------------------------------------------------------------------------------
void lookupPHBTable(int codePt, int* queue, int* prec)
    Assigns the queue and prec parameters values corresponding to a given code 
point.  The code point is assumed to be present in the PHB table.  If it is 
not, an error message is outputted and queue and prec are undefined.
------------------------------------------------------------------------------*/
void dsREDQueue::lookupPHBTable(int codePt, int* queue, int* prec) {
   for (int i = 0; i < phbEntries; i++) {
      if (phb_[i].codePt_ == codePt) {
         *queue = phb_[i].queue_;
         *prec = phb_[i].prec_;
         return;
      }
   }
   printf("ERROR: No match found for code point %d in PHB Table.\n", codePt);
}


/*------------------------------------------------------------------------------
void addPHBEntry(int codePt, int queue, int prec)
    Add a PHB table entry.  (Each entry maps a code point to a queue-precedence
pair.)
------------------------------------------------------------------------------*/
void dsREDQueue::addPHBEntry(int codePt, int queue, int prec) {
	if (phbEntries == MAX_CP) {
      printf("ERROR: PHB Table size limit exceeded.\n");
	} else {
		phb_[phbEntries].codePt_ = codePt;
		phb_[phbEntries].queue_ = queue;
		phb_[phbEntries].prec_ = prec;
		stats.valid_CP[codePt] = 1;
		phbEntries++;
	}
}


/*------------------------------------------------------------------------------
void getStat(int argc, const char*const* argv) 
    
------------------------------------------------------------------------------*/
double dsREDQueue::getStat(int argc, const char*const* argv) {

	if (argc == 3) {
		if (strcmp(argv[2], "drops") == 0)
         return (stats.drops*1.0);
		if (strcmp(argv[2], "edrops") == 0)
         return (stats.edrops*1.0);
		if (strcmp(argv[2], "pkts") == 0)
         return (stats.pkts*1.0);

		if (strcmp(argv[2], "%drops") == 0)
         return (stats.pkts>0?stats.drops*100.0/stats.pkts:0);
		if (strcmp(argv[2], "%edrops") == 0)
         return (stats.pkts>0?stats.edrops*100.0/stats.pkts:0);
   }
	if (argc == 4) {

		int arg3 = (int)strtod(argv[3], NULL);
		double s_pkts   = stats.pkts_CP[arg3];
		double s_drops  = stats.drops_CP[arg3];
		double s_edrops = stats.edrops_CP[arg3];
		double s_TCPcwnd= stats.TCPcwnd_CP[arg3];
		double s_TCPrtt = stats.TCPrtt_CP[arg3];
		double s_TCPbReTX=stats.TCPbReTX_CP[arg3];
		double s_TCPbGoTX=stats.TCPbGoTX_CP[arg3];
		double s_TCPnReTX=stats.TCPnReTX_CP[arg3];

		if (strcmp(argv[2], "drops") == 0)
         return (s_drops);
		if (strcmp(argv[2], "edrops") == 0)
         return (s_edrops);
		if (strcmp(argv[2], "pkts") == 0)
         return (s_pkts);
		if (strcmp(argv[2], "TCPcwnd") == 0)
         return (s_TCPcwnd);
		if (strcmp(argv[2], "TCPrtt") == 0)
         return (s_TCPrtt);
		if (strcmp(argv[2], "TCPbReTX") == 0)
         return (s_TCPnReTX);
		if (strcmp(argv[2], "TCPbGoTX") == 0)
         return (s_TCPbGoTX);
		if (strcmp(argv[2], "TCPnReTX") == 0)
         return (s_TCPbReTX);

	 	if (strcmp(argv[2], "%drops") == 0)
         return (s_pkts>0?s_drops*100/s_pkts:0);
		if (strcmp(argv[2], "%edrops") == 0)
         return (s_pkts>0?s_edrops*100/s_pkts:0);
	}
	return -1.0;
}


/*------------------------------------------------------------------------------
void setNumPrec(int prec) 
    Sets the current number of drop precendences.  The number of precedences is
the number of virtual queues per physical queue.
------------------------------------------------------------------------------*/
void dsREDQueue::setNumPrec(int queue, int numPrec) {


	if (numPrec > MAX_PREC) 
		printf("ERROR: Cannot declare more than %d prcedence levels (as defined by MAX_PREC)\n",MAX_PREC);
	else 
                if (numPrec==0) for (int i=0; i<MAX_PREC; i++) redq_[i].numPrec = numPrec;
		else redq_[queue].numPrec = numPrec;
}

/*------------------------------------------------------------------------------
void setMREDMode(const char* mode)
   sets up the average queue accounting mode.
----------------------------------------------------------------------------*/
void dsREDQueue::setMREDMode(const char* mode, const char* queue) {
	int i;
	mredModeType tempMode;

	if (strcmp(mode, "RIO-C") == 0)
   	tempMode = rio_c;
	else if (strcmp(mode, "RIO-D") == 0)
		tempMode = rio_d;
	else if (strcmp(mode, "WRED") == 0)
		tempMode = wred;
	else if (strcmp(mode, "DROP") == 0)
		tempMode = dropTail;
	else {
		printf("Error: MRED mode %s does not exist\n",mode);
      return;
   }
	if (!queue) for (i = 0; i < MAX_QUEUES; i++) redq_[i].mredMode = tempMode; 
	else redq_[ (int)strtod(queue, NULL) ].mredMode = tempMode;

}


/*------------------------------------------------------------------------------
void printPHBTable()
    Prints the PHB Table, with one entry per line.
------------------------------------------------------------------------------*/
void dsREDQueue::printPHBTable() {
   printf("PHB Table:\n");
   for (int i = 0; i < phbEntries; i++)
      printf("Code Point %d is associated with Queue %d, Precedence %d\n", phb_[i].codePt_, phb_[i].queue_, phb_[i].prec_);
   printf("\n");
}


/*------------------------------------------------------------------------------
void printStats()
    An output method that may be altered to assist debugging.
------------------------------------------------------------------------------*/
void dsREDQueue::printStats() {
	printf("\nPackets Statistics\n");
	printf("=======================================\n");
	printf(" CP  TotPkts   TxPkts   ldrops   edrops\n");
	printf(" --  -------   ------   ------   ------\n");

	for (int i = 0; i < MAX_CP; i++)
		if (stats.pkts_CP[i] != 0)
			printf("%3d %8d  %6.2f%%  %6.2f%%   %6.2f%%\n",i,stats.pkts_CP[i],(stats.pkts_CP[i]-stats.drops_CP[i]-stats.edrops_CP[i])*100.0/stats.pkts_CP[i],stats.drops_CP[i]*100.0/stats.pkts_CP[i],stats.edrops_CP[i]*100.0/stats.pkts_CP[i]);

	printf("----------------------------------------\n");
        if (stats.pkts != 0) 
		printf("All %8d  %6.2f%%  %6.2f%%   %6.2f%%\n",stats.pkts,(stats.pkts-stats.drops-stats.edrops)*100.0/stats.pkts,stats.drops*100.0/stats.pkts,stats.edrops*100.0/stats.pkts);


}


/*------------------------------------------------------------------------------
void setSchedularMode(int schedtype)
   sets up the schedular mode.
----------------------------------------------------------------------------*/
void dsREDQueue::setSchedularMode(int argc, const char*const* argv) {

	const char* schedtype=argv[2];	   

	delete QScheduler;
	if (strcmp(schedtype, "RR") == 0)
   		QScheduler = new dsRR(numQueues_);
	else if (strcmp(schedtype, "WRR") == 0)
		QScheduler = new dsWRR(numQueues_);
	else if (strcmp(schedtype, "WIRR") == 0)
		QScheduler = new dsWIRR(numQueues_);
	else if (strcmp(schedtype, "WFQ") == 0)
	        QScheduler = new dsWFQ(numQueues_,link_->bandwidth());
	else if (strcmp(schedtype, "SCFQ") == 0)
	        QScheduler = new dsSCFQ(numQueues_,link_->bandwidth());
	else if (strcmp(schedtype, "SFQ") == 0)
	        QScheduler = new dsSFQ(numQueues_, link_->bandwidth());
	else if (strcmp(schedtype, "WF2Qp") == 0)
	        QScheduler = new dsWF2Qp(numQueues_,link_->bandwidth());
	else if (strcmp(schedtype, "LLQ") == 0)
		if (argc==5) QScheduler = new dsLLQ(numQueues_,atof(argv[4]),argv[3]);
		else printf("wrong parameter number for LLQ scheduler\n");
	else if (strcmp(schedtype, "PRI") == 0)
		if (argc==4) QScheduler = new dsPQ(numQueues_,atof(argv[3]));
		else QScheduler = new dsPQ(numQueues_,1);
	else
		printf("Error: Scheduler type %s does not exist\n",schedtype);
}


/*------------------------------------------------------------------------------
void addQueueWeight(int queueNum, double weight)
   An input method to set the individual Queue Weights.
----------------------------------------------------------------------------*/
void dsREDQueue::addQueueWeight(int queueNum, double weight) {
   if(queueNum < MAX_QUEUES)      
      QScheduler->AddParam(queueNum,weight);	
   else 
      printf("The queue number is out of range.\n");
}


/*------------------------------------------------------------------------------
void addQueueRate(int queueNum, int rate)
   An input method to set the individual Queue Max Rates for Priority Queueing.
----------------------------------------------------------------------------*/
void dsREDQueue::addQueueRate(int queueNum, int rate) {

   if(queueNum < MAX_QUEUES) QScheduler->AddParam(queueNum,rate);
   else printf("The queue number is out of range.\n");
}

void dsREDQueue::setQSize(int queueNum, int qMaxSize) { 
    if(queueNum < MAX_QUEUES) redq_[queueNum].qlim = phisQueueLimit[queueNum]=qMaxSize;
    else printf("The queue number is out of range.\n");
}

void dsREDQueue::FDinit(const char *const DSCPstr) {

    char FN1[20]="FDcwnd_DSCP";
    strcat(FN1 ,DSCPstr);
    strcat(FN1 ,".tr");
    stats.TCPcwndFD_CP[ (int)strtod(DSCPstr, NULL) ] = new dsFD( 1, FN1);
	
    char FN2[20]="FDrtt_DSCP";
    strcat(FN2 ,DSCPstr);
    strcat(FN2 ,".tr");
    stats.TCPrttFD_CP[  (int)strtod(DSCPstr, NULL) ]  = new dsFD( 1, FN2);
	
}

/*------------------------------------------------------------------------------
int command(int argc, const char*const* argv)
    Commands from the ns file are interpreted through this interface.
------------------------------------------------------------------------------*/
int dsREDQueue::command(int argc, const char*const* argv)
{

	if (strcmp(argv[1], "configQ") == 0) {
                // NB: if num of parameters wrong, then seg fault
		redq_[ (int)strtod(argv[2], NULL) ].config( (int)strtod(argv[3], NULL), argc, argv);
		return(TCL_OK);
	}
	if (strcmp(argv[1], "addPHBEntry") == 0) {
		addPHBEntry( (int)strtod(argv[2], NULL),  (int)strtod(argv[3], NULL),  (int)strtod(argv[4], NULL) );
		return (TCL_OK);
	}
	if (strcmp(argv[1], "meanPktSize") == 0) {
		for (int i = 0; i < MAX_QUEUES; i++)
			redq_[i].setMPS( (int)strtod(argv[2], NULL) );
		return(TCL_OK);
	}
	if (strcmp(argv[1], "setNumPrec") == 0) {
		if (argc == 3) setNumPrec( (int)strtod(argv[2], NULL),0);
		else setNumPrec( (int)strtod(argv[2], NULL),  (int)strtod(argv[3], NULL));
		return(TCL_OK);
	}
	if (strcmp(argv[1], "setQSize") == 0) {
		setQSize( (int)strtod(argv[2], NULL),  (int)strtod(argv[3], NULL));
		return(TCL_OK);
	}
	if (strcmp(argv[1], "getAverage") == 0) {
		Tcl& tcl = Tcl::instance();
		tcl.resultf("%f", redq_[ (int)strtod(argv[2], NULL)].getWeightedLength() );
		return(TCL_OK);
	}
	if (strcmp(argv[1], "getDepartureRate") == 0) {
		Tcl& tcl = Tcl::instance();
		if (argc==4) tcl.resultf("%f", QScheduler->GetDepartureRate( (int)strtod(argv[2], NULL), (int)strtod(argv[3], NULL)) );
		else 	     tcl.resultf("%f", QScheduler->GetDepartureRate( (int)strtod(argv[2], NULL), -1) );
		return(TCL_OK);
	}
	if (strcmp(argv[1], "getStat") == 0) {
		Tcl& tcl = Tcl::instance();
		tcl.resultf("%.2f", getStat(argc,argv));
		return(TCL_OK);
	}
	if (strcmp(argv[1], "getQueueLen") == 0) {
		Tcl& tcl = Tcl::instance();
		tcl.resultf("%d", redq_[ (int)strtod(argv[2], NULL) ].getRealLength());
		return(TCL_OK);
	}
	if (strcmp(argv[1], "getVirtQueueLen") == 0) {
		Tcl& tcl = Tcl::instance();
		tcl.resultf("%d", redq_[ (int)strtod(argv[2], NULL) ].getVirtQueueLen( (int)strtod(argv[3], NULL) ));
		return(TCL_OK);
	}
	if (strcmp(argv[1], "printStats") == 0) {
		printStats();
		return (TCL_OK);
	}
	if (strcmp(argv[1], "printWRRcount") == 0) {
		((dsWIRR*)QScheduler)->printWRRcount();
		return (TCL_OK);
	}
	if (strcmp(argv[1], "printPHBTable") == 0) {
		printPHBTable();
		return (TCL_OK);
	}
	if (strcmp(argv[1], "link") == 0) {
		Tcl& tcl = Tcl::instance();
		LinkDelay* del = (LinkDelay*) TclObject::lookup(argv[2]);
		if (del == 0) {
			tcl.resultf("RED: no LinkDelay object %s",
			argv[2]);
			return(TCL_ERROR);
		}
		link_ = del;
		return (TCL_OK);
	}
	if (strcmp(argv[1], "early-drop-target") == 0) {
		Tcl& tcl = Tcl::instance();
		NsObject* p = (NsObject*)TclObject::lookup(argv[2]);
		if (p == 0) {
			tcl.resultf("no object %s", argv[2]);
			return (TCL_ERROR);
		}
		de_drop_ = p;
		return (TCL_OK);
	}
   if (strcmp(argv[1], "setSchedularMode") == 0) {
  		setSchedularMode(argc, argv);
      return(TCL_OK);
   }
   if (strcmp(argv[1], "setMREDMode") == 0) {
		if (argc == 3)
	      setMREDMode(argv[2],0);
		else
			setMREDMode(argv[2],argv[3]);
      return(TCL_OK);
   }
   	if (strcmp(argv[1], "addQueueWeight") == 0) {
      		addQueueWeight( (int)strtod(argv[2], NULL), atof(argv[3]));
      		return(TCL_OK);
 	}
	if (strcmp(argv[1], "setQueueBW") == 0) {
		redq_[ (int)strtod(argv[2], NULL) ].setPTC(atof(argv[3]));
		//printf(" setting PTC %d %f\n",  (int)strtod(argv[2], NULL),atof(argv[3]));
      		return(TCL_OK);
   	}
        // debug 
	if (strcmp(argv[1], "getPTC") == 0) {
		redq_[1].getPTC();
		redq_[2].getPTC();
		redq_[3].getPTC();
      return(TCL_OK);
   }
   if (strcmp(argv[1], "addQueueRate") == 0) {
      addQueueRate( (int)strtod(argv[2], NULL),  (int)strtod(argv[3], NULL));
      return(TCL_OK);
   }
	if (strcmp(argv[1], "frequencyDistribution") == 0) {
		FDinit(argv[2]);
		return(TCL_OK);
	}
	if (strcmp(argv[1], "flushFD") == 0) {
		stats.TCPcwndFD_CP[ (int)strtod(argv[2], NULL) ]->flush(1,0);
		stats.TCPrttFD_CP[ (int)strtod(argv[2], NULL) ]->flush(10,0);
		return(TCL_OK);
	}
	return(Queue::command(argc, argv));
}


/*-----------------------------------*/
dsFD::dsFD(double RL, char* FN):RangeLenght(RL)
{	strcpy(FileName, FN);
	counter=0;
}

void dsFD::occurency(double Value)
{   	unsigned int Position=(unsigned int)ceil(Value/RangeLenght);
	if (Position>=FDtable.size()) FDtable.resize(Position+1,0);
	FDtable[Position]++;
	counter++;
}


void dsFD::flush(double norm, double threshold)
{	
	ofstream fileFD;
	fileFD.open(FileName,fstream::out);
	for (unsigned int i=0; i<FDtable.size(); i++) {
		double FDpercentage=FDtable[i]*100.0/counter;
		if (FDpercentage>threshold) 
			fileFD << i*RangeLenght/norm << " " << FDpercentage << endl;
	}
	fileFD.close();	
}


