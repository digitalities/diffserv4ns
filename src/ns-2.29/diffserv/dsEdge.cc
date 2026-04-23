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
 * Integrated into ns by Xuan Chen (xuanc@isi.edu) 
 * Add instance of policyClassifier.
 */

#include "dsEdge.h"
#include "dsPolicy.h"
#include "packet.h"
#include "tcp.h"
#include "random.h"


/*------------------------------------------------------------------------------
class edgeClass 
------------------------------------------------------------------------------*/
static class edgeClass : public TclClass {
public:
	edgeClass() : TclClass("Queue/dsRED/edge") {}
	TclObject* create(int, const char*const*) {
		return (new edgeQueue);
	}
} class_edge;


/*------------------------------------------------------------------------------
edgeQueue() Constructor.
------------------------------------------------------------------------------*/
edgeQueue::edgeQueue() {
      	NumMarkRules=0;
}


/*------------------------------------------------------------------------------
void enque(Packet* pkt) 
Post: The incoming packet pointed to by pkt is marked with an appropriate code
  point (as handled by the marking method) and enqueued in the physical and 
  virtual queue corresponding to that code point (as specified in the PHB 
  Table).
Uses: Methods Policy::mark(), lookupPHBTable(), and redQueue::enque(). 
------------------------------------------------------------------------------*/
void edgeQueue::enque(Packet* pkt) {
	int codePt;

	// Mark the packet with the specified priority:
	mark(pkt);
	codePt = policy.mark(pkt);
	dsREDQueue::enque(pkt);
}



/*------------------------------------------------------------------------------
void addMarkRule() 

------------------------------------------------------------------------------*/
void edgeQueue::addMarkRule(int argc, const char*const* argv) {
	
        if (NumMarkRules == MAX_MARK_RULES)
   		 printf("ERROR: Max number of mark rules limit exceeded\n");
        else {
		if (argc==7) {   
			 MarkRulesTable[NumMarkRules].DSCP  = (int)strtod(argv[2], NULL);
			 MarkRulesTable[NumMarkRules].saddr = (int)strtod(argv[3], NULL);
	 	         MarkRulesTable[NumMarkRules].daddr = (int)strtod(argv[4], NULL);

	 	         const char* PacketType=argv[5];	   
			 if (strcmp(PacketType, "tcp") == 0)
   				MarkRulesTable[NumMarkRules].pType = PT_TCP;  // see definition in packet.h
			 else if (strcmp(PacketType, "ack") == 0)
   				MarkRulesTable[NumMarkRules].pType = PT_ACK;  // see definition in packet.h
			 else if (strcmp(PacketType, "udp") == 0)
   				MarkRulesTable[NumMarkRules].pType = PT_UDP;  // see definition in packet.h
			 else if (strcmp(PacketType, "any") == 0) 
				MarkRulesTable[NumMarkRules].pType = PT_NTYPE;    //we use as every packet meaning
			 else { printf("in addMarkRule Application Packet type %s doesn't match\n",PacketType);
			        MarkRulesTable[NumMarkRules].pType = PT_NTYPE;
                              } 

		     	 const char* AppType=argv[6];	   
			 if (strcmp(AppType, "telnet") == 0)
   				MarkRulesTable[NumMarkRules].appType = PT_TELNET;  // see definition in packet.h
			 else if (strcmp(AppType, "ftp") == 0)
   				MarkRulesTable[NumMarkRules].appType = PT_FTP;  // see definition in packet.h
			 else if (strcmp(AppType, "http") == 0)
   				MarkRulesTable[NumMarkRules].appType = PT_HTTP;  // see definition in packet.h
		 	 else if (strcmp(AppType, "audio") == 0)
   				MarkRulesTable[NumMarkRules].appType = PT_AUDIO;  // see definition in packet.h
		 	 else if (strcmp(AppType, "realaudio") == 0)
   				MarkRulesTable[NumMarkRules].appType = PT_REALAUDIO;  // see definition in packet.h
			 else if (strcmp(AppType, "cbr") == 0)
   				MarkRulesTable[NumMarkRules].appType = PT_CBR;  // see definition in packet.h
			 else if (strcmp(AppType, "any") == 0) 
				MarkRulesTable[NumMarkRules].appType = PT_NTYPE;    //we use as every packet meaning
			 else { printf("in addMarkRule packet type %s doesn't match\n",AppType);
			        MarkRulesTable[NumMarkRules].appType = PT_NTYPE;
                              } 

			 NumMarkRules++;
		}
   		         else  printf("ERROR: wrong number of parameters in addMarkRule\n");
	}
}


