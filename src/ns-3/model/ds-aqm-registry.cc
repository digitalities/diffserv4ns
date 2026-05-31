/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "ds-aqm-registry.h"

#include "ns3/abort.h"
#include "ns3/boolean.h"
#include "ns3/diffserv-edge-queue-disc.h"
#include "ns3/ds-cake-helper.h"
#include "ns3/ds-l4s-queue-disc.h"
#include "ns3/ds-red-queue-disc.h"
#include "ns3/object-factory.h"
#include "ns3/red-queue-disc.h"

#include <algorithm>

namespace ns3
{
namespace diffserv
{
namespace aqm_eval
{

namespace
{

Ptr<QueueDisc>
MakeFromTypeId(const std::string& typeId)
{
    ObjectFactory factory;
    factory.SetTypeId(typeId);
    Ptr<Object> obj = factory.Create<Object>();
    Ptr<QueueDisc> qdisc = DynamicCast<QueueDisc>(obj);
    NS_ABORT_MSG_IF(!qdisc, "TypeId " << typeId << " did not yield a QueueDisc");
    return qdisc;
}

Ptr<QueueDisc>
MakeRedFlavour(const std::string& flagAttr)
{
    Ptr<RedQueueDisc> q = CreateObject<RedQueueDisc>();
    q->SetAttribute(flagAttr, BooleanValue(true));
    return q;
}

Ptr<QueueDisc>
MakeDsRed()
{
    // Vanilla-style single-class DsRed.  The four traps documented in
    // aqm-eval-runner.cc (RIO_C thMin=0, empty PHB table, DROP_TAIL
    // misnomer, default qlim=50) are mitigated here in a fixed order:
    // Initialize() to materialise children, then SetMredMode(WRED) for
    // EWMA averaging, SetQueueLimit(0,25) and ConfigQueue with
    // mainline-RED-equivalent thresholds (5/15 packets), then PHB
    // entries for BE and EF so LookupPhb succeeds for both probe and
    // bulk DSCPs.
    Ptr<DsRedQueueDisc> q = CreateObject<DsRedQueueDisc>();
    q->Initialize();
    q->SetMredMode(MredMode::WRED);
    q->SetQueueLimit(0, 25);
    q->ConfigQueue(0, 0, 5.0, 15.0, 0.1);
    q->AddPhbEntry(0, 0, 0);
    q->AddPhbEntry(46, 0, 0);
    return q;
}

Ptr<QueueDisc>
MakeDsL4s(DsL4sQueueDisc::ClassicAqm mode)
{
    Ptr<DsL4sQueueDisc> disc = CreateObject<DsL4sQueueDisc>();
    disc->SetL4sQueueIdx(1);
    disc->SetNumQueues(2);
    disc->SetClassicAqm(mode);
    // qlim differs by mode: Wred uses 25-packet queue (small,
    // RED-style early drop dominates); CoupledOnly uses 200 packets
    // for the pass-through FIFO.
    disc->SetQueueLimit(0, (mode == DsL4sQueueDisc::ClassicAqm::Wred) ? 25 : 200);
    disc->AddPhbEntry(0, 0, 0);
    disc->AddPhbEntry(46, 0, 0);
    return disc;
}

Ptr<QueueDisc>
MakeDsCake(DataRate totalRate)
{
    Ptr<DiffServEdgeQueueDisc> edge = CreateObject<DiffServEdgeQueueDisc>();
    DsCakeHelper::SetAsCakeDiffserv4(edge, totalRate);
    return edge;
}

} // namespace

const char*
FamilyName(AqmEntry::Family f)
{
    switch (f)
    {
    case AqmEntry::Family::Single:
        return "single";
    case AqmEntry::Family::Fq:
        return "fq";
    case AqmEntry::Family::Ds4:
        return "ds4";
    }
    return "?";
}

void
SerialiseAqmEntry(std::ostream& os, const AqmEntry& e)
{
    os << "{"
       << "\"name\": \"" << e.name << "\", "
       << "\"fileTag\": \"" << e.fileTag << "\", "
       << "\"displayName\": \"" << e.displayName << "\", "
       << "\"family\": \"" << FamilyName(e.family) << "\", "
       << "\"supportsEcn\": " << (e.supportsEcn ? "true" : "false") << "}";
}

AqmRegistry::AqmRegistry()
{
    using F = AqmEntry::Family;

    // Mainline single-queue AQMs.
    Register({"ns3::PfifoFastQueueDisc", "PfifoFast", "PfifoFast", F::Single, false, [](DataRate) {
                  return MakeFromTypeId("ns3::PfifoFastQueueDisc");
              }});
    Register({"ns3::RedQueueDisc", "Red", "RED", F::Single, true, [](DataRate) {
                  return MakeFromTypeId("ns3::RedQueueDisc");
              }});
    Register(
        {"ns3::AdaptiveRedQueueDisc", "AdaptiveRed", "Adaptive RED", F::Single, true, [](DataRate) {
             return MakeRedFlavour("ARED");
         }});
    Register({"ns3::CoDelQueueDisc", "CoDel", "CoDel", F::Single, true, [](DataRate) {
                  return MakeFromTypeId("ns3::CoDelQueueDisc");
              }});
    Register({"ns3::PieQueueDisc", "Pie", "PIE", F::Single, true, [](DataRate) {
                  return MakeFromTypeId("ns3::PieQueueDisc");
              }});
    Register({"ns3::CobaltQueueDisc", "Cobalt", "Cobalt", F::Single, true, [](DataRate) {
                  return MakeFromTypeId("ns3::CobaltQueueDisc");
              }});

    // Mainline flow-queue AQMs.
    Register({"ns3::FqCoDelQueueDisc", "FqCoDel", "FQ-CoDel", F::Fq, true, [](DataRate) {
                  return MakeFromTypeId("ns3::FqCoDelQueueDisc");
              }});
    Register({"ns3::FqPieQueueDisc", "FqPie", "FQ-PIE", F::Fq, true, [](DataRate) {
                  return MakeFromTypeId("ns3::FqPieQueueDisc");
              }});
    Register({"ns3::FqCobaltQueueDisc", "FqCobalt", "FQ-Cobalt", F::Fq, true, [](DataRate) {
                  return MakeFromTypeId("ns3::FqCobaltQueueDisc");
              }});

    // DS4-aware composites.
    Register({"DsRed", "DsRed", "DiffServ RED (RIO-C)", F::Ds4, false, [](DataRate) {
                  return MakeDsRed();
              }});
    Register({"DsL4sWred", "DsL4sWred", "DualPI2 (classic = WRED)", F::Ds4, true, [](DataRate) {
                  return MakeDsL4s(DsL4sQueueDisc::ClassicAqm::Wred);
              }});
    Register({"DsL4sCoupledOnly",
              "DsL4sCoupledOnly",
              "DualPI2 (coupled-only)",
              F::Ds4,
              true,
              [](DataRate) { return MakeDsL4s(DsL4sQueueDisc::ClassicAqm::CoupledOnly); }});
    Register({"DsCake", "DsCake", "DiffServ CAKE (diffserv4)", F::Ds4, true, [](DataRate r) {
                  return MakeDsCake(r);
              }});
}

const AqmRegistry&
AqmRegistry::Get()
{
    static const AqmRegistry kInstance;
    return kInstance;
}

const AqmEntry*
AqmRegistry::FindByName(const std::string& name) const
{
    auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const AqmEntry& e) {
        return e.name == name;
    });
    return (it == m_entries.end()) ? nullptr : &(*it);
}

std::vector<std::string>
AqmRegistry::Names() const
{
    std::vector<std::string> out;
    out.reserve(m_entries.size());
    for (const auto& e : m_entries)
    {
        out.push_back(e.name);
    }
    return out;
}

Ptr<QueueDisc>
AqmRegistry::Make(const std::string& name, DataRate totalRate) const
{
    const AqmEntry* e = FindByName(name);
    NS_ABORT_MSG_IF(!e, "AqmRegistry: unknown AQM '" << name << "'");
    return e->factory(totalRate);
}

} // namespace aqm_eval
} // namespace diffserv
} // namespace ns3
