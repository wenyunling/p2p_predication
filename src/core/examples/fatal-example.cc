/*
 * Copyright (c) 2020 Lawrence Livermore National Laboratory
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Peter D. Barnes, Jr. <pdbarnes@llnl.gov>
 */

#include "ns3/assert.h"
#include "ns3/fatal-error.h"
#include "ns3/simulator.h"

#include <iostream>

/**
 * @file
 * @defgroup fatal-example Core example: NS_FATAL error handlers
 * @ingroup core-examples
 * @ingroup fatal
 *
 * Example program illustrating use of the NS_FATAL error handlers.
 */

using namespace ns3;

/**
 * @ingroup fatal-example
 * @brief Triggers a fatal error without message, deferring termination.
 */
void
FatalNoMsg()
{
    std::cerr << "\nEvent triggered fatal error without message, and continuing:" << std::endl;
    NS_FATAL_ERROR_NO_MSG_CONT();
}

/**
 * @ingroup fatal-example
 * @brief Triggers a fatal error with an error message, deferring termination.
 */
void
FatalCont()
{
    std::cerr << "\nEvent triggered fatal error, with custom message, and continuing:" << std::endl;
    NS_FATAL_ERROR_CONT("fatal error, but continuing");
}

/**
 * @ingroup fatal-example
 * @brief Triggers a fatal error with message, and terminating.
 */
void
Fatal()
{
    std::cerr << "\nEvent triggered fatal error, with message, and terminating:" << std::endl;
    NS_FATAL_ERROR("fatal error, terminating");
}

int
main(int argc, char** argv)
{
    // First schedule some events
    Simulator::Schedule(Seconds(1), FatalNoMsg);
    Simulator::Schedule(Seconds(2), FatalCont);
    Simulator::Schedule(Seconds(3), Fatal);

    // Show some errors outside of simulation time
    std::cerr << "\nFatal error with custom message, and continuing:" << std::endl;
    NS_FATAL_ERROR_CONT("fatal error, but continuing");

    std::cerr << "\nFatal error without message, and continuing:" << std::endl;
    NS_FATAL_ERROR_NO_MSG_CONT();

    // Now run the simulator
    Simulator::Run();

    // Should not get here
    NS_FATAL_ERROR("fatal error, terminating");
    NS_ASSERT_MSG(false, "Should not get here.");

    return 0;
}
