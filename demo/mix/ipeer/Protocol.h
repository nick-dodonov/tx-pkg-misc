#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace IPeer
{
    /// Application-level message type tag (first byte of every message).
    enum class MsgType : std::uint8_t
    {
        SyncProbeReq  = 1, ///< Time sync probe request.
        SyncProbeResp = 2, ///< Time sync probe response.
        PayloadUpdate = 3, ///< Position/velocity state broadcast.
    };

    /// Sync probe request: originator records send time.
    struct SyncProbeReqMsg
    {
        double t1; ///< Originator send timestamp (session seconds).
    };
    static_assert(std::is_trivially_copyable_v<SyncProbeReqMsg>);

    /// Sync probe response: NTP-like 3-timestamp model.
    struct SyncProbeRespMsg
    {
        double t1; ///< Original request send time (echoed back).
        double t2; ///< Responder receive time.
        double t3; ///< Responder send time.
    };
    static_assert(std::is_trivially_copyable_v<SyncProbeRespMsg>);

    /// Position/velocity payload broadcast.
    struct PayloadUpdateMsg
    {
        float x;
        float y;
        float vx;
        float vy;
        double timestamp; ///< Session time when this update was created.
    };
    static_assert(std::is_trivially_copyable_v<PayloadUpdateMsg>);

    // -----------------------------------------------------------------------
    // Serialization
    // -----------------------------------------------------------------------

    template <typename T>
    [[nodiscard]] inline std::vector<std::byte> Serialize(MsgType type, const T& msg)
    {
        std::vector<std::byte> buf(1 + sizeof(T));
        buf[0] = static_cast<std::byte>(type);
        std::memcpy(buf.data() + 1, &msg, sizeof(T));
        return buf;
    }

    [[nodiscard]] inline std::vector<std::byte> SerializeSyncProbeReq(const SyncProbeReqMsg& msg)
    {
        return Serialize(MsgType::SyncProbeReq, msg);
    }

    [[nodiscard]] inline std::vector<std::byte> SerializeSyncProbeResp(const SyncProbeRespMsg& msg)
    {
        return Serialize(MsgType::SyncProbeResp, msg);
    }

    [[nodiscard]] inline std::vector<std::byte> SerializePayloadUpdate(const PayloadUpdateMsg& msg)
    {
        return Serialize(MsgType::PayloadUpdate, msg);
    }

    // -----------------------------------------------------------------------
    // Parsing
    // -----------------------------------------------------------------------

    struct ParsedMessage
    {
        MsgType type;
        std::span<const std::byte> payload; ///< Points into the original buffer (no copy).
    };

    [[nodiscard]] inline std::optional<ParsedMessage> ParseMessage(std::span<const std::byte> buf)
    {
        if (buf.empty()) {
            return std::nullopt;
        }
        return ParsedMessage{
            .type = static_cast<MsgType>(buf[0]),
            .payload = buf.subspan(1),
        };
    }

    template <typename T>
    [[nodiscard]] inline std::optional<T> ReadPayload(std::span<const std::byte> payload)
    {
        if (payload.size() < sizeof(T)) {
            return std::nullopt;
        }
        T msg;
        std::memcpy(&msg, payload.data(), sizeof(T));
        return msg;
    }
}
