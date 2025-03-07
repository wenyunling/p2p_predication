/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010-2012 ComSys, RWTH Aachen University
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
 * Authors: Rene Glebke (principal author), Alexander Hocks
 */

#include "BitTorrentTracker.h"

#include "BitTorrentHttpServer.h"

#include "ns3/BitTorrentUtilities.h"
#include "ns3/GlobalMetricsGatherer.h"
#include "ns3/Torrent.h"
#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/callback.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

namespace ns3
{
namespace bittorrent
{

NS_LOG_COMPONENT_DEFINE("BitTorrentTracker");
NS_OBJECT_ENSURE_REGISTERED(BitTorrentTracker);

BitTorrentTracker::BitTorrentTracker()
{
    m_announcePath = "/announce";
    m_scrapePath = "/scrape";
    m_updateInterval = "60";
    m_getSeederStrategy = "random";
    m_handleStartListening = MakeCallback(&BitTorrentHttpServer::StartListening, &HttpCS);
    m_handleStopListening = MakeCallback(&BitTorrentHttpServer::StopListening, &HttpCS);
    m_handleConnectionCreation = MakeCallback(&BitTorrentTracker::ConnectionCreation, this);
    m_handleConnectionCreated = MakeCallback(&BitTorrentHttpServer::ConnectionCreated, &HttpCS);
    m_handleReceiveReq = MakeCallback(&BitTorrentTracker::ReceiveReq, this);
    m_handleReceiveRequest = MakeCallback(&BitTorrentHttpServer::ReceiveRequest, &HttpCS);
    m_handleErrorOccurance = MakeCallback(&BitTorrentTracker::ErrorOccurance, this);
    m_handleDataCreater = MakeCallback(&BitTorrentTracker::DataCreater, this);
    m_handleSendData = MakeCallback(&BitTorrentHttpServer::SendData, &HttpCS);
}

BitTorrentTracker::~BitTorrentTracker()
{
}

TypeId
BitTorrentTracker::GetTypeId()
{
    static TypeId tid = TypeId("ns3::BitTorrentTracker").SetParent<Application>().AddConstructor<BitTorrentTracker>();
    return tid;
}

void
BitTorrentTracker::StartApplication()
{
    // Start the BitTorrentHttpServer component
    // NS_LOG_INFO("TRACKER START APP");
    m_handleStartListening(GetNode(), TcpSocketFactory::GetTypeId(), m_handleConnectionCreation);
}

void
BitTorrentTracker::StopApplication()
{
    // Stop the BitTorrentHttpServer component
    m_handleStopListening();
}

void
BitTorrentTracker::DoDispose()
{
    Application::DoDispose();
}

std::string
BitTorrentTracker::GetAnnouncePath() const
{
    return m_announcePath;
}

void
BitTorrentTracker::SetAnnouncePath(std::string announcePath)
{
    m_announcePath = announcePath;
}

void
BitTorrentTracker::UpdateClientBacksource(BTDict clientInfo)
{
    if (IsStreamInfo(clientInfo))
    {
        std::string streamHash = clientInfo.find("StreamHash")->second;
        std::transform(streamHash.begin(), streamHash.end(), streamHash.begin(), toupper);
        std::string peer_id = (*(clientInfo.find("peer_id"))).second;
        std::map<std::string, BitTorrentTrackerCloudInfo>::iterator cloudInfoIt;
        std::string peer_type = clientInfo.find("PeerType")->second;
        NS_LOG_INFO("We find a streaming client " << peer_id << " type " << peer_type << " with Hash " << streamHash);

        if (peer_type == BT_STREAM_PEERTYPE_CDN)
        {
            // m_CDNBacksource[peer_id].insert(streamHash);
            NS_FATAL_ERROR("PEER TYPE" << peer_type << "DO not need update backsource");
        }
        else if (peer_type == BT_STREAM_PEERTYPE_PCDN)
        {
            m_PCDNBacksource[peer_id].insert(streamHash);
        }
        else if (peer_type == BT_STREAM_PEERTYPE_CLIENT)
        {
            return;
            NS_LOG_INFO("PEER TYPE" << peer_type << "DO not need update backsource");
            // cloudInfoIt = m_cloudInfo.find(streamHash);
            // cloudInfoIt->second.m_clients[peer_id] = clientInfo;
        }
        else
        {
            NS_FATAL_ERROR("UNKNOWN PEER TYPE" << peer_type);
        }
        // NS_LOG_INFO("client " << peer_type << " with id " << peer_id << " now has inside the cloud");
    }
    else
    {
        NS_FATAL_ERROR("should not meet update backsource in vod");
    }
}

std::string
BitTorrentTracker::GetScrapePath() const
{
    return m_scrapePath;
}

void
BitTorrentTracker::SetScrapePath(std::string scrapePath)
{
    m_scrapePath = scrapePath;
}

void
BitTorrentTracker::SetSeederStrategy(std::string strategy)
{
    m_getSeederStrategy = strategy;
}

std::string
BitTorrentTracker::GetAnnounceURL() const
{
    std::stringstream buffer;
    Ptr<Ipv4> ipInterface = GetNode()->GetObject<Ipv4>();

    buffer << "http://";
    ipInterface->GetAddress(1, 0).GetLocal().Print(buffer);
    buffer << ":80";
    buffer << m_announcePath;

    return buffer.str();
}

std::string
BitTorrentTracker::GetScrapeURL() const
{
    std::stringstream buffer;
    Ptr<Ipv4> ipInterface = GetNode()->GetObject<Ipv4>();

    buffer << "http://";
    ipInterface->GetAddress(1, 0).GetLocal().Print(buffer);
    buffer << ":80";
    buffer << m_scrapePath;

    return buffer.str();
}

Time
BitTorrentTracker::GetUpdateInterval() const
{
    return Seconds(lexical_cast<uint32_t>(m_updateInterval));
}

void
BitTorrentTracker::SetUpdateInterval(Time updateInterval)
{
    if (updateInterval.IsStrictlyPositive())
    {
        m_updateInterval = lexical_cast<std::string>(updateInterval.GetSeconds());
    }
}

Ptr<Torrent>
BitTorrentTracker::AddTorrent(std::string path, std::string file)
{
    Ptr<Torrent> torrent = CreateObject<Torrent>();
    torrent->ReadTorrentFile(file);
    torrent->SetDataPath(path);
    std::string info_hash = torrent->GetInfoHash();
    std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);
    AddInfoHash(info_hash);
    torrent->SetAnnounceURL(GetAnnounceURL());
    (*m_cloudInfo.find(info_hash)).second.m_completed = 0;
    return torrent;
}

void
BitTorrentTracker::PrepareForManyClients(Ptr<Torrent> torrent, uint32_t expectedClients)
{
    std::string info_hash = torrent->GetInfoHash();
    std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);

