/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1994-1997 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /nfs/jade/vint/CVSROOT/ns-2/tools/loss-monitor.cc,v 1.18 2000/09/01 03:04:06 haoboy Exp $ (LBL)";
#endif

#include <tclcl.h>
#include <fstream>
#include <string>
#include <math.h>

#include "agent.h"
#include "config.h"
#include "packet.h"
#include "ip.h"
#include "rtp.h"
#include "loss-monitor.h"

static class LossMonitorClass : public TclClass {
public:
	LossMonitorClass() : TclClass("Agent/LossMonitor") {}
	TclObject* create(int, const char*const*) {
		return (new LossMonitor());
	}
} class_loss_mon;

LossMonitor::LossMonitor() : Agent(PT_NTYPE)
{
	bytes_ = 0;
	nlost_ = 0;
	npkts_ = 0;
	expected_ = -1;
	last_packet_time_ = 0.;
	seqno_ = 0;

        flowid_                   = 0;
        npkts_flowid_             = 0;
        sum_ipdv_                 = 0;
        sum_owd_                  = 0;
        current_owd_              = 0;
        current_ipdv_             = 0;
        previous_owd_             = 0;
        min_owd_                  = 100000000;

        bind("flowid_", &flowid_);
        bind("npktsFlowid_", &npkts_flowid_ );
        bind("sumIpdv_", &sum_ipdv_ );
        bind("sumOwd_", &sum_owd_ );
        bind("ipdv_", &current_ipdv_ );
        bind("owd_", &current_owd_ );
        bind("minOwd_", &min_owd_ );
        owdFD  = NULL;
        ipdvFD = NULL;


	bind("nlost_", &nlost_);
	bind("npkts_", &npkts_);
	bind("bytes_", &bytes_);
	bind("lastPktTime_", &last_packet_time_);
	bind("expected_", &expected_);
}

void LossMonitor::recv(Packet* pkt, Handler*)
{
	hdr_rtp* p = hdr_rtp::access(pkt);
	seqno_ = p->seqno();
	bytes_ += hdr_cmn::access(pkt)->size();
	++npkts_;


        // computation for owd and ipdv statistics on a per-flow base
        if (hdr_ip::access(pkt)->flowid()==flowid_) {
        npkts_flowid_++;// counter for packets in flowid_
        current_owd_=(Scheduler::instance().clock()-hdr_cmn::access(pkt)->sendtime_);
        // one-way delay for the received packet
        if (current_owd_<min_owd_)
                min_owd_ = current_owd_;
        sum_owd_+=current_owd_;     
        // sum of the owd; needed to calcolate average at simulation time
        if (previous_owd_!=0)   {   // ipdv=|current_owd_-previous_owd_|
                                if (previous_owd_<current_owd_) current_ipdv_=(current_owd_-previous_owd_);
                                else current_ipdv_=(previous_owd_-current_owd_);
        sum_ipdv_+=current_ipdv_;
                }
        previous_owd_=current_owd_;
        }
 
        if (ipdvFD!=NULL) {
                ipdvFD->occurency(current_ipdv_);
                owdFD->occurency(current_owd_);
        }
 




	/*
	 * Check for lost packets
	 */
	if (expected_ >= 0) {
		int loss = seqno_ - expected_;
		if (loss > 0) {
			nlost_ += loss;
			Tcl::instance().evalf("%s log-loss", name());
		}
	}
	last_packet_time_ = Scheduler::instance().clock();
	expected_ = seqno_ + 1;
	Packet::free(pkt);
}

/*
 * $proc interval $interval
 * $proc size $size
 */
int LossMonitor::command(int argc, const char*const* argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "clear") == 0) {
			expected_ = -1;
			return (TCL_OK);
		}
                if (strcmp(argv[1], "flushFD") == 0) {
                        if (ipdvFD!=NULL) {
                                owdFD->flush(1,0);
                                ipdvFD->flush(1,0);
                        }
                        return (TCL_OK);
                }
        }
        if (argc == 4) {
                if (strcmp(argv[1], "flushFD") == 0) {
                        if (ipdvFD!=NULL) {
                                owdFD->flush(atof(argv[2]),0);
                                ipdvFD->flush(atof(argv[3]),0);
                        }
                        return (TCL_OK);
                }
        }
        if (argc == 5) {
                if (strcmp(argv[1], "FrequencyDistribution") == 0) {
 
                        char FN1[20]="owd";
                        strcat(FN1, argv[4]);
                        owdFD  = new tFD( atof(argv[2]), FN1);
 
                        char FN2[20]="ipdv";
                        strcat(FN2, argv[4]);
                        ipdvFD = new tFD( atof(argv[3]),  FN2);

                       return (TCL_OK);
               }
       }
       if (argc == 6) {
               if (strcmp(argv[1], "flushFD") == 0) {
                       if (ipdvFD!=NULL) {
                              owdFD->flush(atof(argv[2]),atof(argv[4]));
                              ipdvFD->flush(atof(argv[3]),atof(argv[5]));
                       }
                       return (TCL_OK);
               }
       }

	return (Agent::command(argc, argv));
}


tFD::tFD(double RL, char* FN):RangeLenght(RL)
{      strcpy(FileName, FN);
       counter=0;
}

void tFD::occurency(double Value)
{      unsigned int Position=(unsigned int)ceil(Value/RangeLenght);
       if (Position>=FDtable.size()) FDtable.resize(Position+1,0);
       FDtable[Position]++;
       counter++;
}


void tFD::flush(double norm, double threshold)
{
       ofstream fileFD, totalFD;
       fileFD.open(FileName,fstream::out);
       for (unsigned int i=0; i<FDtable.size(); i++) {
               double FDpercentage=FDtable[i]*100.0/counter;
               if (FDpercentage>threshold)
                       fileFD << i*RangeLenght/norm << " " << FDpercentage << endl;
       }
       fileFD.close();
       totalFD.close();
}

