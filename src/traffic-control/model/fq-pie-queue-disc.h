/*
 * Copyright (c) 2016 Universita' degli Studi di Napoli Federico II
 * Copyright (c) 2018 NITK Surathkal (modified for FQ-PIE)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Pasquale Imputato <p.imputato@gmail.com>
 *          Stefano Avallone <stefano.avallone@unina.it>
 * Modified for FQ-PIE by:  Sumukha PK <sumukhapk46@gmail.com>
 *                          Prajval M  <26prajval98@gmail.com>
 *                          Ishaan R D <ishaanrd6@gmail.com>
 *                          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef FQ_PIE_QUEUE_DISC
#define FQ_PIE_QUEUE_DISC

#include "queue-disc.h"

#include "ns3/object-factory.h"

#include <list>
#include <map>

namespace ns3
{

/**
 * @ingroup traffic-control
 *
 * @brief A flow queue used by the FqPie queue disc
 */

class FqPieFlow : public QueueDiscClass
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    /**
     * @brief FqPieFlow constructor
     */
    FqPieFlow();

    ~FqPieFlow() override;

    /**
     * @enum FlowStatus
     * @brief Used to determine the status of this flow queue
     */
    enum FlowStatus
    {
        INACTIVE,
        NEW_FLOW,
        OLD_FLOW
    };

    /**
     * @brief Set the deficit for this flow
     * @param deficit the deficit for this flow
     */
    void SetDeficit(uint32_t deficit);
    /**
     * @brief Get the deficit for this flow
     * @return the deficit for this flow
     */
    int32_t GetDeficit() const;
    /**
     * @brief Increase the deficit for this flow
     * @param deficit the amount by which the deficit is to be increased
     */
    void IncreaseDeficit(int32_t deficit);
    /**
     * @brief Set the status for this flow
     * @param status the status for this flow
     */
    void SetStatus(FlowStatus status);
    /**
     * @brief Get the status of this flow
     * @return the status of this flow
     */
    FlowStatus GetStatus() const;
    /**
     * @brief Set the index for this flow
     * @param index the index for this flow
     */
    void SetIndex(uint32_t index);
    /**
     * @brief Get the index of this flow
     * @return the index of this flow
     */
    uint32_t GetIndex() const;

  private:
    int32_t m_deficit;   //!< the deficit for this flow
    FlowStatus m_status; //!< the status of this flow
    uint32_t m_index;    //!< the index for this flow
};

/**
 * @ingroup traffic-control
 *
 * @brief A FqPie packet queue disc
 */

class FqPieQueueDisc : public QueueDisc
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    /**
     * @brief FqPieQueueDisc constructor
     */
    FqPieQueueDisc();

    ~FqPieQueueDisc() override;

    /**
     * @brief Set the quantum value.
     *
     * @param quantum The number of bytes each queue gets to dequeue on each round of the scheduling
     * algorithm
     */
    void SetQuantum(uint32_t quantum);

    /**
     * @brief Get the quantum value.
     *
     * @returns The number of bytes each queue gets to dequeue on each round of the scheduling
     * algorithm
     */
    uint32_t GetQuantum() const;

    // Reasons for dropping packets
    static constexpr const char* UNCLASSIFIED_DROP =
        "Unclassified drop"; //!< No packet filter able to classify packet
    static constexpr const char* OVERLIMIT_DROP = "Overlimit drop"; //!< Overlimit dropped packets

  private:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    bool CheckConfig() override;
    void InitializeParams() override;

    /**
     * @brief Drop a packet from the head of the queue with the largest current byte count
     * @return the index of the queue with the largest current byte count
     */
    uint32_t FqPieDrop();

    /**
     * Compute the index of the queue for the flow having the given flowHash,
     * according to the set associative hash approach.
     *
     * @param flowHash the hash of the flow 5-tuple
     * @return the index of the queue for the given flow
     */
    uint32_t SetAssociativeHash(uint32_t flowHash);

    // PIE queue disc parameter
    bool m_useEcn;          //!< True if ECN is used (packets are marked instead of being dropped)
    double m_markEcnTh;     //!< ECN marking threshold (default 10% as suggested in RFC 8033)
    Time m_ceThreshold;     //!< Threshold above which to CE mark
    bool m_useL4s;          //!< True if L4S is used (ECT1 packets are marked at CE threshold)
    Time m_sUpdate;         //!< Start time of the update timer
    Time m_tUpdate;         //!< Time period after which CalculateP () is called
    Time m_qDelayRef;       //!< Desired queue delay
    uint32_t m_meanPktSize; //!< Average packet size in bytes
    Time m_maxBurst;        //!< Maximum burst allowed before random early dropping kicks in
    double m_a;             //!< Parameter to pie controller
    double m_b;             //!< Parameter to pie controller
    uint32_t m_dqThreshold; //!< Minimum queue size in bytes before dequeue rate is measured
    bool m_useDqRateEstimator; //!< Enable/Disable usage of dequeue rate estimator for queue delay
                               //!< calculation
    bool
        m_isCapDropAdjustment; //!< Enable/Disable Cap Drop Adjustment feature mentioned in RFC 8033
    bool m_useDerandomization; //!< Enable Derandomization feature mentioned in RFC 8033

    // Fq parameters
    uint32_t m_quantum;              //!< Deficit assigned to flows at each round
    uint32_t m_flows;                //!< Number of flow queues
    uint32_t m_setWays;              //!< size of a set of queues (used by set associative hash)
    uint32_t m_dropBatchSize;        //!< Max number of packets dropped from the fat flow
    uint32_t m_perturbation;         //!< hash perturbation value
    bool m_enableSetAssociativeHash; //!< whether to enable set associative hash

    std::list<Ptr<FqPieFlow>> m_newFlows; //!< The list of new flows
    std::list<Ptr<FqPieFlow>> m_oldFlows; //!< The list of old flows

    std::map<uint32_t, uint32_t> m_flowsIndices; //!< Map with the index of class for each flow
    std::map<uint32_t, uint32_t> m_tags;         //!< Tags used by set associative hash

    ObjectFactory m_flowFactory;      //!< Factory to create a new flow
    ObjectFactory m_queueDiscFactory; //!< Factory to create a new queue
};

} // namespace ns3

#endif /* FQ_PIE_QUEUE_DISC */
