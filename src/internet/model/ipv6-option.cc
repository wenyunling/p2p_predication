/*
 * Copyright (c) 2007-2009 Strasbourg University
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: David Gross <gdavid.devel@gmail.com>
 */

#include "ipv6-option.h"

#include "ipv6-option-header.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Ipv6Option");

NS_OBJECT_ENSURE_REGISTERED(Ipv6Option);

TypeId
Ipv6Option::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ipv6Option")
                            .SetParent<Object>()
                            .SetGroupName("Internet")
                            .AddAttribute("OptionNumber",
                                          "The IPv6 option number.",
                                          UintegerValue(0),
                                          MakeUintegerAccessor(&Ipv6Option::GetOptionNumber),
                                          MakeUintegerChecker<uint8_t>());
    return tid;
}

Ipv6Option::~Ipv6Option()
{
    NS_LOG_FUNCTION(this);
}

void
Ipv6Option::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    m_node = node;
}

NS_OBJECT_ENSURE_REGISTERED(Ipv6OptionPad1);

TypeId
Ipv6OptionPad1::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ipv6OptionPad1")
                            .SetParent<Ipv6Option>()
                            .SetGroupName("Internet")
                            .AddConstructor<Ipv6OptionPad1>();
    return tid;
}

Ipv6OptionPad1::Ipv6OptionPad1()
{
    NS_LOG_FUNCTION(this);
}

Ipv6OptionPad1::~Ipv6OptionPad1()
{
    NS_LOG_FUNCTION(this);
}

uint8_t
Ipv6OptionPad1::GetOptionNumber() const
{
    NS_LOG_FUNCTION(this);

    return OPT_NUMBER;
}

uint8_t
Ipv6OptionPad1::Process(Ptr<Packet> packet,
                        uint8_t offset,
                        const Ipv6Header& ipv6Header,
                        bool& isDropped)
{
    NS_LOG_FUNCTION(this << packet << offset << ipv6Header << isDropped);

    Ptr<Packet> p = packet->Copy();
    p->RemoveAtStart(offset);

    Ipv6OptionPad1Header pad1Header;
    p->RemoveHeader(pad1Header);

    isDropped = false;

    return pad1Header.GetSerializedSize();
}

NS_OBJECT_ENSURE_REGISTERED(Ipv6OptionPadn);

TypeId
Ipv6OptionPadn::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ipv6OptionPadn")
                            .SetParent<Ipv6Option>()
                            .SetGroupName("Internet")
                            .AddConstructor<Ipv6OptionPadn>();
    return tid;
}

Ipv6OptionPadn::Ipv6OptionPadn()
{
    NS_LOG_FUNCTION(this);
}

Ipv6OptionPadn::~Ipv6OptionPadn()
{
    NS_LOG_FUNCTION(this);
}

uint8_t
Ipv6OptionPadn::GetOptionNumber() const
{
    NS_LOG_FUNCTION(this);

    return OPT_NUMBER;
}

uint8_t
Ipv6OptionPadn::Process(Ptr<Packet> packet,
                        uint8_t offset,
                        const Ipv6Header& ipv6Header,
                        bool& isDropped)
{
    NS_LOG_FUNCTION(this << packet << offset << ipv6Header << isDropped);

    Ptr<Packet> p = packet->Copy();
    p->RemoveAtStart(offset);

    Ipv6OptionPadnHeader padnHeader;
    p->RemoveHeader(padnHeader);

    isDropped = false;

    return padnHeader.GetSerializedSize();
}

NS_OBJECT_ENSURE_REGISTERED(Ipv6OptionJumbogram);

TypeId
Ipv6OptionJumbogram::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ipv6OptionJumbogram")
                            .SetParent<Ipv6Option>()
                            .SetGroupName("Internet")
                            .AddConstructor<Ipv6OptionJumbogram>();
    return tid;
}

Ipv6OptionJumbogram::Ipv6OptionJumbogram()
{
    NS_LOG_FUNCTION(this);
}

Ipv6OptionJumbogram::~Ipv6OptionJumbogram()
{
    NS_LOG_FUNCTION(this);
}

uint8_t
Ipv6OptionJumbogram::GetOptionNumber() const
{
    NS_LOG_FUNCTION(this);

    return OPT_NUMBER;
}

uint8_t
Ipv6OptionJumbogram::Process(Ptr<Packet> packet,
                             uint8_t offset,
                             const Ipv6Header& ipv6Header,
                             bool& isDropped)
{
    NS_LOG_FUNCTION(this << packet << offset << ipv6Header << isDropped);

    Ptr<Packet> p = packet->Copy();
    p->RemoveAtStart(offset);

    Ipv6OptionJumbogramHeader jumbogramHeader;
    p->RemoveHeader(jumbogramHeader);

    isDropped = false;

    return jumbogramHeader.GetSerializedSize();
}

NS_OBJECT_ENSURE_REGISTERED(Ipv6OptionRouterAlert);

TypeId
Ipv6OptionRouterAlert::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ipv6OptionRouterAlert")
                            .SetParent<Ipv6Option>()
                            .SetGroupName("Internet")
                            .AddConstructor<Ipv6OptionRouterAlert>();
    return tid;
}

Ipv6OptionRouterAlert::Ipv6OptionRouterAlert()
{
    NS_LOG_FUNCTION(this);
}

Ipv6OptionRouterAlert::~Ipv6OptionRouterAlert()
{
    NS_LOG_FUNCTION(this);
}

uint8_t
Ipv6OptionRouterAlert::GetOptionNumber() const
{
    NS_LOG_FUNCTION(this);

    return OPT_NUMBER;
}

uint8_t
Ipv6OptionRouterAlert::Process(Ptr<Packet> packet,
                               uint8_t offset,
                               const Ipv6Header& ipv6Header,
                               bool& isDropped)
{
    NS_LOG_FUNCTION(this << packet << offset << ipv6Header << isDropped);

    Ptr<Packet> p = packet->Copy();
    p->RemoveAtStart(offset);

    Ipv6OptionRouterAlertHeader routerAlertHeader;
    p->RemoveHeader(routerAlertHeader);

    isDropped = false;

    return routerAlertHeader.GetSerializedSize();
}

} /* namespace ns3 */
