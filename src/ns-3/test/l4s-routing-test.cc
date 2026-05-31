/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Tests for DsL4sQueueDisc covering enqueue-side classification
 * and DualPI2 coupling invariants per RFC 9332 §4.1.
 */

#include "ns3/ds-l4s-coupled-scheduler.h"
#include "ns3/ds-l4s-queue-disc.h"
#include "ns3/ds-pq-scheduler.h"
#include "ns3/fq-codel-queue-disc.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"

#include <cmath>

// Scenario-validation test class definitions — textually included so the
// DsL4sQueueDiscSuite constructor can instantiate them with 'new'. The file
// is NOT listed separately in CMakeLists.txt TEST_SOURCES.
#include "l4s-scenario-validation-test.cc"

using namespace ns3;
using namespace ns3::diffserv;

namespace
{

/// Build a minimal Ipv4QueueDiscItem with the given DSCP and ECN.
Ptr<Ipv4QueueDiscItem>
MakeItem(Ipv4Header::DscpType dscp, Ipv4Header::EcnType ecn, uint32_t payloadSize = 500)
{
    Ipv4Header hdr;
    hdr.SetSource(Ipv4Address("10.0.0.1"));
    hdr.SetDestination(Ipv4Address("10.0.0.2"));
    hdr.SetProtocol(17); // UDP
    hdr.SetDscp(dscp);
    hdr.SetEcn(ecn);
    hdr.SetPayloadSize(payloadSize);

    Ptr<Packet> pkt = Create<Packet>(payloadSize);
    return Create<Ipv4QueueDiscItem>(pkt, Address(), 0x0800, hdr);
}

/// Build and initialize a 2-queue L4S disc with WRED thresholds wide
/// enough that the parent's WRED never force-drops. Returns the disc
/// with classic at idx 0 and L4S at idx 1.
Ptr<DsL4sQueueDisc>
MakeL4sDisc(uint32_t classicQlim = 10000, uint32_t l4sQlim = 10000)
{
    auto disc = CreateObject<DsL4sQueueDisc>();
    disc->SetNumQueues(2);
    disc->SetL4sQueueIdx(1);
    disc->SetQueueLimit(0, classicQlim);
    disc->SetQueueLimit(1, l4sQlim);

    // Classic DSCP 0 -> queue 0 prec 0; L4S bypasses PHB lookup.
    disc->AddPhbEntry(0, 0, 0);

    Ptr<DsPriorityScheduler> sched =
        CreateObjectWithAttributes<DsPriorityScheduler>("NumQueues", UintegerValue(2));
    disc->SetScheduler(sched);

    disc->Initialize();

    // Generous WRED thresholds keep the parent's RIO machinery inert
    // throughout the coupling-invariant tests; only the L4S coupling
    // logic should drive drops/marks.
    disc->ConfigQueue(0, 0, 5000.0, 10000.0, 0.1);
    disc->ConfigQueue(1, 0, 5000.0, 10000.0, 0.1);
    return disc;
}

/// S-L4S.1: ECT(1)/CE route to the L4S sub-queue; NotECT/ECT(0) route to
/// classic.
class DsL4sRoutingTest : public TestCase
{
  public:
    DsL4sRoutingTest()
        : TestCase("L4S enqueue-side routing by ECN codepoint")
    {
    }

    void DoRun() override
    {
        auto disc = CreateObject<DsL4sQueueDisc>();
        disc->SetNumQueues(2);
        disc->SetL4sQueueIdx(1);

        // PHB entry for classic traffic: DSCP 0 (DscpDefault) -> queue 0.
        // L4S traffic bypasses PHB lookup entirely, so no entry needed
        // for it.
        disc->AddPhbEntry(0, 0, 0);

        // Explicit PQ scheduler for determinism; default RR would also work.
        Ptr<DsPriorityScheduler> sched =
            CreateObjectWithAttributes<DsPriorityScheduler>("NumQueues", UintegerValue(2));
        disc->SetScheduler(sched);

        disc->Initialize();

        // Set MRED thresholds large enough that WRED never force-drops
        // within this test. Default thMin=thMax=0 triggers "above thMax"
        // forced drop on the first packet that raises vAve > 0.
        disc->ConfigQueue(0, 0, 100.0, 200.0, 0.1);
        disc->ConfigQueue(1, 0, 100.0, 200.0, 0.1);

        // Packet 1: NotECT with DSCP 0 — classic path, goes to queue 0.
        Ptr<Ipv4QueueDiscItem> classicPkt =
            MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_NotECT);
        NS_TEST_ASSERT_MSG_EQ(disc->Enqueue(classicPkt), true, "Classic packet must enqueue");

        // The composer's direct children are fixed — child 0 is the L4S
        // FIFO, child 1 is the classic DsRedQueueDisc. Accessors provide
        // typed handles.
        Ptr<QueueDisc> classicQ = disc->GetClassicAqmDisc();
        Ptr<QueueDisc> l4sQ = disc->GetL4sQueueDisc();
        NS_TEST_ASSERT_MSG_EQ(classicQ->GetNPackets(),
                              1U,
                              "Classic sub-queue should hold 1 packet after NotECT enqueue");
        NS_TEST_ASSERT_MSG_EQ(l4sQ->GetNPackets(),
                              0U,
                              "L4S sub-queue should be empty after NotECT enqueue");

