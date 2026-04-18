#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace Demo
{
    /// Application-level message type tag (first byte of every message).
    enum class MsgType : std::uint8_t
    {
        SyncProbe     = 1, ///< SynTm sync probe (request or response, wrapped).
        PayloadUpdate = 2, ///< Position/velocity state broadcast.
    };

    /// Position/velocity payload broadcast.
    struct PayloadUpdateMsg
    {
        float x;
        float y;
        float vx;
        float vy;
        std::int64_t syncTimeNs; ///< Synchronized time in nanoseconds when this update was created.
    };
    static_assert(std::is_trivially_copyable_v<PayloadUpdateMsg>);

    // -----------------------------------------------------------------------
    // Serialization
    // -----------------------------------------------------------------------

    /// Wrap raw SynTm sync bytes with our application-level MsgType::SyncProbe header.
    [[nodiscard]] inline std::vector<std::byte> WrapSyncProbe(std::span<const std::byte> syncData)
    {
        std::vector<std::byte> buf(1 + syncData.size());
        buf[0] = static_cast<std::byte>(MsgType::SyncProbe);
        std::memcpy(buf.data() + 1, syncData.data(), syncData.size());
        return buf;
    }

    [[nodiscard]] inline std::vector<std::byte> SerializePayloadUpdate(const PayloadUpdateMsg& msg)
    {
        std::vector<std::byte> buf(1 + sizeof(PayloadUpdateMsg));
        buf[0] = static_cast<std::byte>(MsgType::PayloadUpdate);
        std::memcpy(buf.data() + 1, &msg, sizeof(PayloadUpdateMsg));
        return buf;
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