    std::map<std::string, BitTorrentTrackerCloudInfo>::iterator cloudInfoIt;
    cloudInfoIt = m_cloudInfo.find(info_hash);
    if (cloudInfoIt != m_cloudInfo.end())
    {
        (*cloudInfoIt).second.m_clients.get_allocator().allocate(expectedClients);
    }
}

void
BitTorrentTracker::IgnoreTorrent(Ptr<Torrent> torrent)
{
    std::string info_hash = torrent->GetInfoHash();
    std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);
    RemoveInfoHash(info_hash);
}

void
BitTorrentTracker::AcceptTorrent(Ptr<Torrent> torrent)
{
    std::string info_hash = torrent->GetInfoHash();
    std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);
    AddInfoHash(info_hash);
}

void
BitTorrentTracker::ConnectionCreation(Ptr<Socket> socket, const Address& addr)
{
    // Set the callback for this connection by calling the ConnectionCreated method in the server
    m_handleConnectionCreated(socket, addr, m_handleReceiveReq);
}

void
BitTorrentTracker::ReceiveReq(Ptr<Socket> socket)
{
    // Receive the request from the client by calling the receive request method in the servrer
    m_handleReceiveRequest(socket, m_handleDataCreater, m_handleErrorOccurance);
}

void
BitTorrentTracker::ErrorOccurance(Ptr<Socket> socket, std::string ErrorCode, const Address& fromAddress)
{
    // In case of an error, send nothing
    m_handleSendData(socket, 0, 0, ErrorCode);
}

