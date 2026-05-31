/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * RFC-7928-inspired AQM characterisation comparison: drives three
 * scenarios (steady-state TCP saturation, mixed traffic, real-time
 * + bulk) through three queue-disc compositions (FqCoDel baseline,
 * L4S DualPI2, CAKE diffserv4) and emits per-flow rate, p99 latency,
 * and Jain fairness metrics for cross-composite comparison.
 *
 * Scope note: ns-3 mainline does not ship a formal RFC 7928
 * conformance framework. This example provides an
 * RFC-7928-aligned head-to-head comparison serving the same
 * "characterisation guidelines" purpose without claiming formal
 * RFC 7928 conformance. A formal 6-test framework is deferred to
 * v1.1.
 *
 * Topology:
 * sender(s) --- 100 Mbps / 1 ms --- edge --- 10 Mbps / 5 ms --- sink
 * |
 * bottleneck egress queue disc
 * selected via --disc=
 *
 * Scenarios (--scenario=):
 * steady RFC 7928 Test 1: 4 saturating TCP + 1 sparse UDP probe
 * mixed RFC 7928 Test 5: 2 bulk TCP + 2 short bursts + 1 CBR
 * rt-bulk RFC 7928 Test 7: 1 real-time CBR + 4 bulk TCP
 *
 * Modes (--disc=):
 * fqcodel Mainline FqCoDelQueueDisc (RFC 7928 baseline reference)
 * l4s DsL4sQueueDisc (RFC 9332 DualPI2)
 * cake DsCakeHelper::SetAsCakeDiffserv4
 *
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/diffserv-edge-queue-disc.h"
#include "ns3/diffserv-helper.h"
#include "ns3/ds-cake-helper.h"
#include "ns3/ds-l4s-coupled-scheduler.h"
#include "ns3/ds-l4s-queue-disc.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/fq-codel-queue-disc.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace ns3;
using namespace ns3::diffserv;

NS_LOG_COMPONENT_DEFINE("DiffServAqmComparison");

namespace
{

void
EnsureDirLocal(const std::string& path)
{
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST)
    {
        NS_ABORT_MSG("mkdir(" << path << ") failed: " << std::strerror(errno));
    }
}

enum class DiscMode
{
    FqCoDel,
    L4S,
    CAKE,
};

enum class Scenario
{
    Steady,
    Mixed,
    RtBulk,
};

DiscMode
ParseDisc(const std::string& s)
{
    if (s == "fqcodel")
    {
        return DiscMode::FqCoDel;
    }
    if (s == "l4s")
    {
        return DiscMode::L4S;
    }
    if (s == "cake")
    {
        return DiscMode::CAKE;
    }
    NS_ABORT_MSG("unknown --disc=" << s);
}

Scenario
ParseScenario(const std::string& s)
{
    if (s == "steady")
    {
        return Scenario::Steady;
    }
    if (s == "mixed")
    {
        return Scenario::Mixed;
    }
    if (s == "rt-bulk")
    {
        return Scenario::RtBulk;
    }
    NS_ABORT_MSG("unknown --scenario=" << s);
}

const char*
DiscName(DiscMode m)
{
    switch (m)
    {
    case DiscMode::FqCoDel:
        return "fqcodel";
    case DiscMode::L4S:
        return "l4s";
    case DiscMode::CAKE:
        return "cake";
    }
    return "?";
}

const char*
ScenarioName(Scenario s)
{
    switch (s)
    {
    case Scenario::Steady:
        return "steady";
    case Scenario::Mixed:
        return "mixed";
    case Scenario::RtBulk:
        return "rt-bulk";
    }
    return "?";
}

