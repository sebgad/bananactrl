#include "banana/hal/I2cBus.hpp"

#include <utility>

namespace banana::hal {
namespace {

constexpr int kTimeoutMs = 100;

} // namespace

Result<I2cBus> I2cBus::create(const Config& config)
{
    i2c_master_bus_config_t busConfig{};
    busConfig.i2c_port = config.port;
    busConfig.sda_io_num = config.sda;
    busConfig.scl_io_num = config.scl;
    busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
    busConfig.glitch_ignore_cnt = 7;
    busConfig.flags.enable_internal_pullup = config.internalPullups ? 1 : 0;

    i2c_master_bus_handle_t handle = nullptr;
    if (auto res = toResult(i2c_new_master_bus(&busConfig, &handle)); !res) {
        return fail(res.error());
    }
    return I2cBus{handle};
}

I2cBus::I2cBus(I2cBus&& other) noexcept : handle_(std::exchange(other.handle_, nullptr))
{
}

I2cBus& I2cBus::operator=(I2cBus&& other) noexcept
{
    if (this != &other) {
        release();
        handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
}

I2cBus::~I2cBus()
{
    release();
}

Result<void> I2cBus::probe(std::uint16_t address) const
{
    return toResult(i2c_master_probe(handle_, address, kTimeoutMs));
}

Result<I2cDevice> I2cBus::addDevice(std::uint16_t address, std::uint32_t sclHz) const
{
    i2c_device_config_t devConfig{};
    devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    devConfig.device_address = address;
    devConfig.scl_speed_hz = sclHz;

    i2c_master_dev_handle_t device = nullptr;
    if (auto res = toResult(i2c_master_bus_add_device(handle_, &devConfig, &device)); !res) {
        return fail(res.error());
    }
    return I2cDevice{device, address};
}

void I2cBus::release()
{
    if (handle_ != nullptr) {
        i2c_del_master_bus(handle_);
        handle_ = nullptr;
    }
}

} // namespace banana::hal