void
BitTorrentTracker::DataCreater(std::string path, Ptr<Socket> socket, const Address& fromAddress)
{
    // Only act if announce/scrape URL was correctly submitted
    NS_LOG_INFO("We recv datacenter called");
    if ((path.find(GetAnnouncePath() + '?') != 0) && (path.find(GetScrapePath() + '?') != 0))
    {
        NS_LOG_WARN("BitTorrentTracker: Could not find announce or scrape path in GET URL received from client.");

        m_handleErrorOccurance(socket, "404", fromAddress);
        return;
    }
    else
    {
        std::string response;
        BTDict clientInfo;
        BTDict::iterator clientInfoIt;

        clientInfo = ExtractInfoFromClientMessage(path);

        // If the IP was not supplied as an argument in the announce string, retrieve it from the packet
        if (clientInfo.find("ip") == clientInfo.end())
        {
            InetSocketAddress iaddr = InetSocketAddress::ConvertFrom(fromAddress);
            std::stringstream converterSS;
            converterSS << iaddr.GetIpv4();
            clientInfo.insert(std::pair<std::string, std::string>("ip", converterSS.str()));
        }

        std::string peer_id = "";
        clientInfoIt = clientInfo.find("info_hash");
        if (clientInfo.find("peer_id") == clientInfo.end())
        {
            if (clientInfoIt == clientInfo.end())
            {
                NS_LOG_WARN("BitTorrentTracker: Could not find the info_hash in the request.");
                m_handleErrorOccurance(socket, "400", fromAddress);
                return;
            }
            else
            {
                BTDict::iterator eventInfoIt = clientInfo.find("event");
                if (eventInfoIt != clientInfo.end())
                {
                    if ((*eventInfoIt).second != "scrape")
                    {
                        NS_LOG_WARN("BitTorrentTracker: Could not find the scrape keyword in a request not containing \"info_hash\".");
                        m_handleErrorOccurance(socket, "400", fromAddress);
                        return;
                    }
                }
                else
                {
                    NS_LOG_WARN("BitTorrentTracker: Could not figure out what the client wants.");
                    m_handleErrorOccurance(socket, "400", fromAddress);
                    return;
                }
            }
        }
        else
        {
            peer_id = (*clientInfo.find("peer_id")).second;
        }
        std::string info_hash = (*clientInfo.find("info_hash")).second;
        std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);
        clientInfo["info_hash"] = info_hash;
        if (IsStreamInfo(clientInfo))
        {
            std::string streamHash = (*clientInfo.find("StreamHash")).second;
            std::transform(streamHash.begin(), streamHash.end(), streamHash.begin(), toupper);
            clientInfo["StreamHash"] = streamHash;
        }

        if (!IsStreamInfo(clientInfo) && m_cloudInfo.find(info_hash) == m_cloudInfo.end())
        {
            m_handleErrorOccurance(socket, "404", fromAddress);
            NS_LOG_WARN("BitTorrentTracker: Could not find shared file with info hash " << info_hash << ".");
            return;
        }

        if (clientInfo.find("numwant") == clientInfo.end())
        {
            clientInfo.insert(std::pair<std::string, std::string>("numwant", "50"));
        }

        clientInfoIt = clientInfo.find("event");

        if (clientInfoIt == clientInfo.end())
        {
            UpdateClient(clientInfo);
            response = GenerateResponseForPeer(clientInfo);
        }
        else
        {
            std::string eventType = (*clientInfoIt).second;
            if (eventType == "started")
            {
                if (clientInfo.find("port") == clientInfo.end() || clientInfo.find("uploaded") == clientInfo.end() ||
                    clientInfo.find("downloaded") == clientInfo.end() || clientInfo.find("left") == clientInfo.end())
                {
                    NS_LOG_WARN("BitTorrentTracker: A needed argument for a \"started\" message is missing from client "
                                << (*clientInfo.find("ip")).second << ".");

                    m_handleErrorOccurance(socket, "400", fromAddress);
                    return;
                }

                AddClient(clientInfo);
                response = GenerateResponseForPeer(clientInfo);

                GlobalMetricsGatherer::GetInstance()->WriteToFile("start-announced", peer_id, true);
            }
            else if (eventType == "completed")
            {
                (*m_cloudInfo.find(info_hash)).second.m_completed++;
                AddClient(clientInfo);
                SetClientToSeeder(clientInfo);
                response = GenerateResponseForPeer(clientInfo);

                GlobalMetricsGatherer::GetInstance()->WriteToFile("completion-announced", peer_id, true);

                // Announce the completion of an external client to the global metrics gatherer.
                if (peer_id.find("VODSim") == std::string::npos)
                {
                    GlobalMetricsGatherer::GetInstance()->AnnounceFinishedExternalClient();
                }
            }
            else if (eventType == "stopped")
            {
                RemoveClient(clientInfo);
                response = GenerateResponseForPeer(clientInfo);
            }
            else if (eventType == "scrape")
            {
                response = GenerateResponseForPeer(clientInfo);
            }
            else if (eventType == "getseeder")
            {
                UpdateClientBacksource(clientInfo);
                response = GenerateResponseForPeer(clientInfo);
            }
            else
            {
                NS_LOG_INFO("BitTorrentTracker: Registered unknown event from peer " << (*clientInfo.find("ip")).second << ": \"" << eventType
                                                                                     << "\".");

                m_handleErrorOccurance(socket, "400", fromAddress);
                return;
            }
        }

        uint8_t* responsePtr = new uint8_t[response.size()];
        response.copy(reinterpret_cast<char*>(responsePtr), response.size());
        NS_LOG_DEBUG("Tracker: generate response \"" << response << "\"");
        m_handleSendData(socket, responsePtr, response.size(), "200");
        delete[] responsePtr;
    }
}