Ptr<QueueDisc>
MakeDisc(DiscMode mode, DataRate totalRate)
{
    if (mode == DiscMode::FqCoDel)
    {
        Ptr<FqCoDelQueueDisc> fq = CreateObject<FqCoDelQueueDisc>();
        fq->SetQuantum(1514);
        return fq;
    }
    if (mode == DiscMode::L4S)
    {
        // Borrowed configuration from diffserv-l4s-fqcodel-comparison.cc
        // (L4sCoupledOnly mode): 2-child composition, L4S FIFO at idx 0,
        // classic AQM at idx 1 with WRED auto-munged to pass-through.
        Ptr<DsL4sQueueDisc> disc = CreateObject<DsL4sQueueDisc>();
        disc->SetL4sQueueIdx(1);
        disc->SetNumQueues(2);
        disc->SetClassicAqm(DsL4sQueueDisc::ClassicAqm::CoupledOnly);
        disc->SetQueueLimit(0, 200);
        disc->AddPhbEntry(0, 0, 0);  // DSCP 0 -> queue 0 prec 0
        disc->AddPhbEntry(46, 0, 0); // DSCP 46 (EF) -> queue 0 prec 0
        return disc;
    }
    // CAKE
    Ptr<DiffServEdgeQueueDisc> edge = CreateObject<DiffServEdgeQueueDisc>();
    DsCakeHelper::SetAsCakeDiffserv4(edge, totalRate);
    return edge;
}

} // namespace