        // Packet 2: ECT(1) with DSCP 0 — L4S path, goes to queue 1.
        Ptr<Ipv4QueueDiscItem> l4sPkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_ECT1);
        NS_TEST_ASSERT_MSG_EQ(disc->Enqueue(l4sPkt), true, "L4S packet must enqueue");

        NS_TEST_ASSERT_MSG_EQ(classicQ->GetNPackets(),
                              1U,
                              "Classic sub-queue should still hold 1 packet after ECT(1) enqueue");
        NS_TEST_ASSERT_MSG_EQ(l4sQ->GetNPackets(),
                              1U,
                              "L4S sub-queue should hold 1 packet after ECT(1) enqueue");

        // Packet 3: CE — also L4S path.
        Ptr<Ipv4QueueDiscItem> cePkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_CE);
        NS_TEST_ASSERT_MSG_EQ(disc->Enqueue(cePkt), true, "CE-marked packet must enqueue");

        NS_TEST_ASSERT_MSG_EQ(l4sQ->GetNPackets(),
                              2U,
                              "L4S sub-queue should hold 2 packets after CE enqueue");

        // Packet 4: ECT(0) — classic path (RFC 9331 reserves ECT(1) for L4S).
        Ptr<Ipv4QueueDiscItem> ect0Pkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_ECT0);
        NS_TEST_ASSERT_MSG_EQ(disc->Enqueue(ect0Pkt),
                              true,
                              "ECT(0) packet must enqueue on classic path");

        NS_TEST_ASSERT_MSG_EQ(classicQ->GetNPackets(),
                              2U,
                              "Classic sub-queue should hold 2 packets after ECT(0) enqueue");
        NS_TEST_ASSERT_MSG_EQ(l4sQ->GetNPackets(),
                              2U,
                              "L4S sub-queue should still hold 2 packets after ECT(0) enqueue");
    }
};

/// S-L4S.2: setter/getter round-trip for L4sQueueIdx.
class DsL4sConfigTest : public TestCase
{
  public:
    DsL4sConfigTest()
        : TestCase("L4sQueueIdx setter and getter round-trip")
    {
    }

    void DoRun() override
    {
        auto disc = CreateObject<DsL4sQueueDisc>();
        NS_TEST_ASSERT_MSG_EQ(disc->GetL4sQueueIdx(), 1U, "Default L4sQueueIdx should be 1");
        disc->SetL4sQueueIdx(3);
        NS_TEST_ASSERT_MSG_EQ(disc->GetL4sQueueIdx(), 3U, "Setter should update L4sQueueIdx");

        // Phase B.1 attribute defaults — RFC 9332 alignment.
        NS_TEST_ASSERT_MSG_EQ(disc->GetL4sTargetSojournMs(),
                              1.0,
                              "Default L4S target sojourn should be 1 ms (RFC 9332)");
        NS_TEST_ASSERT_MSG_EQ(disc->GetCouplingFactor(),
                              2.0,
                              "Default coupling factor k should be 2 (RFC 9332)");
        NS_TEST_ASSERT_MSG_EQ(static_cast<int>(disc->GetClassicAqm()),
                              static_cast<int>(DsL4sQueueDisc::ClassicAqm::Wred),
                              "Default classic AQM should be Wred");
    }
};

/// S-L4S.3: empty L4S queue produces zero coupled drop on classic path.
/// With p' driven only by L4S sojourn time, an empty L4S queue ⇒ p' = 0
/// ⇒ p_C = 0 ⇒ no classic packets dropped by coupling.
class DsL4sZeroLoadCouplingTest : public TestCase
{
  public:
    DsL4sZeroLoadCouplingTest()
        : TestCase("Empty L4S queue yields zero classic coupled drops")
    {
    }

    void DoRun() override
    {
        auto disc = MakeL4sDisc();
        disc->AssignStreams(1);

        constexpr uint32_t kN = 1000;
        for (uint32_t i = 0; i < kN; ++i)
        {
            auto pkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_NotECT);
            bool ok = disc->Enqueue(pkt);
            NS_TEST_ASSERT_MSG_EQ(ok, true, "Classic packet should enqueue with empty L4S queue");
            NS_TEST_ASSERT_MSG_EQ(disc->GetLastClassicCoupledProb(),
                                  0.0,
                                  "Coupled drop probability must be zero with empty L4S queue");
        }

        NS_TEST_ASSERT_MSG_EQ(disc->GetBaseProb(),
                              0.0,
                              "Base probability must remain zero throughout when "
                              "L4S queue stays empty");

        // No coupled drops should have been recorded by the parent's
        // drop counter for "L4S_COUPLED_DROP" reason.
        const auto& nDrops = disc->GetStats().nDroppedPacketsBeforeEnqueue;
        auto it = nDrops.find("L4S_COUPLED_DROP");
        NS_TEST_ASSERT_MSG_EQ(it == nDrops.end(),
                              true,
                              "No coupled drop reason should appear in stats with empty L4S queue");
    }
};

