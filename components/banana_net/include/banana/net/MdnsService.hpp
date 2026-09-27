#pragma once

#include <cstdint>

#include "banana/core/Result.hpp"

namespace banana::net {

/// mDNS responder (RAII around mdns_init/mdns_free): `<hostname>.local` + an HTTP service record.
class MdnsService {
public:
    [[nodiscard]] static Result<MdnsService> start(const char* hostname, const char* instanceName,
                                                   std::uint16_t httpPort = 80);

    MdnsService(const MdnsService&) = delete;
    MdnsService& operator=(const MdnsService&) = delete;
    MdnsService(MdnsService&& other) noexcept;
    MdnsService& operator=(MdnsService&& other) noexcept;
    ~MdnsService();

private:
    MdnsService() : running_(true) {}
    void release();

    bool running_ = false;
};

} // namespace banana::net
