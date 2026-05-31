/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Shared cwd-independent path resolution for diffserv test data.
 *
 * Tests that load files from the example-3 data set previously
 * hardcoded a cwd-relative path, which crashed (SIGABRT via
 * NS_FATAL) when the test runner was invoked from any directory
 * other than the ns-3 root.  This helper resolves the data
 * directory via the NS3_DIFFSERV_DATA_DIR environment variable
 * if set, otherwise falls back to the historical relative
 * default (preserving the cd ns3/ns-3-dev happy path).
 *
 * The fallback path is the symlink-staged location produced by
 * scripts/fetch-ns3.sh, which creates
 * ns3/ns-3-dev/contrib/diffserv -> src/ns-3.  Editing this
 * literal requires keeping that symlink target in mind.
 */

#ifndef NS3_DIFFSERV_TEST_DATA_PATHS_H
#define NS3_DIFFSERV_TEST_DATA_PATHS_H

#include <cstdlib>
#include <string>

namespace ns3::diffserv::testing
{

inline std::string
GetExample3DataDir()
{
    if (const char* p = std::getenv("NS3_DIFFSERV_DATA_DIR"); p && *p)
    {
        std::string s(p);
        if (s.back() != '/')
        {
            s.push_back('/');
        }
        return s;
    }
    return "contrib/diffserv/examples/example-3-data/";
}

} // namespace ns3::diffserv::testing

#endif // NS3_DIFFSERV_TEST_DATA_PATHS_H