/// S-L4S.4: squared-ratio coupling invariant. With p' pinned to a
/// fixed value, the observed classic drop ratio approaches (k * p')^2
/// and the observed L4S mark ratio approaches 2 * p'. Their ratio is
/// (k^2 * p') / 2 = 2 * p' for k = 2 (RFC 9332 default).
///
/// To keep the L4S immediate-mark step from saturating p_L to 1.0, we
/// enlarge the L4S target sojourn so the queue length used in the test
/// stays well below threshold. Only the linear p_L = 2 * p' branch is
/// exercised.
class DsL4sSquaredRatioTest : public TestCase
{
  public:
    DsL4sSquaredRatioTest()
        : TestCase("Coupling formula: classic drops scale as squared L4S mark rate")
    {
    }

    void DoRun() override
    {
        auto disc = MakeL4sDisc();
        disc->AssignStreams(7);
        // Push the immediate-mark threshold far above any qlen the
        // test will reach so the linear p_L = 2 * p' branch is tested
        // in isolation.
        disc->SetL4sTargetSojournMs(1e6);
        disc->SetCouplingFactor(2.0);

        constexpr double kPrime = 0.2;
        disc->ForceBaseProbForTest(kPrime);

        // Sanity: with k=2 and p'=0.2, expected:
        //   p_L = 2 * 0.2          = 0.40
        //   p_C = (2 * 0.2)^2      = 0.16
        //   ratio p_C / p_L        = 0.40   (= 2 * p' for k=2)
        const double kExpectedPL = 2.0 * kPrime;
        const double kExpectedPC = std::pow(2.0 * kPrime, 2.0);

        constexpr uint32_t kN = 4000;

        // Enqueue classic packets and count coupled drops.
        uint32_t classicDrops = 0;
        for (uint32_t i = 0; i < kN; ++i)
        {
            auto pkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_NotECT);
            bool ok = disc->Enqueue(pkt);
            if (!ok)
            {
                ++classicDrops;
            }
        }
        double observedPC = static_cast<double>(classicDrops) / kN;

        // Enqueue L4S packets and count CE marks. ECT(1) packets are
        // markable; the parent's m_stats.nTotalMarkedPackets counts
        // every successful Mark() call.
        uint64_t marksBefore = disc->GetStats().nTotalMarkedPackets;
        for (uint32_t i = 0; i < kN; ++i)
        {
            auto pkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_ECT1);
            bool ok = disc->Enqueue(pkt);
            NS_TEST_ASSERT_MSG_EQ(ok, true, "L4S packet enqueue should not drop in this test");
        }
        uint64_t marksAfter = disc->GetStats().nTotalMarkedPackets;
        double observedPL = static_cast<double>(marksAfter - marksBefore) / kN;

        // 4000 samples: 95 % CI half-width for p in [0.16, 0.40] is
        // < 0.025. Use 0.03 tolerance for headroom across RNG streams.
        constexpr double kTol = 0.03;
        NS_TEST_ASSERT_MSG_EQ_TOL(
            observedPC,
            kExpectedPC,
            kTol,
            "Observed p_C must approach (k * p')^2 within statistical tolerance");
        NS_TEST_ASSERT_MSG_EQ_TOL(observedPL,
                                  kExpectedPL,
                                  kTol,
                                  "Observed p_L must approach 2 * p' within statistical tolerance");

        // Snapshot accessors expose the last computed values.
        NS_TEST_ASSERT_MSG_EQ_TOL(disc->GetLastClassicCoupledProb(),
                                  kExpectedPC,
                                  1e-9,
                                  "GetLastClassicCoupledProb must equal (k * p')^2");
        NS_TEST_ASSERT_MSG_EQ_TOL(disc->GetLastL4sMarkProb(),
                                  kExpectedPL,
                                  1e-9,
                                  "GetLastL4sMarkProb must equal 2 * p' (linear branch)");
    }
};

/// S-L4S.5: CE-mark idempotence. RFC 9331 §5: an already-CE packet
/// must not be re-marked or have its mark cleared. Our pipeline calls
/// QueueDisc::Mark, which delegates to Ipv4QueueDiscItem::Mark — that
/// method returns false when the packet is already CE, leaving the
/// header untouched. Verify the packet still has CE after enqueue and
/// the marked-packet counter does NOT increment for already-CE inputs.
class DsL4sCeIdempotenceTest : public TestCase
{
  public:
    DsL4sCeIdempotenceTest()
        : TestCase("CE-marked packet stays CE without double-marking")
    {
    }

