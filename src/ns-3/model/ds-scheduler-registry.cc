/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "ds-scheduler-registry.h"

#include "ds-llq-scheduler.h"
#include "ds-pq-scheduler.h"
#include "ds-rr-scheduler.h"
#include "ds-scfq-scheduler.h"
#include "ds-sfq-scheduler.h"
#include "ds-wf2qp-scheduler.h"
#include "ds-wfq-scheduler.h"
#include "ds-wirr-scheduler.h"
#include "ds-wrr-scheduler.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/object-factory.h"
#include "ns3/uinteger.h"

#include <algorithm>

namespace ns3
{
namespace diffserv
{

namespace
{

void
ApplyWeights(Ptr<DsScheduler> sched, const std::vector<double>& weights)
{
    for (uint32_t i = 0; i < weights.size(); ++i)
    {
        sched->SetParam(i, weights[i]);
    }
}

} // namespace

const char*
FamilyName(SchedulerEntry::Family f)
{
    switch (f)
    {
    case SchedulerEntry::Family::Priority:
        return "priority";
    case SchedulerEntry::Family::RoundRobin:
        return "round-robin";
    case SchedulerEntry::Family::FairQueue:
        return "fair-queue";
    case SchedulerEntry::Family::Hybrid:
        return "hybrid";
    }
    return "?";
}

const char*
ParameterShapeName(SchedulerEntry::ParameterShape s)
{
    switch (s)
    {
    case SchedulerEntry::ParameterShape::None:
        return "none";
    case SchedulerEntry::ParameterShape::PriorityWinLen:
        return "priority-winlen";
    case SchedulerEntry::ParameterShape::RoundRobinWeights:
        return "rr-weights";
    case SchedulerEntry::ParameterShape::FairQueueShares:
        return "fq-shares";
    case SchedulerEntry::ParameterShape::HybridLlq:
        return "hybrid-llq";
    }
    return "?";
}

void
SerialiseSchedulerEntry(std::ostream& os, const SchedulerEntry& e)
{
    os << "{"
       << "\"fileTag\": \"" << e.fileTag << "\", "
       << "\"displayName\": \"" << e.displayName << "\", "
       << "\"family\": \"" << FamilyName(e.family) << "\", "
       << "\"parameterShape\": \"" << ParameterShapeName(e.parameterShape) << "\", "
       << "\"needsLinkBandwidth\": " << (e.needsLinkBandwidth ? "true" : "false") << ", "
       << "\"description\": \"" << e.description << "\"}";
}

SchedulerRegistry::SchedulerRegistry()
{
    using F = SchedulerEntry::Family;
    using P = SchedulerEntry::ParameterShape;

    Register({"pq",
              "PQ",
              F::Priority,
              P::PriorityWinLen,
              false,
              "Strict priority — queue 0 served first; WinLen sets the "
              "per-tick service quantum",
              [](const SchedulerArgs& a) -> Ptr<DsScheduler> {
                  return CreateObjectWithAttributes<DsPriorityScheduler>(
                      "NumQueues",
                      UintegerValue(a.numQueues),
                      "WinLen",
                      DoubleValue(a.winLen));
              }});

    Register({"rr",
              "RR",
              F::RoundRobin,
              P::None,
              false,
              "Plain round-robin across all queues; no weights",
              [](const SchedulerArgs& a) -> Ptr<DsScheduler> {
                  return CreateObjectWithAttributes<DsRoundRobinScheduler>(
                      "NumQueues",
                      UintegerValue(a.numQueues));
              }});

    Register({"wrr",
              "WRR",
              F::RoundRobin,
              P::RoundRobinWeights,
              false,
              "Weighted round-robin; per-queue integer-style weights via SetParam",
              [](const SchedulerArgs& a) -> Ptr<DsScheduler> {
                  auto s = CreateObjectWithAttributes<DsWeightedRoundRobinScheduler>(
                      "NumQueues",
                      UintegerValue(a.numQueues));
                  ApplyWeights(s, a.weights);
                  return s;
              }});

    Register({"wirr",
              "WIRR",
              F::RoundRobin,
              P::RoundRobinWeights,
              false,
              "Weighted interleaved round-robin; per-queue integer weights",
              [](const SchedulerArgs& a) -> Ptr<DsScheduler> {
                  auto s = CreateObjectWithAttributes<DsWeightedInterleavedRoundRobinScheduler>(
                      "NumQueues",
                      UintegerValue(a.numQueues));
                  ApplyWeights(s, a.weights);
                  return s;
              }});

    Register({"scfq",
              "SCFQ",
              F::FairQueue,
              P::FairQueueShares,
              true,
              "Self-clocked fair queueing; per-queue fractional weights summing to 1",
              [](const SchedulerArgs& a) -> Ptr<DsScheduler> {
                  auto s = CreateObjectWithAttributes<DsScfqScheduler>(
                      "NumQueues",
                      UintegerValue(a.numQueues),
                      "LinkBandwidth",
                      DoubleValue(a.linkBps));
                  ApplyWeights(s, a.weights);
                  return s;
              }});

    Register({"sfq",
              "SFQ",
              F::FairQueue,
              P::FairQueueShares,
              true,
              "Start-time fair queueing; per-queue fractional weights summing to 1",
              [](const SchedulerArgs& a) -> Ptr<DsScheduler> {
                  auto s = CreateObjectWithAttributes<DsSfqScheduler>(
                      "NumQueues",
                      UintegerValue(a.numQueues),
                      "LinkBandwidth",
                      DoubleValue(a.linkBps));
                  ApplyWeights(s, a.weights);
                  return s;
              }});

    Register({"wfq",
              "WFQ",
              F::FairQueue,
              P::FairQueueShares,
              true,
              "Parekh-Gallager PGPS — true V(t) snapshot; per-queue fractional weights",
              [](const SchedulerArgs& a) -> Ptr<DsScheduler> {
                  auto s = CreateObjectWithAttributes<DsWfqScheduler>(
                      "NumQueues",
                      UintegerValue(a.numQueues),
                      "LinkBandwidth",
                      DoubleValue(a.linkBps));
                  ApplyWeights(s, a.weights);
                  return s;
              }});

    Register({"wf2qp",
              "WF2Q+",
              F::FairQueue,
              P::FairQueueShares,
              true,
              "Worst-case fair WFQ+ (Bennett-Zhang 2001 time-discrete); per-queue weights",
              [](const SchedulerArgs& a) -> Ptr<DsScheduler> {
                  auto s = CreateObjectWithAttributes<DsWf2qPlusScheduler>(
                      "NumQueues",
                      UintegerValue(a.numQueues),
                      "LinkBandwidth",
                      DoubleValue(a.linkBps));
                  ApplyWeights(s, a.weights);
                  return s;
              }});

    Register({"llq",
              "LLQ",
              F::Hybrid,
              P::HybridLlq,
              true,
              "Cisco LLQ: queue 0 is strict-priority slot (weight=0 sentinel); "
              "queues 1..N share residual via WFQ-style weights summing to 1",
              [](const SchedulerArgs& a) -> Ptr<DsScheduler> {
                  auto s = CreateObjectWithAttributes<DsLlqScheduler>(
                      "NumQueues",
                      UintegerValue(a.numQueues),
                      "LinkBandwidth",
                      DoubleValue(a.linkBps));
                  ApplyWeights(s, a.weights);
                  return s;
              }});
}

const SchedulerRegistry&
SchedulerRegistry::Get()
{
    static const SchedulerRegistry kInstance;
    return kInstance;
}

Ptr<DsScheduler>
SchedulerRegistry::Construct(const std::string& fileTag, const SchedulerArgs& args) const
{
    const SchedulerEntry* e = Find(fileTag);
    NS_ABORT_MSG_IF(!e, "SchedulerRegistry: unknown scheduler '" << fileTag << "'");
    return e->construct(args);
}

std::vector<std::string>
SchedulerRegistry::FileTags() const
{
    std::vector<std::string> out;
    out.reserve(m_entries.size());
    for (const auto& e : m_entries)
    {
        out.push_back(e.fileTag);
    }
    return out;
}

} // namespace diffserv
} // namespace ns3