std::string
BitTorrentTracker::GenerateResponseForPeer(const BTDict& clientInfo) const
{
    std::string result;
    // bool isLiveStreaming = clientInfo.find("LiveStreaming") != clientInfo.end();
    if (IsStreamInfo(clientInfo))
    {
        std::string streamHash = clientInfo.find("StreamHash")->second;
        std::transform(streamHash.begin(), streamHash.end(), streamHash.begin(), toupper);
        std::map<std::string, BitTorrentTrackerCloudInfo>::const_iterator cloudInfoIt;
        cloudInfoIt = m_cloudInfo.find(streamHash);
        auto seederInfo = GetSeeders(streamHash, 0, "OTHER", BT_STREAM_PEERTYPE_CLIENT);
        std::string targetDevice = BT_STREAM_PEERTYPE_PCDN;
        if (clientInfo.find("event") == clientInfo.end() || (*(clientInfo.find("event"))).second.compare("scrape") != 0)
        {
            // announce (regular update)
            std::string peer_id = (*(clientInfo.find("peer_id"))).second;

            // Step 1: Tell the client about the current status of the swarm
            result = "d";
            result += "8:intervali" + m_updateInterval + "e";
            result += "8:completei" + lexical_cast<std::string>((*cloudInfoIt).second.m_seeders.size()) + "e";
            result +=
                "10:incompletei" + lexical_cast<std::string>((*cloudInfoIt).second.m_clients.size() - (*cloudInfoIt).second.m_seeders.size()) + "e";
            result += "10:tracker id" + lexical_cast<std::string>(peer_id.size()) + ":" + peer_id;

            if (clientInfo.find("PeerType")->second == BT_STREAM_PEERTYPE_CLIENT)
            {
                if (clientInfo.find("event") != clientInfo.end() && clientInfo.find("event")->second == "started")
                {
                    // client的最开始连接就需要返回
                    seederInfo = GetSeeders(streamHash, 1, m_getSeederStrategy, BT_STREAM_PEERTYPE_PCDN);
                    targetDevice = BT_STREAM_PEERTYPE_PCDN;
                }
                else if (clientInfo.find("event") != clientInfo.end() && clientInfo.find("event")->second == "getseeder")
                {
                    seederInfo = GetSeeders(streamHash, 1, m_getSeederStrategy, BT_STREAM_PEERTYPE_PCDN);
                    targetDevice = BT_STREAM_PEERTYPE_PCDN;
                }
                else
                {
                    NS_LOG_INFO("client regular update do not need generate peers");
                }
            }
            else if (clientInfo.find("PeerType")->second == BT_STREAM_PEERTYPE_PCDN)
            {
                if (clientInfo.find("event") == clientInfo.end())
                {
                    // regular update 也不返回

                    NS_LOG_INFO("PCDN regular update do not need generate peers");
                }
                else if (clientInfo.find("event")->second == "started")
                {
                    // PCDN的最开始连接不需要返回

                    NS_LOG_INFO("PCDN start up do not need generate peers");
                    // seederInfo = GetSeeders(streamHash, 1, m_getSeederStrategy);
                }
                else if (clientInfo.find("event")->second == "getseeder")
                {
                    // PCDN在getseeder的时候返回
                    seederInfo = GetSeeders(streamHash, 1, m_getSeederStrategy, BT_STREAM_PEERTYPE_CDN);
                    targetDevice = BT_STREAM_PEERTYPE_CDN;
                }
                else
                {
                    NS_LOG_INFO("PCDN do not need generate peers");
                }
            }
            else
            {
                // THIS is CDN
                NS_LOG_INFO("CDN do not need generate peers");
            }

            // Step 2: Get a random selection of peers and pass it to the client -->
            result += "5:peersl";
            std::set<std::string>::const_iterator tmpIt;
            const BTDoubleDict* tmplist = nullptr;
            // uint32_t len;
            if (targetDevice == BT_STREAM_PEERTYPE_CDN)
            {
                tmpIt = m_avaliableCDN.begin();
                tmplist = &m_CDNInfo;
                // len = m_CDNInfo.size();
            }
            else if (targetDevice == BT_STREAM_PEERTYPE_PCDN)
            {
                tmpIt = m_avaliablePCDN.begin();
                tmplist = &m_PCDNInfo;

                // len = m_PCDNInfo.size();
            }
            else
            {
                NS_FATAL_ERROR("CLIENT PART now donot allow to request");
            }
            // std::set<uint32_t> indices =
            //     Utilities::GetRandomSampleF2(lexical_cast<uint32_t>((*(clientInfo.find("numwant"))).second),
            //                                  (*cloudInfoIt).second.m_clients.size()); // Gets random numbers from >>1<< to m_clients.size() => Be
            //                                  sure
            //                                                                           // to always subtract 1 when using these as indices!

            // BTDoubleDict::const_iterator clientsIt = (*cloudInfoIt).second.m_clients.begin();
            // uint32_t curPos = 0;
            std::string curStr;
            for (auto indexIt = seederInfo.begin(); indexIt != seederInfo.end(); ++indexIt)
            {
                // while (curPos < (*indexIt) - 1)
                // {
                //     ++curPos;
                //     ++clientsIt;
                // }
                auto nowIt = tmpIt;

                for (uint32_t curPos = 0; curPos < *indexIt - 1; curPos++)
                {
                    nowIt++;
                }
                BTDict curClient;
                // try{
                curClient = tmplist->at(*nowIt);

                // }catch(const std::out_of_range OOR){
                //     std::cout << "ERROR\n--------------\n";
                //     std::cout << *nowIt << std::endl;
                //     for(auto i = tmplist->begin(); i != tmplist->end(); i++)
                //     {
                //         std::cout << i->first << "\n";
                //     }
                //     std::cout << "--------------\n";
                //     NS_FATAL_ERROR("EXIT");

                // }
                if ((*curClient.find("peer_id")).second != (*(clientInfo.find("peer_id"))).second)
                {
                    result += "d";
                    curStr = (*curClient.find("peer_id")).second;
                    result += "7:peer id" + lexical_cast<std::string>(curStr.size()) + ":" + curStr;
                    curStr = (*curClient.find("ip")).second;
                    result += "2:ip" + lexical_cast<std::string>(curStr.size()) + ":" + curStr;
                    curStr = (*curClient.find("port")).second;
                    result += "4:porti" + curStr + "e";
                    curStr = streamHash;
                    result += "10:streamhash" + lexical_cast<std::string>(curStr.size()) + ":" + curStr;
                    result += "e";
                    NS_LOG_INFO("Tracker: assign peer@" << (*curClient.find("ip")).second << "to " << (*(clientInfo.find("peer_id"))).second);
                }
                else
                {
                    NS_LOG_WARN("IREEGULAR");
                }
            }

            result += "e"; // Client list
            // <-- Step 2: Get a random selection of peers and pass it to the client

            result += "e"; // Root dictionary
        }
        else
        {
            NS_LOG_WARN(this << "recv info without event keyword!");
        }
    }
    else
    {
        std::string info_hash = (*(clientInfo.find("info_hash"))).second;
        std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);

        std::map<std::string, BitTorrentTrackerCloudInfo>::const_iterator cloudInfoIt;
        cloudInfoIt = m_cloudInfo.find(info_hash);

        // Generate an answer for an announce (first case) or a scrape request (second case)
        if (clientInfo.find("event") == clientInfo.end() || (*(clientInfo.find("event"))).second.compare("scrape") != 0)
        {
            std::string peer_id = (*(clientInfo.find("peer_id"))).second;

            // Step 1: Tell the client about the current status of the swarm
            result = "d";
            result += "8:intervali" + m_updateInterval + "e";
            result += "8:completei" + lexical_cast<std::string>((*cloudInfoIt).second.m_seeders.size()) + "e";
            result +=
                "10:incompletei" + lexical_cast<std::string>((*cloudInfoIt).second.m_clients.size() - (*cloudInfoIt).second.m_seeders.size()) + "e";
            result += "10:tracker id" + lexical_cast<std::string>(peer_id.size()) + ":" + peer_id;

            // Step 2: Get a random selection of peers and pass it to the client -->
            result += "5:peersl";

            std::set<uint32_t> indices =
                Utilities::GetRandomSampleF2(lexical_cast<uint32_t>((*(clientInfo.find("numwant"))).second),
                                             (*cloudInfoIt).second.m_clients.size()); // Gets random numbers from >>1<< to m_clients.size() => Be sure
                                                                                      // to always subtract 1 when using these as indices!

            BTDoubleDict::const_iterator clientsIt = (*cloudInfoIt).second.m_clients.begin();
            uint32_t curPos = 0;
            std::string curStr;
            for (std::set<uint32_t>::const_iterator indexIt = indices.begin(); indexIt != indices.end(); ++indexIt)
            {
                while (curPos < (*indexIt) - 1)
                {
                    ++curPos;
                    ++clientsIt;
                }

                BTDict curClient = (*clientsIt).second;
                if ((*curClient.find("peer_id")).second != (*(clientInfo.find("peer_id"))).second)
                {
                    result += "d";
                    curStr = (*curClient.find("peer_id")).second;
                    result += "7:peer id" + lexical_cast<std::string>(curStr.size()) + ":" + curStr;
                    curStr = (*curClient.find("ip")).second;
                    result += "2:ip" + lexical_cast<std::string>(curStr.size()) + ":" + curStr;
                    curStr = (*curClient.find("port")).second;
                    result += "4:porti" + curStr + "e";
                    result += "e";
                }
            }

            result += "e"; // Client list
            // <-- Step 2: Get a random selection of peers and pass it to the client

            result += "e"; // Root dictionary
        }
        else if ((*(clientInfo.find("event"))).second.compare("scrape") == 0) // Answer for a scrape
        {
            result += "d5:files";

            if (cloudInfoIt != m_cloudInfo.end())
            {
                // Step 1: Create a dictionary for the supplied info_hash with the hash converted into binary as the key
                std::string binInfoHash;
                char tmpVal;
                for (size_t j = 0; j < info_hash.size(); j++)
                {
                    tmpVal = Utilities::GetValueOfHexChar(info_hash[j]) * 16;
                    j++;
                    tmpVal += Utilities::GetValueOfHexChar(info_hash[j]);
                    binInfoHash.push_back(tmpVal);
                }

                result += "d20:";
                result += binInfoHash;
                result += "d";

                // Step 2: Add other information to the dictionary
                // Step 2a: "complete" (=number of seeders)
                result += "8:completei";
                result += lexical_cast<std::string>((*cloudInfoIt).second.m_seeders.size());
                result += "e";
                // Step 2b: "downloaded" (=number of complete events)
                result += "10:downloadedi";
                result += lexical_cast<std::string>((*cloudInfoIt).second.m_completed);
                result += "e";
                // Step 2c: "incomplete" (=number of leechers)
                result += "10:incompletei";
                result += lexical_cast<std::string>((*cloudInfoIt).second.m_clients.size() - (*cloudInfoIt).second.m_seeders.size());
                result += "e";
                result += "e";
                result += "e";
            }

            result += "e";
        }
    }
    return result;
}