    void DoRun() override
    {
        auto disc = MakeL4sDisc();
        disc->AssignStreams(11);
        disc->SetL4sTargetSojournMs(1e6);
        // Force p_L = 1.0 effectively by pinning p' = 0.5 (linear
        // branch saturates: 2 * 0.5 = 1.0). Every packet draws "mark".
        disc->ForceBaseProbForTest(0.5);

        uint64_t marksBefore = disc->GetStats().nTotalMarkedPackets;

        constexpr uint32_t kN = 100;
        for (uint32_t i = 0; i < kN; ++i)
        {
            // Pre-marked CE packets: idempotence applies.
            auto pkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_CE);
            bool ok = disc->Enqueue(pkt);
            NS_TEST_ASSERT_MSG_EQ(ok, true, "CE-marked packet must still enqueue");
        }
        uint64_t marksAfter = disc->GetStats().nTotalMarkedPackets;

        NS_TEST_ASSERT_MSG_EQ(marksAfter - marksBefore,
                              0U,
                              "Mark counter must not increment for already-CE "
                              "packets (RFC 9331 §5 idempotence)");

        // Confirm the L4S queue (composer child 0) still received all
        // the packets. Accessor-based lookup is refactor-robust.
        Ptr<QueueDisc> l4sQ = disc->GetL4sQueueDisc();
        NS_TEST_ASSERT_MSG_EQ(l4sQ->GetNPackets(),
                              kN,
                              "All CE packets should be enqueued on the L4S sub-queue");

        // Now compare with ECT(1): the same enqueue rate should produce
        // marks, since the linear coupling saturates.
        uint64_t marksB = disc->GetStats().nTotalMarkedPackets;
        constexpr uint32_t kM = 100;
        for (uint32_t i = 0; i < kM; ++i)
        {
            auto pkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_ECT1);
            bool ok = disc->Enqueue(pkt);
            NS_TEST_ASSERT_MSG_EQ(ok, true, "ECT(1) packet must enqueue");
        }
        uint64_t marksAft = disc->GetStats().nTotalMarkedPackets;
        NS_TEST_ASSERT_MSG_EQ(marksAft - marksB,
                              kM,
                              "Mark counter must increment by kM for ECT(1) inputs at p_L = 1");
    }
};

/// Helper for scheduling enqueues at specific simulation times.
void
EnqueueEct1(Ptr<DsL4sQueueDisc> disc)
{
    auto pkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_ECT1);
    disc->Enqueue(pkt);
}

/// S-L4S.6: immediate-mark threshold. When the L4S sub-queue head
/// packet has been queued longer than the target, every newly arriving
/// L4S packet is CE-marked (p_L = 1). Phase B.2 measures sojourn from
/// the per-packet enqueue tag, so simulation time must advance for the
/// head packet to age past the threshold.
class DsL4sImmediateMarkThresholdTest : public TestCase
{
  public:
    DsL4sImmediateMarkThresholdTest()
        : TestCase("L4S immediate-mark step above target sojourn")
    {
    }

    void DoRun() override
    {
        Simulator::Destroy(); // start clean
        auto disc = MakeL4sDisc();
        disc->AssignStreams(13);

        // Pin p' = 0 so the linear branch contributes nothing; only the
        // immediate-mark step can produce marks.
        disc->ForceBaseProbForTest(0.0);
        disc->SetL4sTargetSojournMs(1.0);

        // Warmup at t = 0 so the head packet's timestamp is t = 0.
        auto warmup = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_ECT1);
        bool okWarm = disc->Enqueue(warmup);
        NS_TEST_ASSERT_MSG_EQ(okWarm, true, "Warm-up packet must enqueue");

        // Schedule kN further enqueues starting at t = 10 ms (well above
        // the 1 ms target sojourn). Each enqueue spaced by 1 us so the
        // head sojourn keeps growing across iterations.
        constexpr uint32_t kN = 50;
        for (uint32_t i = 0; i < kN; ++i)
        {
            Simulator::Schedule(MilliSeconds(10) + MicroSeconds(i), &EnqueueEct1, disc);
        }
        Simulator::Stop(MilliSeconds(20));

        uint64_t marksBefore = disc->GetStats().nTotalMarkedPackets;
        Simulator::Run();
        uint64_t marksAfter = disc->GetStats().nTotalMarkedPackets;
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(marksAfter - marksBefore,
                              kN,
                              "Every packet enqueued above target sojourn must be CE-marked");
    }
};

/// S-L4S.7 (B.2 conformance): controller step response. Per RFC 9332
/// §A.2, the P.I.² controller integrates the *classic* sub-queue's
/// sojourn against the classic target. With the classic sub-queue
/// holding a packet whose enqueue timestamp ages past that target,
/// the periodic P+I controller drives p' upward across ticks.
/// Verifies (a) p' starts at 0, (b) p' is positive after several ticks
/// of sustained over-target sojourn, (c) p' grows monotonically over
/// a short sequence of samples (the integral term dominates the
/// derivative on a steadily-increasing input).
class DsL4sControllerStepResponseTest : public TestCase
{
  public:
    DsL4sControllerStepResponseTest()
        : TestCase("Controller P+I drives p' upward under sustained over-target "
                   "sojourn")
    {
    }

