#pragma once

#include "Rtt/Rtc/LocalSigClient.h"
#include "Rtt/Rtc/RtcClient.h"
#include "Rtt/Rtc/RtcServer.h"
#include "Rtt/Rtc/SigHub.h"
#include "Rtt/Transport.h"

#include <memory>
#include <string>

namespace IPeer
{
    /// Available transport modes for peer communication.
    enum class TransportMode : int
    {
        InProcessRtc = 0, ///< WebRTC via in-process SigHub (no network).
    };

    inline const char* TransportModeName(TransportMode mode)
    {
        switch (mode) {
            case TransportMode::InProcessRtc: return "In-Process RTC";
        }
        return "Unknown";
    }

    /// Creates per-peer ITransport instances based on the selected mode.
    ///
    /// For InProcessRtc: all peers share a SigHub; each gets a LocalSigClient
    /// and a RtcClient transport (which handles both outbound and inbound).
    class TransportFactory
    {
    public:
        explicit TransportFactory(TransportMode mode = TransportMode::InProcessRtc)
            : _mode(mode)
            , _sigHub(std::make_shared<Rtt::Rtc::SigHub>())
        {}

        [[nodiscard]] TransportMode Mode() const { return _mode; }

        /// Create a transport for a peer with the given string ID.
        [[nodiscard]] std::shared_ptr<Rtt::ITransport> CreateTransport(const std::string& peerId)
        {
            switch (_mode) {
                case TransportMode::InProcessRtc: {
                    auto sigClient = std::make_shared<Rtt::Rtc::LocalSigClient>(_sigHub);
                    return Rtt::Rtc::RtcClient::MakeDefault({
                        .sigClient = sigClient,
                        .localId = {peerId},
                    });
                }
            }
            return nullptr;
        }

    private:
        TransportMode _mode;
        std::shared_ptr<Rtt::Rtc::SigHub> _sigHub;
    };
}
