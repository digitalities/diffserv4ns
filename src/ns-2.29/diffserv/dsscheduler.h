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


#include <queue>
#include "packet.h"	// need Queue class specs
#include <math.h>

class dsScheduler
{
public:
	dsScheduler(int NQ);
	dsScheduler(int NQ, double LBw);
	dsScheduler(int NQ, double LBw, const char* PFQSchedType);
	virtual void Reset(){}
	virtual ~dsScheduler(){}
	virtual void EnqueEvent(Packet* pkt, int Queue){}    		 // update params, if needed
	virtual int  DequeEvent(){return -1;}                            // return queue number to deque
	virtual void AddParam(int, double) {}
	void    UpdateDepartureRate(int Queue,int Prec, int PSize);      // measure of departure rate for each queue
	double  GetDepartureRate(int Queue, int Prec);
protected:
	void applyTSWMeter(int PSize, double *AvgRate, double *ArrTime);
	void initMeter();
	double queueAvgRate[MAX_QUEUES];
	double queueArrTime[MAX_QUEUES];

	double qpAvgRate[MAX_QUEUES][MAX_PREC];
	double qpArrTime[MAX_QUEUES][MAX_PREC];

	int NumQueues;
	double winLen;   // for rate estimator, usually 1
};

class dsRR : public dsScheduler
{
public: 
	dsRR(int NQ);
	virtual ~dsRR(){}
	virtual void Reset();
	virtual void EnqueEvent(Packet* pkt, int Queue);
	virtual int  DequeEvent();	
private:
	int    queueLen[MAX_QUEUES]; // in packets
	int    qToDq;	
};

class dsWRR : public dsScheduler
{
public: 
	dsWRR(int NQ);
	virtual ~dsWRR(){}
	virtual void Reset();
	virtual void EnqueEvent(Packet* pkt, int Queue);
	virtual int  DequeEvent();	
	virtual void AddParam(int Queue, double Weight){queueWeight[Queue]=(int) ceil(Weight);}
private:
	int    queueLen[MAX_QUEUES]; // in packets
	int    queueWeight[MAX_QUEUES]; // in packets
	int    wirrTemp[MAX_QUEUES];
	int    qToDq;	
};

class dsWIRR : public dsScheduler
{
public: 
	dsWIRR(int NQ);
	virtual ~dsWIRR(){}
	virtual void Reset();
	virtual void EnqueEvent(Packet* pkt, int Queue);
	virtual int  DequeEvent();	
	virtual void AddParam(int Queue, double Weight){queueWeight[Queue]=(int) ceil(Weight);}
	void 	printWRRcount();
private:
	int      queueLen[MAX_QUEUES]; // in packets
	int      queueWeight[MAX_QUEUES]; // in packets
	int      wirrTemp[MAX_QUEUES];
	int 	 slicecount[MAX_QUEUES];
	unsigned char wirrqDone[MAX_QUEUES];
	int      qToDq;	
	int      queuesDone;
};



class dsPQ : public dsScheduler
{
public: 
	dsPQ(int NQ);
	dsPQ(int NQ, double WinLen);
	virtual ~dsPQ(){}
	virtual void Reset();
	virtual void EnqueEvent(Packet* pkt, int Queue);
	virtual int  DequeEvent();
	virtual void AddParam(int Queue, double MaxRate){queueMaxRate[Queue]=MaxRate/8;}
private:
	double queueMaxRate[MAX_QUEUES];
	int    queueLen[MAX_QUEUES]; // in packets
	
	
};

class dsWFQ : public dsScheduler, public Handler
{
public: 
	dsWFQ(int NQ);
	dsWFQ(int NQ, double LBw);
	virtual void Reset();
	virtual ~dsWFQ(){}
	virtual void EnqueEvent(Packet* pkt, int Queue);
	virtual int  DequeEvent();
	virtual void AddParam(int Queue, double Weight){fs_[Queue].weight_=Weight;}
	void handle(Event *);	
private:
	double LinkBandwith;
	int idle; 		        // boolean ... GPS reference system is idle? 
	double v_time;		        // virtual time
	double last_vt_update;	        // last virt_time update time
	