    void DoRun() override
    {
        Simulator::Destroy();
        auto disc = MakeL4sDisc();
        disc->AssignStreams(17);
        disc->SetControllerInterval(MilliSeconds(16));

        // Pre-load 1 NotECT packet at t=0 into the classic sub-queue
        // (RFC 9332 §A.2: the controller reads classic-queue sojourn,
        // not L4S sojourn — ECT(1) would route to the L4S sub-queue
        // and leave the classic AQM empty, suppressing the controller).
        // Without dequeues, the head sojourn equals the elapsed sim time
        // via the DsL4sTimestampTag path in ComputeClassicSojournMs.
        auto warmup = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_NotECT);
        disc->Enqueue(warmup);

        NS_TEST_ASSERT_MSG_EQ(disc->GetBaseProb(),
                              0.0,
                              "Initial p' must be zero before any controller tick");

        // Sample p' at three points to check monotone growth.
        double p1 = 0.0;
        double p2 = 0.0;
        double p3 = 0.0;
        Simulator::Schedule(MilliSeconds(20), [&]() { p1 = disc->GetBaseProb(); });
        Simulator::Schedule(MilliSeconds(80), [&]() { p2 = disc->GetBaseProb(); });
        Simulator::Schedule(MilliSeconds(160), [&]() { p3 = disc->GetBaseProb(); });
        Simulator::Stop(MilliSeconds(200));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_GT(p1, 0.0, "p' must be positive after the first tick over the target");
        NS_TEST_ASSERT_MSG_GT(p2,
                              p1,
                              "p' must keep growing while sojourn keeps growing past target");
        NS_TEST_ASSERT_MSG_GT(p3,
                              p2,
                              "p' must keep growing across additional ticks under "
                              "sustained overload");
        NS_TEST_ASSERT_MSG_LT(p3, 1.0, "p' must remain in [0, 1] (clamp invariant)");
    }
};

/// S-L4S.8 (B.2 conformance): controller stays at zero with no L4S
/// traffic. Periodic ticks fire 60 times across 1 second; with the
/// L4S sub-queue empty throughout, the proportional term equals
/// alpha * (-target) which is negative — clamped to zero. p' must
/// never drift positive.
class DsL4sControllerNoDriftTest : public TestCase
{
  public:
    DsL4sControllerNoDriftTest()
        : TestCase("Controller p' stays at zero with empty L4S queue across many "
                   "ticks")
    {
    }

    void DoRun() override
    {
        Simulator::Destroy();
        auto disc = MakeL4sDisc();
        disc->AssignStreams(19);

        // No L4S packets ever enqueued.
        Simulator::Stop(Seconds(1.0));
        Simulator::Run();
        double endProb = disc->GetBaseProb();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(endProb,
                              0.0,
                              "p' must remain pinned at 0 when L4S queue stays "
                              "empty (clamped negative error)");
    }
};

/// S-L4S.9 (B.2 conformance): CoupledOnly mode bypasses parent WRED.
/// With p' = 0 (no coupled drops) and qlen well above what would
/// trigger Wred early drops, all classic packets must enqueue. In
/// Wred mode (the default in MakeL4sDisc), the same sequence would
/// see the parent's RIO/WRED early-drop fire once vAve grows.
class DsL4sCoupledOnlyBypassWredTest : public TestCase
{
  public:
    DsL4sCoupledOnlyBypassWredTest()
        : TestCase("CoupledOnly mode bypasses parent WRED early drops")
    {
    }

    void DoRun() override
    {
        // Build a disc with CoupledOnly. Note: do NOT call ConfigQueue
        // afterwards on classic queues — the CoupledOnly InitializeParams
        // auto-config (DROP_TAIL with pass-through thresholds) is the
        // contract, and post-Initialize ConfigQueue would overwrite it.
        auto disc = CreateObject<DsL4sQueueDisc>();
        disc->SetNumQueues(2);
        disc->SetL4sQueueIdx(1);
        disc->SetClassicAqm(DsL4sQueueDisc::ClassicAqm::CoupledOnly);
        disc->SetQueueLimit(0, 200);
        disc->SetQueueLimit(1, 200);
        disc->AddPhbEntry(0, 0, 0);
        Ptr<DsPriorityScheduler> sched =
            CreateObjectWithAttributes<DsPriorityScheduler>("NumQueues", UintegerValue(2));
        disc->SetScheduler(sched);
        disc->Initialize();

        // L4S queue still needs explicit thresholds (it's not classic
        // and InitializeParams doesn't touch it).
        disc->ConfigQueue(1, 0, 100.0, 200.0, 0.1);

        disc->AssignStreams(23);
        disc->ForceBaseProbForTest(0.0); // no coupled drops

        constexpr uint32_t kN = 100;
        uint32_t enqueued = 0;
        for (uint32_t i = 0; i < kN; ++i)
        {
            auto pkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_NotECT);
            if (disc->Enqueue(pkt))
            {
                ++enqueued;
            }
        }
        NS_TEST_ASSERT_MSG_EQ(enqueued,
                              kN,
                              "CoupledOnly + p'=0 must accept every classic packet "
                              "up to physical buffer limit");

