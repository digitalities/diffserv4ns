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
/* Copyright (C) 2001-2006  Sergio Andreozzi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * GNU Licenses: http://www.gnu.org/licenses/gpl.txt
 *
*/

/*
 * ns-2.35 port: this file replaces src/ns-2.29/diffserv/dsscheduler.cc.
 * BUG-4 fix: in dsSFQ::DequeEvent(), empty() check moved before front() to eliminate
 * undefined behaviour when a flow queue is empty. See docs/HISTORICAL_BUGS.md BUG-4.
 */

#include "dsconsts.h"
#include "dsscheduler.h"


/*  Scheduler class 
*/

/* helpful functions */
#define max(x,y) ((x>y)?x:y)
#define min(x,y) ((x>y)?y:x)
#define MAXDOUBLE 100000000.0

dsScheduler::dsScheduler(int NQ)
{  winLen=1; 
   initMeter();
}

dsScheduler::dsScheduler(int NQ, double LBw)
{  winLen=1; 
   initMeter();
}

dsScheduler::dsScheduler(int NQ, double LBw, const char* PFQSchedType)
{  winLen=1; 
   initMeter();
}

void dsScheduler::initMeter()
{ for (int i=0; i<MAX_QUEUES; i++) {
	queueAvgRate[i]=0;
	for (int j=0; j<MAX_PREC; j++) qpAvgRate[i][j]=0;
  
  }
}

void dsScheduler::applyTSWMeter(int PSize, double *AvgRate, double *ArrTime)
{
	double now, bytesInTSW, newBytes;
	bytesInTSW = *AvgRate * winLen;
	newBytes = bytesInTSW+PSize;
	now = Scheduler::instance().clock();
	*AvgRate = newBytes/ (now - *ArrTime + winLen); // in bytes
	*ArrTime = now;
}	


void dsScheduler::UpdateDepartureRate(int Queue, int Prec, int PSize)
{
 for (int i=0; i<NumQueues; i++) {
	applyTSWMeter((i==Queue) ? PSize : 0, &queueAvgRate[i], &queueArrTime[i]); 
	for (int j=0; j<MAX_PREC; j++)
		applyTSWMeter( ((i==Queue)&&(j==Prec)) ? PSize : 0, &qpAvgRate[i][j], &qpArrTime[i][j]); 
 }
}

double dsScheduler::GetDepartureRate(int Queue, int Prec) 
{ // printf("avg rate %f\n",qpAvgRate[Queue][Prec]);
   if (Prec==-1)    return(queueAvgRate[Queue]*8);
   else return(qpAvgRate[Queue][Prec]*8);  // in bit per seconds
}

dsRR::dsRR(int NQ):dsScheduler(NQ) 
{	
	NumQueues=NQ;	
	for (int i=0; i<MAX_QUEUES; i++) queueLen[i]=0;
	qToDq=-1;
        Reset();
}

void dsRR::Reset()
{
  
}

int dsRR::DequeEvent()
{  int i=0; 
   qToDq = ((qToDq + 1) % NumQueues);
   while ((i < NumQueues) && (queueLen[qToDq] == 0)) {
			qToDq = ((qToDq + 1) % NumQueues);			
   			i++;
   }
   if (i==NumQueues) return(-1); else {queueLen[qToDq]--; return qToDq;}
}

void dsRR::EnqueEvent(Packet* pkt, int Queue)
{
   queueLen[Queue]++; 
}

dsWRR::dsWRR(int NQ):dsScheduler(NQ) 
{	
	NumQueues=NQ;	
	for (int i=0; i<MAX_QUEUES; i++) {queueLen[i]=0; queueWeight[i]=1;}
	qToDq=0;
        Reset();
}

void dsWRR::Reset()
{  for (int i=0; i<MAX_QUEUES; i++) {wirrTemp[i]=0;}
}