BTDict
BitTorrentTracker::ExtractInfoFromClientMessage(std::string path)
{
    std::map<std::string, std::string> result;

    std::string key, value;

    size_t questionmarkPos = path.find('?', 0);
    size_t equalPos = path.find('=', questionmarkPos);
    size_t ampersandPos = path.find('&', equalPos);

    size_t scrapePos = path.find("scrape", 0);
    bool isScrape = false;

    if (scrapePos != std::string::npos)
    {
        result.insert(std::pair<std::string, std::string>("event", "scrape"));
        isScrape = true;
    }

    if (questionmarkPos == std::string::npos || equalPos == std::string::npos || (ampersandPos == std::string::npos && !isScrape))
    {
        return result;
    }

    if (!isScrape)
    {
        // Insert first part of GET request
        key = path.substr(questionmarkPos + 1, equalPos - questionmarkPos - 1);
        value = path.substr(equalPos + 1, ampersandPos - equalPos - 1);
        if (key == "info_hash")
        {
            value = Utilities::DecodeInfoHash(value);
            std::transform(value.begin(), value.end(), value.begin(), toupper);
        }
        else
        {
            value = Utilities::UnescapeHex(value);
        }
        result.insert(std::pair<std::string, std::string>(key, value));

        // Insert the rest
        if (ampersandPos != std::string::npos)
        {
            while (ampersandPos != path.size())
            {
                equalPos = path.find('=', ampersandPos);

                key = path.substr(ampersandPos + 1, equalPos - ampersandPos - 1);

                ampersandPos = path.find('&', equalPos);

                if (ampersandPos == std::string::npos)
                {
                    ampersandPos = path.size();
                }

                value = path.substr(equalPos + 1, ampersandPos - equalPos - 1);
                if (key == "info_hash")
                {
                    value = Utilities::DecodeInfoHash(value);
                    std::transform(value.begin(), value.end(), value.begin(), toupper);
                }
                else
                {
                    value = Utilities::UnescapeHex(value);
                }

                result.insert(std::pair<std::string, std::string>(key, value));
            }
        }
    }
    else
    {
        key = path.substr(questionmarkPos + 1, equalPos - questionmarkPos - 1);
        value = path.substr(equalPos + 1, path.size() - equalPos);
        if (key == "info_hash")
        {
            value = Utilities::DecodeInfoHash(value);
            std::transform(value.begin(), value.end(), value.begin(), toupper);
        }
        else
        {
            value = Utilities::UnescapeHex(value);
        }
        result.insert(std::pair<std::string, std::string>(key, value));
    }

    return result;
}