        // The classic AQM is composer child 1, retrieved via the
        // typed accessor.
        Ptr<QueueDisc> classicQ = disc->GetClassicAqmDisc();
        NS_TEST_ASSERT_MSG_EQ(classicQ->GetNPackets(),
                              kN,
                              "Classic AQM should hold all kN packets in CoupledOnly mode");
    }
};

/// S-L4S.10 (B.3 conformance): coupled scheduler honours L4S priority
/// up to the burst cap, then forces a classic dequeue. Verifies that
/// (a) all enqueued packets are eventually dequeued, (b) no run of
/// L4S dequeues exceeds burstCap consecutive entries while a classic
/// queue has packets, (c) the diagnostic forced-classic counter
/// matches the number of cap-driven preemptions, and (d) when the
/// classic queue empties, the L4S queue can drain freely.
class DsL4sCoupledSchedulerStarvationTest : public TestCase
{
  public:
    DsL4sCoupledSchedulerStarvationTest()
        : TestCase("Coupled scheduler enforces classic dequeues at burst-cap "
                   "boundaries")
    {
    }

    void DoRun() override
    {
        constexpr uint32_t kNumQueues = 2;
        constexpr uint32_t kL4sIdx = 1;
        constexpr uint32_t kBurst = 3;

        Ptr<DsL4sCoupledScheduler> sched =
            CreateObjectWithAttributes<DsL4sCoupledScheduler>("NumQueues",
                                                              UintegerValue(kNumQueues),
                                                              "L4sQueueIdx",
                                                              UintegerValue(kL4sIdx),
                                                              "BurstCap",
                                                              UintegerValue(kBurst));

        // Enqueue 12 L4S, 4 classic. With burst cap 3, expected
        // dequeue pattern: every 3rd L4S triggers a classic until
        // classics run out, then remaining L4S drain freely.
        constexpr uint32_t kL4sIn = 12;
        constexpr uint32_t kClassicIn = 4;
        for (uint32_t i = 0; i < kL4sIn; ++i)
        {
            sched->OnEnqueue(kL4sIdx, 1500);
        }
        for (uint32_t i = 0; i < kClassicIn; ++i)
        {
            sched->OnEnqueue(0, 1500);
        }

        std::vector<int> sequence;
        sequence.reserve(kL4sIn + kClassicIn);
        while (true)
        {
            int q = sched->SelectNextQueue();
            if (q < 0)
            {
                break;
            }
            sequence.push_back(q);
        }

        // (a) All packets dequeued exactly once.
        NS_TEST_ASSERT_MSG_EQ(sequence.size(),
                              kL4sIn + kClassicIn,
                              "All enqueued packets must be dequeued");
        uint32_t l4sOut = 0;
        uint32_t classicOut = 0;
        for (int q : sequence)
        {
            if (static_cast<uint32_t>(q) == kL4sIdx)
            {
                ++l4sOut;
            }
            else
            {
                ++classicOut;
            }
        }
        NS_TEST_ASSERT_MSG_EQ(l4sOut, kL4sIn, "L4S dequeue count must match enqueue count");
        NS_TEST_ASSERT_MSG_EQ(classicOut,
                              kClassicIn,
                              "Classic dequeue count must match enqueue count");

        // (b) No L4S run exceeds burstCap *while* a classic queue is
        // still backlogged. Once classics are exhausted, the L4S
        // remainder drains freely (run length unbounded).
        uint32_t classicRemaining = kClassicIn;
        uint32_t currentL4sRun = 0;
        for (int q : sequence)
        {
            if (static_cast<uint32_t>(q) == kL4sIdx)
            {
                ++currentL4sRun;
                NS_TEST_ASSERT_MSG_LT_OR_EQ(
                    currentL4sRun,
                    kBurst,
                    "L4S run cannot exceed burst cap while classic has packets");
            }
            else
            {
                NS_TEST_ASSERT_MSG_GT(classicRemaining,
                                      0U,
                                      "Cannot dequeue classic with none enqueued");
                --classicRemaining;
                currentL4sRun = 0;
                if (classicRemaining == 0)
                {
                    break;
                }
            }
        }

        // (c) Forced-classic counter matches the number of preemptions
        // observed: each classic dequeue while L4S was backlogged was
        // forced (with kClassicIn=4 and L4S always available, all 4
        // classic dequeues are forced).
        NS_TEST_ASSERT_MSG_EQ(sched->GetForcedClassicCount(),
                              kClassicIn,
                              "Forced-classic counter must equal classic dequeues "
                              "(L4S was backlogged for all of them)");
    }
};

/// S-L4S.11 (B.3): coupled scheduler does not deadlock when only L4S
/// has traffic. The burst cap must not prevent L4S from draining
/// completely if classic is empty.
class DsL4sCoupledSchedulerL4sOnlyTest : public TestCase
{
  public:
    DsL4sCoupledSchedulerL4sOnlyTest()
        : TestCase("Coupled scheduler drains L4S freely when classic is empty")
    {
    }