int dsWRR::DequeEvent()
{    int i=0;
     if (wirrTemp[qToDq]<=0){
  	      qToDq = ((qToDq + 1) % NumQueues);
     	      wirrTemp[qToDq] = queueWeight[qToDq] - 1;
      } else wirrTemp[qToDq] = wirrTemp[qToDq] -1;
					
      while ((i < NumQueues) && (queueLen[qToDq] == 0)) {
			wirrTemp[qToDq] = 0;
  	      		qToDq = ((qToDq + 1) % NumQueues);
     	       		wirrTemp[qToDq] = queueWeight[qToDq] - 1;
			i++;
      }
       if (i==NumQueues) return(-1); else {queueLen[qToDq]--; return qToDq;}
}

void dsWRR::EnqueEvent(Packet* pkt, int Queue)
{
   queueLen[Queue]++; 
}

dsWIRR::dsWIRR(int NQ):dsScheduler(NQ) 
{	
	NumQueues=NQ;	
   	queuesDone = MAX_QUEUES;
	for (int i=0; i<MAX_QUEUES; i++) {queueLen[i]=0; queueWeight[i]=1;}
	qToDq=0;
        Reset();
}

void dsWIRR::Reset()
{     for(int i=0;i<MAX_QUEUES;i++){
		slicecount[i]=0;
      		wirrTemp[i]=0;
      		wirrqDone[i]=0;
      }
}

int dsWIRR::DequeEvent()
{    int i=0;
     qToDq = ((qToDq + 1) % NumQueues);
     while ((i<NumQueues) && ((queueLen[qToDq]==0) || (wirrqDone[qToDq]))) {
			if (!wirrqDone[qToDq]) {
				queuesDone++;
				wirrqDone[qToDq]=1;
			}
  	      		qToDq = ((qToDq + 1) % NumQueues);
			i++;
      }
      if (wirrTemp[qToDq] == 1) {
  	       queuesDone +=1;
          wirrqDone[qToDq]=1;
      }
      wirrTemp[qToDq]-=1;
      if(queuesDone >= NumQueues) {
          queuesDone = 0;
          for(i=0;i<NumQueues;i++) {
  	       wirrTemp[i] = queueWeight[i];
     	       wirrqDone[i]=0;
          }   	
      }
      if (i==NumQueues) return(-1); else {queueLen[qToDq]--; return(qToDq);}
}

void dsWIRR::printWRRcount() {

   for (int i = 0; i < NumQueues; i++){
      printf("%d: %d %d %d.\n", i, slicecount[i],queueLen[i],queueWeight[i]);
   }
}


void dsWIRR::EnqueEvent(Packet* pkt, int Queue)
{
   queueLen[Queue]++; 
}


dsPQ::dsPQ(int NQ, double WinLen):dsScheduler(NQ, WinLen)
{	
	NumQueues=NQ;	
	winLen=WinLen; 
	for (int i=0; i<MAX_QUEUES; i++) {
		queueMaxRate[i]=0;
		queueLen[i]=0;
        }
	Reset();

}

void dsPQ::Reset()
{
  for(int i=0;i<MAX_QUEUES;i++)	queueArrTime[i] = 0.0;
}

int dsPQ::DequeEvent()
{ 
   int qToDq=0, i=0;
   while ((i < NumQueues) && ((queueLen[qToDq] == 0) ||
	((queueAvgRate[qToDq] > queueMaxRate[qToDq]) && queueMaxRate[qToDq]))) {
	       i++;
	       qToDq = i;
	}
	if (i == NumQueues) {
		i = qToDq = 0;			
		while ((i < NumQueues) && (queueLen[qToDq] == 0)) {
				qToDq = ((qToDq + 1) % NumQueues);
				i++;
		}
	}
    if (i<NumQueues) {
        queueLen[qToDq]--;   
        return qToDq; 
    } else 
    {return(-1); printf("PRIORITY SCHEDULER: no packet to be dequeued\n");}  // no packet to be dequeued
}

void dsPQ::EnqueEvent(Packet* pkt, int Queue)
{
   queueLen[Queue]++; 
}