void
BitTorrentTracker::AddInfoHash(std::string info_hash)
{
    std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);

    BitTorrentTrackerCloudInfo btci;
    m_cloudInfo.insert(std::pair<std::string, BitTorrentTrackerCloudInfo>(info_hash, btci));
    NS_LOG_INFO("BitTorrentTracker: Now accepting torrents with info hash " << info_hash << ".");
}

void
BitTorrentTracker::RemoveInfoHash(std::string info_hash)
{
    std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);

    m_cloudInfo.erase(info_hash);
    NS_LOG_INFO("BitTorrentTracker: Will not accept torrents with info hash " << info_hash << " from now on.");
}

void
BitTorrentTracker::AddStreamHash(std::string streamHash)
{
    std::transform(streamHash.begin(), streamHash.end(), streamHash.begin(), toupper);
    if (m_cloudInfo.find(streamHash) != m_cloudInfo.end())
    {
        return;
    }

    BitTorrentTrackerCloudInfo btci;
    m_cloudInfo.insert(std::pair<std::string, BitTorrentTrackerCloudInfo>(streamHash, btci));
    NS_LOG_INFO("BitTorrentTracker: Now accepting stream with info hash " << streamHash << ".");
}

void
BitTorrentTracker::RemoveStreamHash(std::string info_hash)
{
    std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);

    m_cloudInfo.erase(info_hash);
    NS_LOG_INFO("BitTorrentTracker: Will not accept stream with info hash " << info_hash << " from now on.");
}

bool
BitTorrentTracker::IsStreamInfo(const BTDict& clientInfo) const
{
    return (clientInfo.find("LiveStreaming") != clientInfo.end());
}

void
BitTorrentTracker::AddClient(BTDict& clientInfo)
{
    if (IsStreamInfo(clientInfo))
    {
        std::string streamHash = clientInfo.find("StreamHash")->second;
        std::transform(streamHash.begin(), streamHash.end(), streamHash.begin(), toupper);
        std::string peer_id = (*(clientInfo.find("peer_id"))).second;
        std::map<std::string, BitTorrentTrackerCloudInfo>::iterator cloudInfoIt;
        std::string peer_type = clientInfo.find("PeerType")->second;
        NS_LOG_INFO("We find a streaming client " << peer_id << " type " << peer_type << " with Hash " << streamHash);

        if (peer_type == BT_STREAM_PEERTYPE_CDN)
        {
            m_CDNInfo[peer_id] = clientInfo;
            m_avaliableCDN.insert(peer_id);
        }
        else if (peer_type == BT_STREAM_PEERTYPE_PCDN)
        {
            m_PCDNInfo[peer_id] = clientInfo;
            m_PCDNBacksource[peer_id] = std::set<std::string>();
            m_avaliablePCDN.insert(peer_id);
        }
        else if (peer_type == BT_STREAM_PEERTYPE_CLIENT)
        {
            cloudInfoIt = m_cloudInfo.find(streamHash);
            cloudInfoIt->second.m_clients[peer_id] = clientInfo;
        }
        else
        {
            NS_FATAL_ERROR("UNKNOWN PEER TYPE" << peer_type);
        }
        NS_LOG_INFO("client " << peer_type << " with id " << peer_id << " now has inside the cloud");
    }
    else
    {
        std::string info_hash = (*(clientInfo.find("info_hash"))).second;
        std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);
        std::string peer_id = (*(clientInfo.find("peer_id"))).second;
        std::map<std::string, BitTorrentTrackerCloudInfo>::iterator cloudInfoIt;

        cloudInfoIt = m_cloudInfo.find(info_hash);

        (*cloudInfoIt).second.m_clients.insert(std::pair<std::string, BTDict>(peer_id, clientInfo));
        NS_LOG_INFO("BitTorrentTracker: Clients in cloud: " << (*cloudInfoIt).second.m_clients.size());
    }
}

