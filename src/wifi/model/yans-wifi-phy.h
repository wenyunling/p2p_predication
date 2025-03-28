/*
 * Copyright (c) 2005,2006 INRIA
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 *          Ghada Badawy <gbadawy@gmail.com>
 *          Sébastien Deronne <sebastien.deronne@gmail.com>
 */

#ifndef YANS_WIFI_PHY_H
#define YANS_WIFI_PHY_H

#include "wifi-phy.h"

namespace ns3
{

class YansWifiChannel;

/**
 * @brief 802.11 PHY layer model
 * @ingroup wifi
 *
 * This PHY implements a model of 802.11a. The model
 * implemented here is based on the model described
 * in "Yet Another Network Simulator" published in WNS2 2006;
 * an author-prepared version of this paper is at:
 * https://hal.inria.fr/file/index/docid/78318/filename/yans-rr.pdf
 *
 * This PHY model depends on a channel loss and delay
 * model as provided by the ns3::PropagationLossModel
 * and ns3::PropagationDelayModel classes, both of which are
 * members of the ns3::YansWifiChannel class.
 */
class YansWifiPhy : public WifiPhy
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    YansWifiPhy();
    ~YansWifiPhy() override;

    void SetInterferenceHelper(const Ptr<InterferenceHelper> helper) override;
    void StartTx(Ptr<const WifiPpdu> ppdu) override;
    Ptr<Channel> GetChannel() const override;
    MHz_u GetGuardBandwidth(MHz_u currentChannelWidth) const override;
    std::tuple<dBr_u, dBr_u, dBr_u> GetTxMaskRejectionParams() const override;
    WifiSpectrumBandInfo GetBand(MHz_u bandWidth, uint8_t bandIndex = 0) override;
    FrequencyRange GetCurrentFrequencyRange() const override;
    WifiSpectrumBandFrequencies ConvertIndicesToFrequencies(
        const WifiSpectrumBandIndices& indices) const override;

    /**
     * Set the YansWifiChannel this YansWifiPhy is to be connected to.
     *
     * @param channel the YansWifiChannel this YansWifiPhy is to be connected to
     */
    void SetChannel(const Ptr<YansWifiChannel> channel);

    /**
     * Logs the arrival of a PPDU, including its power and duration.
     * This will also trace PPDUs below WifiPhy::RxSensitivity
     *
     * @param [in] ppdu The PPDU being traced upon its arrival.
     * @param [in] rxPowerDbm The received power of the PPDU in dBm.
     * @param [in] duration The duration of the PPDU signal.
     */
    void TraceSignalArrival(Ptr<const WifiPpdu> ppdu, double rxPowerDbm, Time duration);

    /**
     * Callback invoked when the PHY model starts to process a signal
     *
     * @param ppdu The PPDU being processed
     * @param rxPowerDbm received signal power (dBm)
     * @param duration Signal duration
     */
    typedef void (*SignalArrivalCallback)(Ptr<const WifiPpdu> ppdu,
                                          double rxPowerDbm,
                                          Time duration);

  protected:
    void DoDispose() override;

  private:
    void FinalizeChannelSwitch() override;

    Ptr<YansWifiChannel> m_channel; //!< YansWifiChannel that this YansWifiPhy is connected to

    TracedCallback<Ptr<const WifiPpdu>, double, Time>
        m_signalArrivalCb; //!< Signal Arrival callback
};

} // namespace ns3

#endif /* YANS_WIFI_PHY_H */
