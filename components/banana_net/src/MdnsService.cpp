#include "banana/net/MdnsService.hpp"

#include <utility>

#include "mdns.h"

namespace banana::net {

Result<MdnsService> MdnsService::start(const char* hostname, const char* instanceName, std::uint16_t httpPort)
{
    if (auto res = toResult(mdns_init()); !res) {
        return fail(res.error());
    }
    MdnsService service; // frees mDNS again if a later step fails
    if (auto res = toResult(mdns_hostname_set(hostname)); !res) {
        return fail(res.error());
    }
    if (auto res = toResult(mdns_instance_name_set(instanceName)); !res) {
        return fail(res.error());
    }
    if (auto res = toResult(mdns_service_add(nullptr, "_http", "_tcp", httpPort, nullptr, 0)); !res) {
        return fail(res.error());
    }
    return service;
}

MdnsService::MdnsService(MdnsService&& other) noexcept : running_(std::exchange(other.running_, false))
{
}

MdnsService& MdnsService::operator=(MdnsService&& other) noexcept
{
    if (this != &other) {
        release();
        running_ = std::exchange(other.running_, false);
    }
    return *this;
}

MdnsService::~MdnsService()
{
    release();
}

void MdnsService::release()
{
    if (running_) {
        mdns_free();
        running_ = false;
    }
}

} // namespace banana::net
