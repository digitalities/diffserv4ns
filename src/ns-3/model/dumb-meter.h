/*
 * Copyright (C) 2001-2026 Sergio Andreozzi
 * Copyright (C) 2000 Nortel Networks
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Ported from DiffServ4NS dsPolicy.cc DumbPolicy (2001).
 */

#ifndef NS3_DIFFSERV_DUMB_METER_H
#define NS3_DIFFSERV_DUMB_METER_H

#include "meter.h"

namespace ns3
{
namespace diffserv
{

/**
 * @ingroup diffserv
 *
 * @brief Pass-through meter that always marks GREEN.
 *
 * The DumbMeter updates arrival time but performs no bucket accounting.
 * Useful as a baseline and as a template for new meter implementations.
 *
 * @see dsPolicy.cc DumbPolicy (line 491)
 */
class DumbMeter : public Meter
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

#endif // NS3_DIFFSERV_DUMB_METER_H