    void DoRun() override
    {
        Ptr<DsL4sCoupledScheduler> sched =
            CreateObjectWithAttributes<DsL4sCoupledScheduler>("NumQueues",
                                                              UintegerValue(2),
                                                              "L4sQueueIdx",
                                                              UintegerValue(1),
                                                              "BurstCap",
                                                              UintegerValue(2)); // burst cap = 2

        constexpr uint32_t kN = 20;
        for (uint32_t i = 0; i < kN; ++i)
        {
            sched->OnEnqueue(1, 1500);
        }

        uint32_t served = 0;
        while (true)
        {
            int q = sched->SelectNextQueue();
            if (q < 0)
            {
                break;
            }
            NS_TEST_ASSERT_MSG_EQ(q, 1, "Only L4S queue should be served");
            ++served;
        }
        NS_TEST_ASSERT_MSG_EQ(served, kN, "All L4S packets must drain even past the burst cap");
        NS_TEST_ASSERT_MSG_EQ(sched->GetForcedClassicCount(),
                              0U,
                              "Forced-classic counter stays at 0 when classic queue is empty");
    }
};

/// S-L4S.12: FqCoDelQueueDisc as inner classic AQM.
///
/// Exercises composition when `ClassicAqm::FqCoDel` is selected. The
/// disc must: (a) substitute an `FqCoDelQueueDisc` for the default
/// `DsRedQueueDisc`, (b) route ECT(1)/CE to the L4S FIFO and NotECT
/// to the FqCoDel inner, (c) aggregate drops via ns-3's standard
/// `ChildQueueDiscDropFunctor` so they appear in the composer's stats,
/// and (d) fire coupled p_C drops inner-agnostically when p' > 0.
///
/// The test does NOT call any Red-specific forwarder (AddPhbEntry,
/// ConfigQueue, SetNumQueues, SetMredMode, SetNumPrec,
/// SetMeanPacketSize, SetQueueBandwidth). Those are documented as
/// Red-only and would assert on the foreign inner.
class DsL4sFqCoDelInnerAqmTest : public TestCase
{
  public:
    DsL4sFqCoDelInnerAqmTest()
        : TestCase("FqCoDelQueueDisc as inner classic AQM: routing, coupled "
                   "drop, drop aggregation")
    {
    }

    void DoRun() override
    {
        auto disc = CreateObject<DsL4sQueueDisc>();
        disc->SetL4sQueueIdx(1);

        // Pre-build the FqCoDel inner ourselves so we can seed the
        // quantum. In an on-device deployment FqCoDel auto-sets the
        // quantum from the NetDevice MTU; in this headless test there
        // is no device, so we call SetQuantum() directly.
        Ptr<FqCoDelQueueDisc> fq = CreateObject<FqCoDelQueueDisc>();
        fq->SetQuantum(1500);
        disc->SetClassicAqmDisc(fq);

        // SetQueueLimit(1, ...) is safe against any inner (it only
        // touches the L4S FIFO child). SetQueueLimit on the classic
        // slot is Red-only and must NOT be called here.
        disc->SetQueueLimit(1, 10000);

        Ptr<DsPriorityScheduler> sched =
            CreateObjectWithAttributes<DsPriorityScheduler>("NumQueues", UintegerValue(2));
        disc->SetScheduler(sched);

        disc->Initialize();

        // Inner classic AQM must be an FqCoDelQueueDisc, not RED.
        Ptr<QueueDisc> classicQ = disc->GetClassicAqmDisc();
        NS_TEST_ASSERT_MSG_NE(DynamicCast<FqCoDelQueueDisc>(classicQ),
                              nullptr,
                              "Inner classic AQM must be an FqCoDelQueueDisc");
        NS_TEST_ASSERT_MSG_EQ(DynamicCast<DsRedQueueDisc>(classicQ),
                              nullptr,
                              "Inner classic AQM must not be a DsRedQueueDisc");

        Ptr<QueueDisc> l4sQ = disc->GetL4sQueueDisc();
        NS_TEST_ASSERT_MSG_NE(l4sQ, nullptr, "L4S lane must be initialised");
        if (!l4sQ)
        {
            Simulator::Destroy();
            return;
        }

        // (b) Routing by ECN codepoint: ECT(1) / CE to L4S, NotECT / ECT(0)
        // to FqCoDel. Differentiate the flows by source address so
        // FqCoDel's flow-hashing doesn't collapse them into the same
        // inner bucket, which would still be correct but harder to
        // observe with GetNPackets() on the composite inner.
        disc->AssignStreams(29);
        disc->ForceBaseProbForTest(0.0); // no coupled drops in this first phase

        constexpr uint32_t kNClassic = 20;
        constexpr uint32_t kNL4s = 20;
        for (uint32_t i = 0; i < kNClassic; ++i)
        {
            auto pkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_NotECT);
            bool ok = disc->Enqueue(pkt);
            NS_TEST_ASSERT_MSG_EQ(ok,
                                  true,
                                  "Classic (NotECT) packet must enqueue on FqCoDel inner");
        }
        for (uint32_t i = 0; i < kNL4s; ++i)
        {
            auto pkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_ECT1);
            bool ok = disc->Enqueue(pkt);
            NS_TEST_ASSERT_MSG_EQ(ok, true, "L4S (ECT(1)) packet must enqueue on FIFO lane");
        }

        // FqCoDel aggregates packets across its flow buckets into
        // GetNPackets(); the total must match kNClassic.
        NS_TEST_ASSERT_MSG_EQ(classicQ->GetNPackets(),
                              kNClassic,
                              "All NotECT packets must land on the FqCoDel inner");
        NS_TEST_ASSERT_MSG_EQ(l4sQ->GetNPackets(),
                              kNL4s,
                              "All ECT(1) packets must land on the L4S lane");

        // (d) Coupled-drop firing is inner-agnostic. Pin p' so every
        // classic enqueue draws a coupled drop (p_C = (2*0.5)^2 = 1).
        disc->ForceBaseProbForTest(0.5);

        uint32_t classicDrops = 0;
        constexpr uint32_t kCouplingProbe = 50;
        for (uint32_t i = 0; i < kCouplingProbe; ++i)
        {
            auto pkt = MakeItem(Ipv4Header::DscpDefault, Ipv4Header::ECN_NotECT);
            if (!disc->Enqueue(pkt))
            {
                ++classicDrops;
            }
        }
        NS_TEST_ASSERT_MSG_EQ(classicDrops,
                              kCouplingProbe,
                              "Every classic packet must be coupled-dropped at p' = 0.5 (p_C = 1)");

        // (c) Drop accounting: coupled drops appear under
        // "L4S_COUPLED_DROP" in the composer's per-reason map.
        const auto& dropMap = disc->GetStats().nDroppedPacketsBeforeEnqueue;
        auto it = dropMap.find("L4S_COUPLED_DROP");
        NS_TEST_ASSERT_MSG_EQ(it != dropMap.end(),
                              true,
                              "L4S_COUPLED_DROP reason must appear in stats map");
        NS_TEST_ASSERT_MSG_EQ(it->second,
                              kCouplingProbe,
                              "L4S_COUPLED_DROP counter must equal coupling-probe count");

        // Inner FqCoDel stats survive composition: GetNPackets is a
        // live read of its flow buckets. L4S FIFO still holds all
        // kNL4s packets (no coupled drop on the L4S side).
        NS_TEST_ASSERT_MSG_EQ(
            classicQ->GetNPackets(),
            kNClassic,
            "FqCoDel inner must retain kNClassic packets after coupled-drop probe "
            "(coupled drops are composer-level, never reach the inner)");
        NS_TEST_ASSERT_MSG_EQ(l4sQ->GetNPackets(), kNL4s, "L4S lane must retain kNL4s packets");
    }
};