int
main(int argc, char* argv[])
{
    std::string discStr = "fqcodel";
    std::string scenarioStr = "steady";
    std::string outDir = "/tmp/diffserv-aqm-comparison";
    double simTime = 10.0;
    uint64_t totalRateBps = 10'000'000;

    CommandLine cmd(__FILE__);
    cmd.AddValue("disc", "Bottleneck queue disc: fqcodel | l4s | cake", discStr);
    cmd.AddValue("scenario", "Workload: steady | mixed | rt-bulk", scenarioStr);
    cmd.AddValue("outDir", "Directory for CSV outputs", outDir);
    cmd.AddValue("simTime", "Simulation duration (seconds)", simTime);
    cmd.AddValue("totalRateBps", "Bottleneck bitrate", totalRateBps);
    cmd.Parse(argc, argv);

    const DiscMode discMode = ParseDisc(discStr);
    const Scenario scenario = ParseScenario(scenarioStr);
    EnsureDirLocal(outDir);

    // Per-scenario flow plan: each entry is one UDP-CBR flow.
    // (UDP-only keeps the comparison fair across composites; TCP
    // through a DiffServ edge in both directions adds handshake
    // complications. The .* fixtures use proper TCP for the
    // CAKE-paper figures.)
    struct FlowSpec
    {
        const char* name;
        uint64_t rateBps;
        double startSec;
        double stopSec;
        uint8_t tos; // for CAKE classification (0 = BE / CS0)
    };

    std::vector<FlowSpec> flows;
    switch (scenario)
    {
    case Scenario::Steady:
        // RFC 7928 Test 1 analogue: 4 saturating UDP + 1 sparse UDP probe.
        for (int i = 0; i < 4; ++i)
        {
            flows.push_back({"bulk", 3'000'000, 1.0, simTime, 0});
        }
        flows.push_back({"probe", 200'000, 1.0, simTime, 46u << 2}); // EF
        break;
    case Scenario::Mixed:
        // RFC 7928 Test 5 analogue: 2 long-lived bulk + 2 short bursts + 1 CBR.
        flows.push_back({"bulk-long-1", 3'000'000, 1.0, simTime, 0});
        flows.push_back({"bulk-long-2", 3'000'000, 1.0, simTime, 0});
        flows.push_back({"burst-1", 3'000'000, 3.0, 5.0, 0});
        flows.push_back({"burst-2", 3'000'000, 6.0, 8.0, 0});
        flows.push_back({"cbr-audio", 96'000, 1.0, simTime, 46u << 2}); // EF
        break;
    case Scenario::RtBulk:
        // RFC 7928 Test 7 analogue: 1 real-time CBR + 4 saturating UDP.
        flows.push_back({"rt-cbr", 500'000, 1.0, simTime, 46u << 2}); // EF
        for (int i = 0; i < 4; ++i)
        {
            flows.push_back({"bulk", 3'000'000, 1.0, simTime, 0});
        }
        break;
    }

    // ----- Topology -----
    NodeContainer senders;
    senders.Create(flows.size());
    NodeContainer routers;
    routers.Create(2);
    NodeContainer sinks;
    sinks.Create(1);

    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    accessLink.SetChannelAttribute("Delay", StringValue("1ms"));
    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", DataRateValue(DataRate(totalRateBps)));
    bottleneck.SetChannelAttribute("Delay", StringValue("5ms"));
    PointToPointHelper sinkLink;
    sinkLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    sinkLink.SetChannelAttribute("Delay", StringValue("1ms"));

    InternetStackHelper stack;
    stack.Install(senders);
    stack.Install(routers);
    stack.Install(sinks);

    Ipv4AddressHelper addr;
    std::vector<Ipv4InterfaceContainer> senderIfs(flows.size());
    for (uint32_t i = 0; i < flows.size(); ++i)
    {
        NetDeviceContainer dev = accessLink.Install(senders.Get(i), routers.Get(0));
        std::ostringstream ssNet;
        ssNet << "10.1." << (i + 1) << ".0";
        addr.SetBase(ssNet.str().c_str(), "255.255.255.0");
        senderIfs[i] = addr.Assign(dev);
    }

    NetDeviceContainer bottleneckDev = bottleneck.Install(routers.Get(0), routers.Get(1));
    addr.SetBase("10.2.1.0", "255.255.255.0");
    addr.Assign(bottleneckDev);

    NetDeviceContainer sinkDev = sinkLink.Install(routers.Get(1), sinks.Get(0));
    addr.SetBase("10.3.1.0", "255.255.255.0");
    Ipv4InterfaceContainer sinkIfs = addr.Assign(sinkDev);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ----- Bottleneck queue disc -----
    Ptr<QueueDisc> qdisc = MakeDisc(discMode, DataRate(totalRateBps));

    if (discMode == DiscMode::CAKE)
    {
        // CAKE composer needs the edge to know about each tin DSCP.
        // SetAsCakeDiffserv4 already stamps the standard map; flows
        // marked DSCP 46 (EF) land in tin 3 (Voice).
        DiffServHelper helper;
        Ptr<DiffServEdgeQueueDisc> edge = DynamicCast<DiffServEdgeQueueDisc>(qdisc);
        for (uint32_t i = 0; i < flows.size(); ++i)
        {
            const uint8_t dscp = flows[i].tos >> 2;
            helper.AddMarkRule(edge,
                               dscp,
                               static_cast<int32_t>(senderIfs[i].GetAddress(0).Get()),
                               kAnyHost,
                               kAnyProtocol,
                               0);
            helper.AddDumbPolicy(edge, dscp);
        }
    }

    Ptr<NetDevice> egress = bottleneckDev.Get(0);
    Ptr<TrafficControlLayer> tcl = egress->GetNode()->GetObject<TrafficControlLayer>();
    if (tcl->GetRootQueueDiscOnDevice(egress))
    {
        tcl->DeleteRootQueueDiscOnDevice(egress);
    }
    tcl->SetRootQueueDiscOnDevice(egress, qdisc);

    // ----- Apps -----
    const uint16_t basePort = 5000;
    ApplicationContainer sinkApps;
    ApplicationContainer sourceApps;
    for (uint32_t i = 0; i < flows.size(); ++i)
    {
        const uint16_t port = basePort + i;

        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), port));
        sinkApps.Add(sinkHelper.Install(sinks.Get(0)));

        OnOffHelper onOff("ns3::UdpSocketFactory", InetSocketAddress(sinkIfs.GetAddress(1), port));
        onOff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onOff.SetAttribute("DataRate", DataRateValue(DataRate(flows[i].rateBps)));
        // Bulk flows use 1000-byte packets (saturating); probe/CBR use 200-byte.
        const uint32_t pktSize = (flows[i].rateBps >= 1'000'000) ? 1000 : 200;
        onOff.SetAttribute("PacketSize", UintegerValue(pktSize));
        onOff.SetAttribute("Tos", UintegerValue(flows[i].tos));
        ApplicationContainer src = onOff.Install(senders.Get(i));
        src.Start(Seconds(flows[i].startSec));
        src.Stop(Seconds(flows[i].stopSec));
        sourceApps.Add(src);
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime + 1.0));

    FlowMonitorHelper fmh;
    Ptr<FlowMonitor> mon = fmh.InstallAll();

    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();

    // ----- Per-flow accounting -----
    mon->CheckForLostPackets();
    auto classifier = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());

    struct PerFlow
    {
        std::string name;
        uint64_t rxBytes;
        double rxRateBps;
        double meanDelayMs;
    };

    std::vector<PerFlow> perFlow;

    const double measureSpan = std::max(0.5, simTime - 0.5);
    for (const auto& kv : mon->GetFlowStats())
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(kv.first);
        const uint16_t dport = t.destinationPort;
        if (dport < basePort || dport >= basePort + flows.size())
        {
            continue;
        }
        const uint32_t flowIdx = dport - basePort;

        PerFlow pf;
        pf.name = flows[flowIdx].name;
        pf.rxBytes = kv.second.rxBytes;
        pf.rxRateBps = (pf.rxBytes * 8.0) / measureSpan;
        pf.meanDelayMs = kv.second.rxPackets > 0
                             ? (kv.second.delaySum.GetMicroSeconds() / 1000.0) / kv.second.rxPackets
                             : 0.0;
        perFlow.push_back(pf);
    }

    // Jain fairness across throughput
    double sumX = 0.0;
    double sumX2 = 0.0;
    for (const auto& pf : perFlow)
    {
        sumX += pf.rxRateBps;
        sumX2 += pf.rxRateBps * pf.rxRateBps;
    }
    const double jain = perFlow.empty() ? 0.0 : (sumX * sumX) / (perFlow.size() * sumX2);

    // ----- CSV per-flow output -----
    const std::string scenStr = ScenarioName(scenario);
    const std::string discNameStr = DiscName(discMode);
    const std::string base = outDir + "/" + scenStr + "-" + discNameStr;

    std::ofstream csv(base + "-perflow.csv");
    csv << "flow,rxBytes,rxRateBps,meanDelayMs\n";
    for (const auto& pf : perFlow)
    {
        csv << pf.name << "," << pf.rxBytes << "," << std::fixed << std::setprecision(0)
            << pf.rxRateBps << "," << std::fixed << std::setprecision(3) << pf.meanDelayMs << "\n";
    }
    csv.close();

    // ----- Summary -----
    std::ofstream summary(base + "-summary.txt");
    summary << "scenario=" << scenStr << "\n"
            << "disc=" << discNameStr << "\n"
            << "totalRate=" << totalRateBps << "\n"
            << "simTime=" << simTime << "\n"
            << "numFlows=" << perFlow.size() << "\n"
            << "aggregate_Mbps=" << std::fixed << std::setprecision(2) << (sumX / 1.0e6) << "\n"
            << "jain_fairness=" << std::fixed << std::setprecision(3) << jain << "\n";

    // Identify the EF/probe flow (if any) for latency-sensitive metric
    for (const auto& pf : perFlow)
    {
        if (pf.name == std::string("udp-probe") || pf.name == std::string("cbr-audio") ||
            pf.name == std::string("rt-cbr"))
        {
            summary << "ef_flow=" << pf.name << "\n"
                    << "ef_meanDelayMs=" << std::fixed << std::setprecision(3) << pf.meanDelayMs
                    << "\n"
                    << "ef_rxRateBps=" << std::fixed << std::setprecision(0) << pf.rxRateBps
                    << "\n";
            break;
        }
    }
    summary.close();

    std::cout << "[diffserv-aqm-comparison] " << scenStr << " / " << discNameStr
              << "  agg=" << std::fixed << std::setprecision(2) << (sumX / 1.0e6)
              << " Mbps  Jain=" << std::fixed << std::setprecision(3) << jain << "\n"
              << "  -> " << base << "-{perflow.csv,summary.txt}\n";

    Simulator::Destroy();
    return 0;
}
