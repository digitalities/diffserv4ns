/*
 * Copyright (C) 2001-2026 Sergio Andreozzi
 * Copyright (C) 2000 Nortel Networks
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Ported from DiffServ4NS dsPolicy.cc TRTCMPolicy (2001).
 */

#ifndef NS3_DIFFSERV_TR_TCM_METER_H
#define NS3_DIFFSERV_TR_TCM_METER_H

#include "meter.h"

namespace ns3
{
namespace diffserv
{

/**
 * @ingroup diffserv
 *
 * @brief Two Rate Three Colour Marker (RFC 2698).
 *
 * Two independent rates: CIR fills cBucket, PIR fills pBucket.
 * Policer checks pBucket first: if pBucket < size -> RED.
 * On YELLOW, only pBucket is decremented (cBucket untouched).
 * On GREEN, BOTH buckets are decremented.
 * On RED, NEITHER bucket is decremented.
 *
 * Reference: dsPolicy.cc TRTCMPolicy::applyMeter (line 729) and
 * TRTCMPolicy::applyPolicer (line 761).
 *
 */
class TrTcmMeter : public Meter
{
  public:
    /**
     * @brief Get the TypeId for this class.
     * @return the TypeId.
     */
    static TypeId GetTypeId();

    void ApplyMeter(PolicyEntry& entry, double nowSeconds, uint32_t packetSize) override;

  protected:
    Colour DoApplyPolicer(PolicyEntry& entry, uint32_t packetSize) override;
};

} // namespace diffserv
} // namespace ns3

#endif // NS3_DIFFSERV_TR_TCM_METER_H