dsWFQ::dsWFQ(int NQ, double LBw):dsScheduler(NQ, LBw) 
{	
	NumQueues=NQ;	
	LinkBandwith=LBw;
	for (int i=0; i<MAX_QUEUES; i++) {
		fs_[i].weight_=1;
		fs_[i].B=0;	
	}  	
  	v_time = 0; 
  	idle = 1;
  	wfq_event = 0;
  	sum = 0;
  	last_vt_update = 0;	sessionDelay=0;
  	GPSDeparture=-1;	

	Reset(); 
}

void dsWFQ::Reset()
{ for (int i=0; i<MAX_QUEUES; i++) fs_[i].finish_t=0;
  idle=1;
}

void dsWFQ::EnqueEvent(Packet* pkt, int queue)
{
    double now=Scheduler::instance().clock();

    // virtual time update. Formula 10
	if(idle) {
		v_time=0;
		last_vt_update=now;
		idle=0;
	} else {
		v_time=v_time+(now-last_vt_update)/sum;
		last_vt_update=now;
	}

    // finish time computation. Formula 11 
    fs_[queue].finish_t = (max(fs_[queue].finish_t,v_time))+
                         ((double)hdr_cmn::access(pkt)->size())*8/(fs_[queue].weight_*LinkBandwith); 


    // update sum and B
    if( (fs_[queue].B++)==0 ) sum+=fs_[queue].weight_; 
    // insertion in both lists
    fs_[queue].GPS.push(fs_[queue].finish_t);
    fs_[queue].PGPS.push(sessionDelay+fs_[queue].finish_t);
	
    // schedule next departure in the GPS reference system
    if (wfq_event!=0) {
		Scheduler::instance().cancel(wfq_event);
		delete wfq_event;
    }      
    scheduleWFQ();   
}

void dsWFQ::handle(Event *e) 
{
	double now = Scheduler::instance().clock();
        //double fTime=fs_[GPSDeparture].GPS.front();
      
	//update virtual time
	v_time=v_time+(now-last_vt_update)/sum;
	last_vt_update=now;

	//extract packet in GPS system
	
   	if (GPSDeparture!=-1) {		        
		fs_[GPSDeparture].GPS.pop();
 	        if (--fs_[GPSDeparture].B == 0) sum-=fs_[GPSDeparture].weight_; 
		if ( fabs(sum) < 0.00001 ) sum=0; 
		if (sum==0) {Reset();            
			     sessionDelay=now;	
	  		     }  	
        } else printf("DEBUG: ERROR, no active sessions \n");
    
	// if GPS is not idle, schedule next GPS departure
	delete e;
	if(!idle) 
		scheduleWFQ();
	else 
		wfq_event=NULL;
}

void dsWFQ::scheduleWFQ() 
{
	wfq_event=new Event(); 
        double tmp;
	GPSDeparture=-1; 

  	double minFinishTime=MAXDOUBLE;
  	for (int i=0; i<NumQueues; i++)  
   	 	if (!fs_[i].GPS.empty()) {
			if  (fs_[i].GPS.front()<minFinishTime) {
			                GPSDeparture=i; 
			                minFinishTime=fs_[GPSDeparture].GPS.front();
                        }			
                }
              
        if (GPSDeparture!=-1) { 
		tmp=(minFinishTime-v_time)*sum;
		// following line is there to recover errors due to finite precision	
		if (tmp<0) tmp=0; 
		Scheduler::instance().schedule((Handler *)this,wfq_event,tmp);
	} else printf("DEDUG: ERROR no event to schedule \n");	
}

int dsWFQ::DequeEvent()
{ 
  int qToDq=-1; 
  double minFinishTime=MAXDOUBLE;
  for (int i=0; i<NumQueues; i++) 
	if (!fs_[i].PGPS.empty()) 
	     if (fs_[i].PGPS.front()<minFinishTime) {qToDq=i; minFinishTime=fs_[qToDq].PGPS.front();}
	  
  
  if (qToDq!=-1) fs_[qToDq].PGPS.pop(); 
  return qToDq;	
}

