#pragma once

#ifdef __linux__
// todo Impl linux version
#elif _WIN32
#include <Windows.h>
#pragma comment(lib, "Rpcrt4.lib")
#else
#error Unsupported platform
#endif

#include <optional>
#include <string>
#include <iomanip>
#include <array>
#include <sstream>

namespace aZero::Helper
{
    struct UUID
    {
        unsigned long  Data1;
        unsigned short Data2;
        unsigned short Data3;
        std::array<unsigned char, 8> Data4;

        static constexpr std::size_t UUID_Size = 36;

        static constexpr std::size_t GetSizeAsString()
        {
            return UUID_Size;
        }

        static std::string StringFromCharArray(const std::array<char, UUID_Size>& array)
        {
            return std::string(array.data(), array.size());
        }

        static std::array<char, UUID_Size> CharArrayFromString(const std::string& str)
        {
            std::array<char, UUID_Size> arr;
            std::memcpy(arr.data(), str.data(), UUID_Size);
            return arr;
        }

        std::array<char, UUID_Size> ToCharArray() const
        {
            return CharArrayFromString(this->ToString());
        }

        std::string ToString() const
        {
            std::ostringstream ss;
            ss << std::hex << std::setfill('0')
                << std::setw(8) << Data1 << '-'
                << std::setw(4) << Data2 << '-'
                << std::setw(4) << Data3 << '-'
                << std::setw(2) << static_cast<unsigned>(Data4[0])
                << std::setw(2) << static_cast<unsigned>(Data4[1]) << '-'
                << std::setw(2) << static_cast<unsigned>(Data4[2])
                << std::setw(2) << static_cast<unsigned>(Data4[3])
                << std::setw(2) << static_cast<unsigned>(Data4[4])
                << std::setw(2) << static_cast<unsigned>(Data4[5])
                << std::setw(2) << static_cast<unsigned>(Data4[6])
                << std::setw(2) << static_cast<unsigned>(Data4[7]);

            return ss.str();
        }

        bool FromString(std::string_view str)
        {
            // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
            if (str.size() != 36 ||
                str[8] != '-' ||
                str[13] != '-' ||
                str[18] != '-' ||
                str[23] != '-')
            {
                return false;
            }

            UUID parsed{};

            auto parseHex = [](std::string_view text, auto& value) -> bool
                {
                    unsigned int temp = 0;

                    const auto result = std::from_chars(
                        text.data(),
                        text.data() + text.size(),
                        temp,
                        16
                    );

                    if (result.ec != std::errc{} ||
                        result.ptr != text.data() + text.size())
                    {
                        return false;
                    }

                    value = static_cast<std::remove_reference_t<decltype(value)>>(temp);
                    return true;
                };

            if (!parseHex(str.substr(0, 8), parsed.Data1) ||
                !parseHex(str.substr(9, 4), parsed.Data2) ||
                !parseHex(str.substr(14, 4), parsed.Data3) ||
                !parseHex(str.substr(19, 2), parsed.Data4[0]) ||
                !parseHex(str.substr(21, 2), parsed.Data4[1]) ||
                !parseHex(str.substr(24, 2), parsed.Data4[2]) ||
                !parseHex(str.substr(26, 2), parsed.Data4[3]) ||
                !parseHex(str.substr(28, 2), parsed.Data4[4]) ||
                !parseHex(str.substr(30, 2), parsed.Data4[5]) ||
                !parseHex(str.substr(32, 2), parsed.Data4[6]) ||
                !parseHex(str.substr(34, 2), parsed.Data4[7]))
            {
                return false;
            }

            *this = parsed;
            return true;
        }

        UUID() = default;
#ifdef __linux__
        // todo Impl linux version
#elif _WIN32
        explicit UUID(const ::UUID& uuid) {
            std::memcpy(this, &uuid, sizeof(UUID));
        }
#endif
    };

    static inline std::optional<aZero::Helper::UUID> CreateUUID()
    {
#ifdef __linux__
        // todo Impl linux version
#error Linux implementation not yet available
#elif _WIN32
        ::UUID uuid;
        if (UuidCreate(&uuid) != RPC_S_OK) {
            return std::nullopt;
        }
        return UUID(uuid);
#endif
    }
}