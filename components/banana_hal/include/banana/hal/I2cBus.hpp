#pragma once

#include <cstdint>

#include "driver/i2c_master.h"

#include "banana/core/Result.hpp"
#include "banana/hal/I2cDevice.hpp"

namespace banana::hal {

/// I2C master bus (RAII around i2c_new_master_bus). Devices must be destroyed before the bus.
class I2cBus {
public:
    struct Config {
        gpio_num_t sda = GPIO_NUM_NC;
        gpio_num_t scl = GPIO_NUM_NC;
        i2c_port_num_t port = -1; ///< -1 = first free port
        bool internalPullups = true;
    };

    [[nodiscard]] static Result<I2cBus> create(const Config& config);

    I2cBus(const I2cBus&) = delete;
    I2cBus& operator=(const I2cBus&) = delete;
    I2cBus(I2cBus&& other) noexcept;
    I2cBus& operator=(I2cBus&& other) noexcept;
    ~I2cBus();

    /// ESP_OK if a device acknowledges `address`, ESP_ERR_NOT_FOUND otherwise.
    [[nodiscard]] Result<void> probe(std::uint16_t address) const;

    [[nodiscard]] Result<I2cDevice> addDevice(std::uint16_t address, std::uint32_t sclHz = 100'000) const;

private:
    explicit I2cBus(i2c_master_bus_handle_t handle) : handle_(handle) {}
    void release();

    i2c_master_bus_handle_t handle_ = nullptr;
};

} // namespace banana::hal