	struct flowState {
		queue<double>   GPS;
		queue<double>   PGPS;
    		double weight_;		 	   /* Weight of the flow */
		unsigned int B;	  		   // set of active queues in the GPS reference system
		      			           // B[]!=0 queue is active, ==0 queue is inactive
		double PGPSfinish_t;	           // finish_time for front element of PGPS queue 
		double GPSfinish_t;	           // finish_time for front element of GPS queue
		double finish_t;		   // always last computed finish_time	
  
   	} fs_[MAX_QUEUES];

	int GPSDeparture;
	double sum;			// sum of weights of the activ queues
	Event *wfq_event; 	// this is the event corresponding to the end
				// of the current tx in the GPS reference system
	void scheduleWFQ();
	double sessionDelay;	
	
};

class dsSCFQ : public dsScheduler
{
public: 
	dsSCFQ(int NQ);
	dsSCFQ(int NQ, double LBw);
	virtual void Reset();
	virtual ~dsSCFQ(){}
	virtual void EnqueEvent(Packet* pkt, int Queue);
	virtual int  DequeEvent();
	virtual void AddParam(int Queue, double Weight){session[Queue].weight=Weight;}
	
private:
	double LinkBandwith;
	double tlabel;
	struct session_struct {
		double label;
		queue<double> SessionQueue;
	  	double weight;    // A queue weight per queue
	} session[MAX_QUEUES];
};


class dsWF2Qp : public dsScheduler
{
public: 
	dsWF2Qp(int NQ);
	dsWF2Qp(int NQ, double LBw);
	virtual void Reset();
	virtual ~dsWF2Qp(){}
	virtual void EnqueEvent(Packet* pkt, int Queue);
	virtual int  DequeEvent();
	virtual void AddParam(int Queue, double Weight){flow[Queue].weight=Weight;}
	
private:
	/* flow structure */
   	struct flow_struct {
		queue<int> flowQueue;
    		int qcrtSize;     /* current queue size (in bytes) */
    		double weight;    /* Weight of the flow */
    		double S;         /* Starting time of flow , not checked for wraparound*/
    		double F;         /* Ending time of flow, not checked for wraparound */
   	} flow[MAX_QUEUES];

	
  	double V;            /* Virtual time , not checked for wraparound!*/
	double lastTimeV;
  	double LinkBandwith;
  

};


/* Start-time fair queueing*/
class dsSFQ : public dsScheduler
{
public: 
	dsSFQ(int NQ);
	dsSFQ(int NQ, double LBw);
	virtual void Reset();
	virtual ~dsSFQ(){}
	virtual void EnqueEvent(Packet* pkt, int Queue);
	virtual int  DequeEvent();
	virtual void AddParam(int Queue, double Weight){flow[Queue].weight=Weight;}	
private:
	struct PacketTags_struct {
		double StartTag;
		double FinishTag;
	} PacketTags;
	struct flow_struct {
		double LastFinishTag;
		queue<PacketTags_struct> FlowQueue;
	  	double weight;    // A queue weight per queue
	} flow[MAX_QUEUES];
	double LinkBandwith;
	double V;
	double MaxFinishTag;
	int    idle;
};


/* Low Latency Queueing */
class dsLLQ : public dsScheduler
{
public: 
	dsLLQ(int NQ);
	dsLLQ(int NQ, double LBw, const char* PFQSchedType);
	virtual void Reset();
	virtual ~dsLLQ(){}
	virtual void EnqueEvent(Packet* pkt, int Queue);
	virtual int  DequeEvent();
	virtual void AddParam(int Queue, double Weight){xPFQ->AddParam(Queue-1,Weight);}	
private:	
	double PFQAssignedBandwith;
	dsScheduler* xPFQ;
	dsPQ*        xPQ;
};