dsSCFQ::dsSCFQ(int NQ, double LBw):dsScheduler(NQ, LBw) 
{	
	NumQueues=NQ;	
	LinkBandwith=LBw;
	for (int i=0; i<MAX_QUEUES; i++) {
		session[i].weight=1;
	}
	Reset();
}

void dsSCFQ::Reset()
{ for (int i=0; i<MAX_QUEUES; i++) {
		 session[i].label=0;
   }
	tlabel=0;
}

void dsSCFQ::EnqueEvent(Packet* pkt, int queue)
{   session[queue].label = (max(session[queue].label, tlabel))+
                           ((double)hdr_cmn::access(pkt)->size())/session[queue].weight/(LinkBandwith/8.0);
    session[queue].SessionQueue.push(session[queue].label); 
}

int dsSCFQ::DequeEvent()
{ 
  int qToDq=-1; 
  double minFinishTime=MAXDOUBLE;
  for (int i=0; i<NumQueues; i++) 
	if ((!session[i].SessionQueue.empty())&&(session[i].SessionQueue.front()<minFinishTime)) {
		qToDq=i; 
		minFinishTime=session[qToDq].SessionQueue.front();
	}     
  if (qToDq!=-1) { tlabel=minFinishTime; 
	           session[qToDq].SessionQueue.pop();}
  return qToDq;
}


dsSFQ::dsSFQ(int NQ, double LBw):dsScheduler(NQ, LBw) 
{	
	NumQueues=NQ;	
	LinkBandwith=LBw; 
	for (int i=0; i<MAX_QUEUES; i++) {
		flow[i].weight=1;
	}
	idle=1; 
        MaxFinishTag=0.0;
	Reset();
}

void dsSFQ::Reset()
{   	for (int i=0; i<MAX_QUEUES; i++) {
		 flow[i].LastFinishTag=0;
   	}
	V=0;
}

void dsSFQ::EnqueEvent(Packet* pkt, int queue)
{   PacketTags.StartTag=max(V,flow[queue].LastFinishTag);
    PacketTags.FinishTag=PacketTags.StartTag+((double)hdr_cmn::access(pkt)->size())/flow[queue].weight/(LinkBandwith/8.0);
    flow[queue].LastFinishTag=PacketTags.FinishTag;
    flow[queue].FlowQueue.push(PacketTags); 
    idle=0;	
}

int dsSFQ::DequeEvent()
{ int qToDq=-1;
  double MinStartTag=MAXDOUBLE;

  for (int i=0; i<NumQueues; i++) {
	// BUG-4 fix: empty() before front() — see HISTORICAL_BUGS.md
	if (!flow[i].FlowQueue.empty()) {
		PacketTags=flow[i].FlowQueue.front();
		if (PacketTags.StartTag<MinStartTag) {
			qToDq=i;
			MinStartTag=PacketTags.StartTag;
		}
	}
  }

  if (qToDq!=-1) { V=MinStartTag; 
	           MaxFinishTag=max(MaxFinishTag,PacketTags.FinishTag);
	           flow[qToDq].FlowQueue.pop();
		 }
  else if (idle==0) { MaxFinishTag=0;
		      Reset(); 	
	 	      idle=1;
                  }
  return qToDq;
}

dsWF2Qp::dsWF2Qp(int NQ, double LBw):dsScheduler(NQ, LBw) 
{	
     	NumQueues=NQ;	
	LinkBandwith=LBw; 
	/* initialize flow's structure */
  	for (int i = 0; i < MAX_QUEUES; ++i) {
    		flow[i].qcrtSize        = 0;
    		flow[i].weight          = 1;
    		flow[i].S               = 0;
    		flow[i].F               = 0;
  	}
  	V=0; lastTimeV=0;
	//Reset();
}	

void dsWF2Qp::Reset()
{ 
}