/*------------------------------------------------------------------------------
void printMarkRulesTable() 
------------------------------------------------------------------------------
void edgeQueue::printMarkRulesTable() {
	printf("Mark Rules Table - %d rules added\n",numMarkRules);	
        printf("Rule Number     DSCP     Packet Type   SourceAddr   DestAddr\n");
	for (int i=0; i<numMarkRules;i++)
           printf("    %d           %d        %s       %d       %d   \n",i,markRulesTable[i].DSCP,
	   (markRulesTable[i].saddr==ANY)?ANY:markRulesTable[i].saddr,
           (markRulesTable[i].daddr==ANY)?ANY:markRulesTable[i].daddr);	
}
*/
	

/*------------------------------------------------------------------------------
void mark(Packet *pkt) 

------------------------------------------------------------------------------*/
void edgeQueue::mark(Packet *pkt) {
	hdr_ip* iph=hdr_ip::access(pkt);
	hdr_cmn* cmn=hdr_cmn::access(pkt);
	int marked=0;
	int MarkRule;
     	
	for (MarkRule=0; MarkRule<NumMarkRules; MarkRule++) 
	    if (( (MarkRulesTable[MarkRule].saddr==iph->saddr() ) || ( MarkRulesTable[MarkRule].saddr==-1) )
				                &&
 	        ( (MarkRulesTable[MarkRule].daddr==iph->daddr() ) || ( MarkRulesTable[MarkRule].daddr==-1) )
						&&
 	        ( (MarkRulesTable[MarkRule].appType==cmn->app_type() ) || ( MarkRulesTable[MarkRule].appType== PT_NTYPE) ) 
						&&
 	        ( (MarkRulesTable[MarkRule].pType==cmn->ptype() ) || ( MarkRulesTable[MarkRule].pType== PT_NTYPE) ) )
  	    {
	   		    iph->prio_ = MarkRulesTable[MarkRule].DSCP;
			    marked=1; break;

 	    }
	//if (iph->prio_==1)
	 // printf(" marked %d tabType %d pcktype %d tab App %d apptype %d\n", iph->prio_, MarkRulesTable[MarkRule].pType,cmn->ptype(),MarkRulesTable[MarkRule].appType,cmn->app_type());
	
	if (!marked) iph->prio_ = 0;    // Default PHB; Always define a queue for this aggregate

}





/*------------------------------------------------------------------------------
int command(int argc, const char*const* argv) 
    Commands from the ns file are interpreted through this interface.
------------------------------------------------------------------------------*/
int edgeQueue::command(int argc, const char*const* argv) {
  if (strcmp(argv[1], "addPolicyEntry") == 0) {
    // Note: the definition of policy has changed.
    policy.addPolicyEntry(argc, argv);
    return(TCL_OK);
  };

  if (strcmp(argv[1], "addPolicerEntry") == 0) {
    // Note: the definition of policy has changed.
    policy.addPolicerEntry(argc, argv);
    return(TCL_OK);
  };

  if (strcmp(argv[1], "addMarkRule") == 0) {
    addMarkRule(argc, argv);
    return(TCL_OK);
  };
  
  if (strcmp(argv[1], "printPolicyTable") == 0) {
    policy.printPolicyTable();
    return(TCL_OK);
  }
  if (strcmp(argv[1], "printPolicerTable") == 0) {
    policy.printPolicerTable();
    return(TCL_OK);
    
  }
  
  return(dsREDQueue::command(argc, argv));
};
