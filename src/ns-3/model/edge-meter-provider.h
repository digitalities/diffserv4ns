/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NS3_DIFFSERV_EDGE_METER_PROVIDER_H
#define NS3_DIFFSERV_EDGE_METER_PROVIDER_H

#include "diffserv-constants.h"

#include "ns3/ptr.h"

namespace ns3
{
namespace diffserv
{

class Meter;

/**
 * @ingroup diffserv
 *
 * @brief Lookup hook a classifier consults to obtain the Meter
 * algorithm instance for a given MeterType.
 *
 * The hook decouples classifier dispatch from meter ownership: the
 * edge disc holds the meter strategy slots, and any classifier
 * installed on the edge (DSCP-keyed or per-flow) consults the same
 * provider so custom meters injected via edge->SetMeter(type, meter)
 * are honoured everywhere.
 *
 */
class EdgeMeterProvider
{
  public:
    virtual ~EdgeMeterProvider() = default;

    /**
     * @brief Return the Meter algorithm for @p type, lazy-creating a
     * default implementation if no custom meter has been installed.
     *
     * @param type meter algorithm selector
     * @return the Meter instance, or nullptr if @p type is unsupported
     */
    virtual Ptr<Meter> GetMeter(MeterType type) = 0;
};

} // namespace diffserv
} // namespace ns3

#endif // NS3_DIFFSERV_EDGE_METER_PROVIDER_H