void dsWF2Qp::EnqueEvent(Packet* pkt, int flowId)
{
 
  int pktSize=hdr_cmn::access(pkt)->size();
  if (!flow[flowId].qcrtSize) {
      /* If flow queue is empty, calculate start and finish times 
       * paragraph 5.2 formula (23b) and (24)  			  
       */
      flow[flowId].S = max(V, flow[flowId].F);
      flow[flowId].F = flow[flowId].S+pktSize/(flow[flowId].weight*LinkBandwith/8);

      /* update system virtual clock  
       * paragraph 5.2 formula (22)
       */
      double minS = flow[flowId].S;
      for (int i = 0; i < NumQueues; ++i) 
	if ((flow[i].qcrtSize)&&(flow[i].S<minS)) minS=flow[i].S;
      V=max(minS,V);
    }
    flow[flowId].flowQueue.push(pktSize);
    flow[flowId].qcrtSize+=pktSize;
}

int dsWF2Qp::DequeEvent()
{  
  int     i;
  int     pktSize;
  double  minF   = MAXDOUBLE;
  int     flowId = -1;
  double  W = 0;

  
  /* look for the candidate flow with the earliest finish time */
  for (i = 0; i<NumQueues; i++){
    if (flow[i].qcrtSize) { 
                W+=flow[i].weight;
		if ((flow[i].S<=V)&&(flow[i].F<minF)) {
			flowId = i;
			minF   = flow[i].F;
    		}
    }
  }

  if (flowId!=-1) {
	pktSize=flow[flowId].flowQueue.front();
  	flow[flowId].qcrtSize-=pktSize;
 	flow[flowId].flowQueue.pop();	
  
  	/* Set start and finish times of the remaining packets in the queue */
  	if (!flow[flowId].flowQueue.empty()) {
    		flow[flowId].S = flow[flowId].F;
    		flow[flowId].F = flow[flowId].S + flow[flowId].flowQueue.front()/(flow[flowId].weight*LinkBandwith/8);
  	}

  	/* update the virtual clock */
  	/* looking for min service time of eligibles (=active) flows */
	double now=Scheduler::instance().clock();
  	double minS=MAXDOUBLE;
  	for (i = 0; i < NumQueues; ++i) 
    		if ((flow[i].qcrtSize)&&(flow[i].S<minS)) minS=flow[i].S;
	if (minS==MAXDOUBLE) minS=0;

  	/* provided service in the last period, this packet sent */

	W = (now-lastTimeV)/W;
  	V = max(minS,(V+W));
	lastTimeV=now;
  }
  return(flowId);
}

dsLLQ::dsLLQ(int NQ, double LBw, const char* PFQSchedType):dsScheduler(NQ, LBw, PFQSchedType) 
{	
	NumQueues=NQ;
	PFQAssignedBandwith=LBw; 
	if (strcmp(PFQSchedType, "WFQ")==0)
		xPFQ = new dsWFQ(NQ-1,PFQAssignedBandwith);
	else if (strcmp(PFQSchedType, "WF2Qp")==0)
		xPFQ = new dsWF2Qp(NQ-1,PFQAssignedBandwith);
	else if (strcmp(PFQSchedType, "SCFQ")==0)
		xPFQ = new dsSCFQ(NQ-1,PFQAssignedBandwith);
	else if (strcmp(PFQSchedType, "SFQ")==0)
		xPFQ = new dsSFQ(NQ-1,PFQAssignedBandwith);
	xPQ = new dsPQ(1,1);
//	Reset();
}

void dsLLQ::Reset()
{ 
}

void dsLLQ::EnqueEvent(Packet* pkt, int queue)
{   if (queue==0) xPQ->EnqueEvent(pkt, queue);
    else xPFQ->EnqueEvent(pkt, queue-1);	
}

int dsLLQ::DequeEvent()
{ int qToDq=xPQ->DequeEvent();
  if (qToDq==-1) { qToDq=xPFQ->DequeEvent();	
		   if (qToDq>-1) qToDq++;
	         }
  return qToDq;
}


