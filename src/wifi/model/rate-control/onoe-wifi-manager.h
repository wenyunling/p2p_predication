/*
 * Copyright (c) 2003,2007 INRIA
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#ifndef ONOE_WIFI_MANAGER_H
#define ONOE_WIFI_MANAGER_H

#include "ns3/traced-value.h"
#include "ns3/wifi-remote-station-manager.h"

namespace ns3
{

struct OnoeWifiRemoteStation;

/**
 * @brief an implementation of the rate control algorithm developed
 *        by Atsushi Onoe
 *
 * @ingroup wifi
 *
 * This algorithm is well known because it has been used as the default
 * rate control algorithm for the madwifi driver. I am not aware of
 * any publication or reference about this algorithm beyond the madwifi
 * source code.
 *
 * This RAA does not support HT modes and will error
 * exit if the user tries to configure this RAA with a Wi-Fi MAC
 * that supports 802.11n or higher.
 */
class OnoeWifiManager : public WifiRemoteStationManager
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    OnoeWifiManager();
    ~OnoeWifiManager() override;

  private:
    void DoInitialize() override;
    WifiRemoteStation* DoCreateStation() const override;
    void DoReportRxOk(WifiRemoteStation* station, double rxSnr, WifiMode txMode) override;
    void DoReportRtsFailed(WifiRemoteStation* station) override;
    void DoReportDataFailed(WifiRemoteStation* station) override;
    void DoReportRtsOk(WifiRemoteStation* station,
                       double ctsSnr,
                       WifiMode ctsMode,
                       double rtsSnr) override;
    void DoReportDataOk(WifiRemoteStation* station,
                        double ackSnr,
                        WifiMode ackMode,
                        double dataSnr,
                        MHz_u dataChannelWidth,
                        uint8_t dataNss) override;
    void DoReportFinalRtsFailed(WifiRemoteStation* station) override;
    void DoReportFinalDataFailed(WifiRemoteStation* station) override;
    WifiTxVector DoGetDataTxVector(WifiRemoteStation* station, MHz_u allowedWidth) override;
    WifiTxVector DoGetRtsTxVector(WifiRemoteStation* station) override;

    /**
     * Update the number of retry (both short and long).
     *
     * @param station the ONOE wifi remote station
     */
    void UpdateRetry(OnoeWifiRemoteStation* station);
    /**
     * Update the mode.
     *
     * @param station the ONOE wifi remote station
     */
    void UpdateMode(OnoeWifiRemoteStation* station);

    Time m_updatePeriod;           ///< update period
    uint32_t m_addCreditThreshold; ///< add credit threshold
    uint32_t m_raiseThreshold;     ///< raise threshold

    TracedValue<uint64_t> m_currentRate; //!< Trace rate changes
};

} // namespace ns3

#endif /* ONOE_WIFI_MANAGER_H */
