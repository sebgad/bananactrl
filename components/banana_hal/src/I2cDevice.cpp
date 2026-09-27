#include "banana/hal/I2cDevice.hpp"

#include <utility>

namespace banana::hal {
namespace {

constexpr int kTimeoutMs = 100;

} // namespace

I2cDevice::I2cDevice(I2cDevice&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)), address_(other.address_)
{
}

I2cDevice& I2cDevice::operator=(I2cDevice&& other) noexcept
{
    if (this != &other) {
        release();
        handle_ = std::exchange(other.handle_, nullptr);
        address_ = other.address_;
    }
    return *this;
}

I2cDevice::~I2cDevice()
{
    release();
}

Result<void> I2cDevice::write(std::span<const std::uint8_t> data)
{
    return toResult(i2c_master_transmit(handle_, data.data(), data.size(), kTimeoutMs));
}

Result<void> I2cDevice::writeRead(std::span<const std::uint8_t> data, std::span<std::uint8_t> response)
{
    return toResult(i2c_master_transmit_receive(handle_, data.data(), data.size(), response.data(),
                                                response.size(), kTimeoutMs));
}

void I2cDevice::release()
{
    if (handle_ != nullptr) {
        i2c_master_bus_rm_device(handle_);
        handle_ = nullptr;
    }
}

} // namespace banana::hal
