/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010-2011 ComSys, RWTH Aachen University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Rene Glebke, Martin Lang
 */

#ifndef REQUESTSCHEDULINGSTRATEGYLIVE_H_
#define REQUESTSCHEDULINGSTRATEGYLIVE_H_

#include "ns3/AbstractStrategy.h"
#include "ns3/BitTorrentPeer.h"
#include "ns3/RequestSchedulingStrategyBase.h"

namespace ns3
{
namespace bittorrent
{

class Peer;

/**
 * \ingroup BitTorrent
 *
 * \brief Provides base functionality for handling incoming requests from other BitTorrent clients.
 *
 * This class provides a basic implementation for the handling of upload requests (mainly REQUEST messages [BitTorrent message id 6]) received by a
 * client. You may override or add handler functions for events generated by the BitTorrentClient class and derivated classes to implement other or
 * additional behavior, such as upload traffic shaping.
 *
 * The base implementation checks whether the requesting client is currently unchoked. If so, it directly initiates a transfer of the piece
 * to the requester; else, it silently drops the request.
 */
class RequestSchedulingStrategyLive : public RequestSchedulingStrategyBase
{
    // Constructors etc.
  public:
    RequestSchedulingStrategyLive(Ptr<BitTorrentClient> myClient);
    ~RequestSchedulingStrategyLive() override;
    /**
     * Initialize the strategy. Register the needed event listeners with the associated client.
     */
    void DoInitialize() override;

    // Event handlers
  public:
    virtual void ProcessStreamBufferReadyEvent(std::string streamHash);
};

} // namespace bittorrent
} // namespace ns3

#endif /* REQUESTSCHEDULERBASE_H_ */