void
BitTorrentTracker::UpdateClient(BTDict& clientInfo)
{
    if (IsStreamInfo(clientInfo))
    {
        std::string streamHash = clientInfo.find("StreamHash")->second;
        std::transform(streamHash.begin(), streamHash.end(), streamHash.begin(), toupper);
        std::string peer_id = (*(clientInfo.find("peer_id"))).second;
        std::map<std::string, BitTorrentTrackerCloudInfo>::iterator cloudInfoIt;
        std::string peer_type = clientInfo.find("PeerType")->second;
        BTDict* oldClientInfo;

        if (peer_type == BT_STREAM_PEERTYPE_CDN)
        {
            if (m_CDNInfo.find(peer_id) == m_CDNInfo.end())
            {
                return;
            }
            oldClientInfo = &m_CDNInfo[peer_id];
            if (lexical_cast<int>(clientInfo.find("Connected")->second) >= lexical_cast<int>(clientInfo.find("ConnectMax")->second))
            {
                m_avaliableCDN.erase(peer_id);
            }
            else
            {
                m_avaliableCDN.insert(peer_id);
            }
        }
        else if (peer_type == BT_STREAM_PEERTYPE_PCDN)
        {
            if (m_PCDNInfo.find(peer_id) == m_PCDNInfo.end())
            {
                return;
            }
            oldClientInfo = &m_PCDNInfo[peer_id];
            if (lexical_cast<int>(clientInfo.find("Connected")->second) >= lexical_cast<int>(clientInfo.find("ConnectMax")->second))
            {
                m_avaliablePCDN.erase(peer_id);
                NS_LOG_INFO("Tracker:: erase avaliavle PCDN list " << clientInfo.find("ip")->second );
            }
            else
            {
                m_avaliablePCDN.insert(peer_id);
            }
            NS_LOG_INFO("Tracker:: recv update @" << clientInfo.find("ip")->second << " with connected "
                                                  << clientInfo.find("Connected")->second << " and connectMax "
                                                  << clientInfo.find("ConnectMax")->second);
            // m_PCDNInfo[peer_id] = clientInfo;
        }
        else if (peer_type == BT_STREAM_PEERTYPE_CLIENT)
        {
            cloudInfoIt = m_cloudInfo.find(streamHash);
            oldClientInfo = &cloudInfoIt->second.m_clients[peer_id];
        }
        else
        {
            NS_FATAL_ERROR("UNKNOWN PEER TYPE" << peer_type);
            return;
        }
        (*oldClientInfo)["uploaded"] = clientInfo["uploaded"];
        (*oldClientInfo)["downloaded"] = clientInfo["downloaded"];
        (*oldClientInfo)["left"] = clientInfo["left"];
        (*oldClientInfo)["Connected"] = clientInfo["Connected"];
        (*oldClientInfo)["ConnectMax"] = clientInfo["ConnectMax"];
        // (*oldClientInfo)["left"] = clientInfo["left"];
        return;
    }
    std::string info_hash = (*(clientInfo.find("info_hash"))).second;
    std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);
    std::string peer_id = (*(clientInfo.find("peer_id"))).second;
    std::map<std::string, BitTorrentTrackerCloudInfo>::iterator cloudInfoIt;
    BTDoubleDict::iterator clientsIt;
    BTDict* oldClientInfo;
    BTDict::iterator clientIt;

    cloudInfoIt = m_cloudInfo.find(info_hash);
    clientsIt = (*cloudInfoIt).second.m_clients.find(peer_id);

    if (clientsIt != (*cloudInfoIt).second.m_clients.end())
    {
        oldClientInfo = &((*clientsIt).second);
    }
    else
    {
        clientsIt = (*cloudInfoIt).second.m_seeders.find(peer_id);
        if (clientsIt != (*cloudInfoIt).second.m_seeders.end())
        {
            oldClientInfo = &((*clientsIt).second);
        }
        else
        {
            return;
        }
    }

    // Note: oldClient info is assumed to be a valid struct / object from this point on
    (*oldClientInfo)["uploaded"] = clientInfo["uploaded"];
    (*oldClientInfo)["downloaded"] = clientInfo["downloaded"];
    (*oldClientInfo)["left"] = clientInfo["left"];
}

void
BitTorrentTracker::SetClientToSeeder(BTDict& clientInfo)
{
    std::string info_hash = (*(clientInfo.find("info_hash"))).second;
    std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);
    std::string peer_id = (*(clientInfo.find("peer_id"))).second;
    std::map<std::string, BitTorrentTrackerCloudInfo>::iterator cloudInfoIt;

    cloudInfoIt = m_cloudInfo.find(info_hash);
    clientInfo.insert(std::pair<std::string, std::string>("finished after", lexical_cast<std::string>(Simulator::Now().GetSeconds())));
    (*cloudInfoIt).second.m_seeders.insert(std::pair<std::string, BTDict>(peer_id, clientInfo));
}

void
BitTorrentTracker::RemoveClient(const BTDict& clientInfo)
{
    if (IsStreamInfo(clientInfo))
    {
        std::string streamHash = clientInfo.find("StreamHash")->second;
        std::transform(streamHash.begin(), streamHash.end(), streamHash.begin(), toupper);
        std::string peer_id = (*(clientInfo.find("peer_id"))).second;
        std::map<std::string, BitTorrentTrackerCloudInfo>::iterator cloudInfoIt;
        std::string peer_type = clientInfo.find("PeerType")->second;
        BTDict* oldClientInfo;

        if (peer_type == BT_STREAM_PEERTYPE_CDN)
        {
            if (m_CDNInfo.find(peer_id) == m_CDNInfo.end())
            {
                return;
            }
            m_CDNInfo.erase(peer_id);
            m_avaliableCDN.erase(peer_id);
        }
        else if (peer_type == BT_STREAM_PEERTYPE_PCDN)
        {
            if (m_PCDNInfo.find(peer_id) == m_PCDNInfo.end())
            {
                return;
            }
            // oldClientInfo = &m_PCDNInfo[peer_id];
            m_PCDNInfo.erase(peer_id);
            m_PCDNBacksource.erase(peer_id);
            m_avaliablePCDN.erase(peer_id);
        }
        else if (peer_type == BT_STREAM_PEERTYPE_CLIENT)
        {
            cloudInfoIt = m_cloudInfo.find(streamHash);
            cloudInfoIt->second.m_clients.erase(peer_id);
        }
        else
        {
            NS_FATAL_ERROR("UNKNOWN PEER TYPE" << peer_type);
            return;
        }
        return;
    }
    std::string info_hash = (*(clientInfo.find("info_hash"))).second;
    std::transform(info_hash.begin(), info_hash.end(), info_hash.begin(), toupper);
    std::string peer_id = (*(clientInfo.find("peer_id"))).second;
    std::map<std::string, BitTorrentTrackerCloudInfo>::iterator cloudInfoIt;
    BTDoubleDict::iterator clientsIt;

    cloudInfoIt = m_cloudInfo.find(info_hash);

    clientsIt = (*cloudInfoIt).second.m_clients.find(peer_id);
    if (clientsIt != (*cloudInfoIt).second.m_clients.end())
    {
        (*cloudInfoIt).second.m_clients.erase(clientsIt);
    }

    clientsIt = (*cloudInfoIt).second.m_seeders.find(peer_id);
    if (clientsIt != (*cloudInfoIt).second.m_seeders.end())
    {
        // First, log seeder as gone (for thesis purposes), then remove it
        (*cloudInfoIt).second.m_leftSeeders.insert(std::pair<std::string, BTDict>(peer_id, clientInfo));
        (*cloudInfoIt).second.m_seeders.erase(clientsIt);
    }
}