class DsL4sQueueDiscSuite : public TestSuite
{
  public:
    DsL4sQueueDiscSuite()
        : TestSuite("diffserv-l4s", Type::UNIT)
    {
        AddTestCase(new DsL4sRoutingTest, Duration::QUICK);
        AddTestCase(new DsL4sConfigTest, Duration::QUICK);
        AddTestCase(new DsL4sZeroLoadCouplingTest, Duration::QUICK);
        AddTestCase(new DsL4sSquaredRatioTest, Duration::QUICK);
        AddTestCase(new DsL4sCeIdempotenceTest, Duration::QUICK);
        AddTestCase(new DsL4sImmediateMarkThresholdTest, Duration::QUICK);
        AddTestCase(new DsL4sControllerStepResponseTest, Duration::QUICK);
        AddTestCase(new DsL4sControllerNoDriftTest, Duration::QUICK);
        AddTestCase(new DsL4sCoupledOnlyBypassWredTest, Duration::QUICK);
        AddTestCase(new DsL4sCoupledSchedulerStarvationTest, Duration::QUICK);
        AddTestCase(new DsL4sCoupledSchedulerL4sOnlyTest, Duration::QUICK);
        AddTestCase(new DsL4sFqCoDelInnerAqmTest, Duration::QUICK);
        AddTestCase(new DsL4sScenarioPiControlFiresTest, Duration::EXTENSIVE);
        AddTestCase(new DsL4sScenarioS1LatencyDifferentiationTest, Duration::EXTENSIVE);
        AddTestCase(new DsL4sScenarioS2CoexistenceThroughputEquivalenceTest, Duration::EXTENSIVE);
        AddTestCase(new DsL4sScenarioS1AdvantageLatencyDeltaTest, Duration::EXTENSIVE);
        AddTestCase(new DsL4sScenarioFqCoDelComparisonSmokePerModeTest(), Duration::EXTENSIVE);
        AddTestCase(new DsL4sScenarioFqCoDelClassicCompositionalSafetyTest(),
                    Duration::EXTENSIVE);
        AddTestCase(new DsL4sScenarioDualPi2GprtParityTest(), Duration::EXTENSIVE);
        AddTestCase(new DsL4sScenarioCakeCompositionFairnessTest(), Duration::EXTENSIVE);
        AddTestCase(new DsL4sScenarioCakeCompositionThroughputParityTest(), Duration::EXTENSIVE);
    }
};

DsL4sQueueDiscSuite g_dsL4sSuite;

} // namespace
