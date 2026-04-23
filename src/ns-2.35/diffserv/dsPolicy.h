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

#ifndef DS_POLICY_H
#define DS_POLICY_H
#include "dsred.h"
#include "dsconsts.h"
#include "packet.h"

// BUG-10 (2026-04-18): Forward-declare RNG so `Policy::rng_` can be a
// pointer without dragging tools/rng.h into every consumer of this
// header. The full definition is pulled in by dsPolicy.cc only.
class RNG;

#define ANY_HOST -1		// Add to enable point to multipoint policy
#define ANY_PACKET -1
#define FLOW_TIME_OUT 5.0      // The flow does not exist already.
// MAX_POLICIES was 20 in the 2001 original (one policy per DSCP — the
// IP ToS/DSCP field is 6 bits so 64 codepoints is the theoretical cap).
// 2026-04-18 adds per-flow policy entries via
// addFlowPolicyEntry so the table can grow to one entry per (src, dst,
// codePt) triple. Scenario-2-ns235-srtcm registers 500 per-flow entries
// (50 Telnet + 50 FTP + 400 HTTP) plus 2 DSCP-only fallbacks, so we
// raise the cap to 1024.
#define MAX_POLICIES 1024	// Max. size of Policy Table.

#define DUMB 0
#define TSW2CM 1
#define TSW3CM 2
#define TB 3
#define SRTCM 4
#define TRTCM 5
#define FW 6

enum policerType {dumbPolicer, TSW2CMPolicer, TSW3CMPolicer, tokenBucketPolicer, srTCMPolicer, trTCMPolicer, FWPolicer};

enum meterType {dumbMeter, tswTagger, tokenBucketMeter, srTCMMeter, trTCMMeter, fwTagger};


class Policy;
class TBPolicy;

//struct policyTableEntry
struct policyTableEntry {
  nsaddr_t sourceNode, destNode;	// Source-destination pair
  int policy_index;                     // Index to the policy table.
  policerType policer;
  meterType meter;
  int codePt;		     // In-profile code point
  double cir;		     // Committed information rate (bytes per s)
  double cbs;		     // Committed burst size (bytes)
  double cBucket;	     // Current size of committed bucket (bytes)
  double ebs;		     // Excess burst size (bytes)
  double eBucket;	     // Current size of excess bucket (bytes)
  double pir;		     // Peak information rate (bytes per s)
  double pbs;		     // Peak burst size (bytes)
  double pBucket;	     // Current size of peak bucket (bytes)
  double arrivalTime;	     // Arrival time of last packet in TSW metering
  double avgRate, winLen;    // Used for TSW metering
  // 2026-04-18: per-flow three-color output.
  // When `perFlow` is true, PolicyClassifier::mark() synthesizes a
  // policerTableEntry on the stack using (greenCP, yellowCP, redCP)
  // instead of searching the global policer table, allowing per-flow
  // entries sharing the same initial DSCP to carry independent 3-color
  // schemes (e.g. Telnet g=10 y=10 r=12 and HTTP g=10 y=12 r=14). The
  // arrival DSCP is still used as the lookup key (via `codePt`) to find
  // the right per-flow entry.
  bool perFlow;              // true iff this is a per-flow srTCM/trTCM entry
  int greenCP;               // output DSCP for in-profile (conforming)
  int yellowCP;              // output DSCP for CBS overrun (CIR <= rate < PIR)
  int redCP;                 // output DSCP for EBS/PBS overrun
};
	

// This struct specifies the elements of a policer table entry.
struct policerTableEntry {
  policerType policer;
  int initialCodePt;
  int downgrade1;
  int downgrade2;
  int policy_index;
};

// Class PolicyClassifier: keep the policy and polier tables.
class PolicyClassifier : public TclObject {
 public:
  PolicyClassifier();
  void addPolicyEntry(int argc, const char*const* argv);
  // Per-flow variant (2026-04-18):
  // argv[2]=srcNodeId, argv[3]=dstNodeId. The remaining argv fields follow
  // the same schema as addPolicyEntry starting from argv[4] (codePt,
  // policyType, [CIR CBS [EBS|PIR PBS]]). The newly-appended policy entry
  // is back-filled with (sourceNode, destNode) so the three-arg
  // getPolicyTableEntry lookup can disambiguate per-connection state
  // (independent srTCM buckets, etc.). Completes the Nortel/xuanc
  // per-flow hook that was declared but dormant since 2000.
  void addFlowPolicyEntry(int argc, const char*const* argv);
  void addPolicerEntry(int argc, const char*const* argv);
  int mark(Packet *pkt);

  // prints the policy tables
  void printPolicyTable();
  void printPolicerTable();

  // BUG-10: isolate the probabilistic-marking RNG stream for a named
  // policy-pool slot ("TSW2CM", "TSW3CM", or "FW"). Lazy-creates the
  // pool slot if absent, then calls setRngStream(stream) on the Policy
  // instance. Returns 0 on success, -1 on unknown policy name. Callers
  // that never invoke this observe pre-fix behaviour (global default
  // stream).
  int setPolicyRngStream(const char *policyName, int stream);

protected:
  // The table keeps pointers to the real policy
  // Added to support multiple policy per interface.
  Policy *policy_pool[MAX_POLICIES];

