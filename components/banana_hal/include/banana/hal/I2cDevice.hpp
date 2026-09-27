#pragma once

#include <cstdint>
#include <span>

#include "driver/i2c_master.h"

#include "banana/core/Result.hpp"

namespace banana::hal {

class I2cBus;

/// Device on an I2cBus (RAII around i2c_master_bus_add_device). Created by I2cBus::addDevice().
class I2cDevice {
public:
    I2cDevice(const I2cDevice&) = delete;
    I2cDevice& operator=(const I2cDevice&) = delete;
    I2cDevice(I2cDevice&& other) noexcept;
    I2cDevice& operator=(I2cDevice&& other) noexcept;
    ~I2cDevice();

    [[nodiscard]] Result<void> write(std::span<const std::uint8_t> data);
    /// Write (e.g. a register pointer), repeated start, read.
    [[nodiscard]] Result<void> writeRead(std::span<const std::uint8_t> data,
                                         std::span<std::uint8_t> response);

    [[nodiscard]] std::uint16_t address() const { return address_; }

private:
    friend class I2cBus;
    I2cDevice(i2c_master_dev_handle_t handle, std::uint16_t address) : handle_(handle), address_(address) {}
    void release();

    i2c_master_dev_handle_t handle_ = nullptr;
    std::uint16_t address_ = 0;
};

} // namespace banana::hal