std::set<uint32_t>
BitTorrentTracker::GetSeeders(const std::string streamHash, int requireNum, std::string strategy, std::string targetDevice) const
{
    std::set<uint32_t> result;
    if (strategy == "random")
    {
        result = GetSeedersRandom(streamHash, requireNum, targetDevice);
    }
    else if (strategy == "treeFirst")
    {
        result = GetSeedersTreeFirst(streamHash, requireNum, targetDevice);
    }
    else
    {
    }
    return result;
}

std::set<uint32_t>
BitTorrentTracker::GetSeedersRandom(const std::string streamHash, int requireNum, std::string targetDevice) const
{
    NS_LOG_INFO("Random get seeder called");
    // std::set<BTDoubleDict::const_iterator> result;
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    // std::map<std::string, BitTorrentTrackerCloudInfo>::const_iterator cloudInfoIt;
    // cloudInfoIt = m_cloudInfo.find(streamHash);
    BTDoubleDict::const_iterator tmpIt;
    uint32_t len;
    if (targetDevice == BT_STREAM_PEERTYPE_CDN)
    {
        // tmpIt = m_CDNInfo.begin();
        len = m_avaliableCDN.size();
    }
    else if (targetDevice == BT_STREAM_PEERTYPE_PCDN)
    {
        // tmpIt = m_avaliablePCDN.begin();
        len = m_avaliablePCDN.size();
    }
    else
    {
        NS_FATAL_ERROR("CLIENT PART now donot allow to request");
    }
    std::set<uint32_t> result = Utilities::GetRandomSampleF2(requireNum, len);
    // auto nowIt = tmpIt;
    // int nowPos = 0;
    // for(auto it = indices.begin(); it != indices.end(); it++)
    // {
    //     nowIt = tmpIt;
    //     while(nowPos < (*it - 1))
    //     {
    //         nowPos++;
    //         nowIt++;
    //     }
    //     result.insert(nowIt);

    // }
    return result;
}

std::set<uint32_t>
ns3::bittorrent::BitTorrentTracker::GetSeedersTreeFirst(const std::string streamHash, int requireNum, std::string targetDevice) const
{
    std::set<uint32_t> result;
    std::set<std::string>::const_iterator tmpIt;
    // const BTDoubleDict* tmplist = nullptr;
    uint32_t len;
    if (targetDevice == BT_STREAM_PEERTYPE_CDN)
    {
        tmpIt = m_avaliableCDN.begin();
        // tmplist = &m_CDNInfo;
        len = m_avaliableCDN.size();
    }
    else if (targetDevice == BT_STREAM_PEERTYPE_PCDN)
    {
        tmpIt = m_avaliablePCDN.begin();
        // tmplist = &m_PCDNInfo;

        len = m_avaliablePCDN.size();
    }
    else
    {
        NS_FATAL_ERROR("CLIENT PART now donot allow to request");
    }

    uint32_t nowpos = 0;
    auto nowIt = tmpIt;
    while (result.size() < requireNum && nowpos < len)
    {
        auto backsourceIT = m_PCDNBacksource.find(*nowIt);
        if (backsourceIT != m_PCDNBacksource.end())
        {
            if (backsourceIT->second.contains(streamHash))
            {
                result.insert(nowpos + 1);
                NS_LOG_INFO("Tracker:" << "found a tree node");
            }
        }

        // result.insert(nowpos);
        // if()

        nowIt++;
        nowpos++;
    }
    nowpos = 0;
    nowIt = tmpIt;
    std::list<uint32_t> perm = Utilities::GetPermutationP(len, len);
    for (auto i : perm)
    {
        if (result.size() >= requireNum)
        {
            break;
        }
        result.insert(i);
    }
    // while(result.size() < requireNum && nowpos < len)
    // {
    //     // auto backsourceIT = m_PCDNBacksource.find(*tmpIt);
    //     // if(backsourceIT != m_PCDNBacksource.end())
    //     // {
    //         // if(backsourceIT->second.contains(streamHash))
    //     result.insert(nowpos + 1);
    //     // }

    //     // result.insert(nowpos);
    //     // if()

    //     nowIt++;
    //     nowpos++;
    // }
    return result;
}

std::set<uint32_t>
BitTorrentTracker::GetSeedersOptimal(const std::string streamHash, int requireNum, std::string targetDevice) const
{
    return std::set<uint32_t>();
}

} // namespace bittorrent
} // namespace ns3