  // policy table and its pointer
  policyTableEntry policyTable[MAX_POLICIES];
  int policyTableSize;
  // policer table and its pointer
  policerTableEntry policerTable[MAX_CP];
  int policerTableSize;

  policyTableEntry* getPolicyTableEntry(int codePt);
  // Per-flow lookup (): exact (src,dst,codePt) match
  // first, then (ANY_HOST,ANY_HOST,codePt) fallback. The single-arg
  // overload above is kept for back-compat with DSCP-only policies.
  policyTableEntry* getPolicyTableEntry(nsaddr_t src, nsaddr_t dst,
                                        int codePt);
  policerTableEntry* getPolicerTableEntry(int policy_index, int oldCodePt);
};

// Below are actual policy classes.
// Supper class Policy can't do anything useful.
class Policy : public TclObject {
 public:
  Policy() : rng_(NULL) {};
  virtual ~Policy();

  // Metering and policing methods:
  // Have to initialize all the pointers before actually do anything with them!
  // If not, ok with gcc but not cc!!! Nov 29, xuanc
  virtual void applyMeter(policyTableEntry *policy, Packet *pkt) = 0;
  virtual int applyPolicer(policyTableEntry *policy, policerTableEntry *policer, Packet *pkt) = 0;

  // BUG-10 (2026-04-18): per-policy RNG stream isolation.
  //
  // Pre-fix, TSW2CMPolicy / TSW3CMPolicy / FWPolicy called
  // Random::uniform(0.0, 1.0) directly, which draws from ns-2's global
  // default stream. That stream's position drifts when unrelated
  // RNG-consuming objects are added upstream, so probabilistic marks
  // were not reproducible across module reconfigurations. Calling
  // setRngStream(N) creates a dedicated RNG seeded with stream N; the
  // `uniform()` helper delegates to it when present and falls back to
  // the global default otherwise — so scenarios that do not opt in
  // observe identical pre-fix behaviour.
  void setRngStream(int stream);

 protected:
  // Returns a uniform draw from the isolated stream if configured, else
  // from ns-2's global default stream (backwards-compatible default).
  double uniform();

  RNG *rng_;
};

// DumbPolicy will do nothing, but is a good example to show how to add 
// new policy.
class DumbPolicy : public Policy {
 public:
  DumbPolicy() : Policy(){};

  // Metering and policing methods:
  void applyMeter(policyTableEntry *policy, Packet *pkt);
  int applyPolicer(policyTableEntry *policy, policerTableEntry *policer, Packet *pkt);
};

class TSW2CMPolicy : public Policy {
 public:
  TSW2CMPolicy() : Policy(){};

  // protected:
  // Metering and policing methods:
  void applyMeter(policyTableEntry *policy, Packet *pkt);
  int applyPolicer(policyTableEntry *policy, policerTableEntry *policer, Packet *pkt);
};

class TSW3CMPolicy : public Policy {
 public:
  TSW3CMPolicy() : Policy(){};

  // Metering and policing methods:
  void applyMeter(policyTableEntry *policy, Packet *pkt);
  int applyPolicer(policyTableEntry *policy, policerTableEntry *policer, Packet *pkt);
};

class TBPolicy : public Policy {
 public:
  TBPolicy() : Policy(){};

  // protected:
  // Metering and policing methods:
  void applyMeter(policyTableEntry *policy, Packet *pkt);
  int applyPolicer(policyTableEntry *policy, policerTableEntry *policer, Packet *pkt);
};

class SRTCMPolicy : public Policy {
 public:
  SRTCMPolicy() : Policy(){};

  // Metering and policing methods:
  void applyMeter(policyTableEntry *policy, Packet *pkt);
  int applyPolicer(policyTableEntry *policy, policerTableEntry *policer, Packet *pkt);
};

class TRTCMPolicy : public Policy {
 public:
  TRTCMPolicy() : Policy(){};

  // Metering and policing methods:
  void applyMeter(policyTableEntry *policy, Packet *pkt);
  int applyPolicer(policyTableEntry *policy, policerTableEntry *policer, Packet *pkt);
};

struct flow_entry {
  int fid;
  double last_update;
  int bytes_sent;
  int count;
  struct flow_entry *next;
};

struct flow_list {
  struct flow_entry *head;
  struct flow_entry *tail;
};

class FWPolicy : public Policy {
public:
FWPolicy();
~FWPolicy();

// Metering and policing methods:
void applyMeter(policyTableEntry *policy, Packet *pkt);
int applyPolicer(policyTableEntry *policy, policerTableEntry *policer, Packet *pkt);

void printFlowTable();

 protected:
  // The table to keep the flow states.
  struct flow_list flow_table;
};

#endif
