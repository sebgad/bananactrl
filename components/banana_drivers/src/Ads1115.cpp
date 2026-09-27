#include "banana/drivers/Ads1115.hpp"

#include <array>

namespace banana::drivers {

using ads1115::ConfigRegister;
using ads1115::Register;

Result<Ads1115> Ads1115::create(hal::I2cDevice& device, const ITemperatureConverter& converter,
                                const Settings& settings)
{
    Ads1115 ads{device, converter};
    // Reading the config register doubles as presence check.
    if (auto config = ads.readConfig(); !config) {
        return fail(config.error());
    }
    if (auto res = ads.configure(settings); !res) {
        return fail(res.error());
    }
    return ads;
}

Result<void> Ads1115::configure(const Settings& settings)
{
    if (settings.conversionReadyPin) {
        // RDY mode: Hi_thresh MSB = 1, Lo_thresh MSB = 0 (datasheet 9.3.8)
        if (auto res = writeRegister(Register::HighThreshold, 0xFFFF); !res) {
            return res;
        }
        if (auto res = writeRegister(Register::LowThreshold, 0x0000); !res) {
            return res;
        }
    }
    // Written last: in continuous mode this starts the conversions.
    if (auto res = writeRegister(Register::Config, settings.config.pack()); !res) {
        return res;
    }
    auto readBack = readConfig();
    if (!readBack) {
        return fail(readBack.error());
    }
    if (!readBack->sameSettings(settings.config)) {
        return fail(ESP_ERR_INVALID_RESPONSE);
    }
    settings_ = settings;
    filter_.clear();
    return {};
}

Result<float> Ads1115::readCelsius()
{
    auto volts = readVolts();
    if (!volts) {
        return fail(volts.error());
    }
    return converter_->toCelsius(*volts);
}

bool Ads1115::healthy()
{
    const auto config = readConfig();
    return config && config->mode == ads1115::Mode::Continuous;
}

Result<float> Ads1115::readVolts()
{
    auto raw = readRegister(Register::Conversion);
    if (!raw) {
        return fail(raw.error());
    }
    filter_.push(static_cast<std::int16_t>(*raw)); // two's complement
    const float counts = settings_.filterActive ? filter_.value() : static_cast<float>(filter_.latest());
    return counts * ads1115::lsbVolts(settings_.config.pga);
}

Result<ConfigRegister> Ads1115::readConfig()
{
    auto raw = readRegister(Register::Config);
    if (!raw) {
        return fail(raw.error());
    }
    return ConfigRegister::unpack(*raw);
}

Result<std::uint16_t> Ads1115::readRegister(Register reg)
{
    const std::array pointer{static_cast<std::uint8_t>(reg)};
    std::array<std::uint8_t, 2> data{};
    if (auto res = device_->writeRead(pointer, data); !res) {
        return fail(res.error());
    }
    return static_cast<std::uint16_t>((data[0] << 8U) | data[1]); // MSB first
}

Result<void> Ads1115::writeRegister(Register reg, std::uint16_t value)
{
    const std::array data{static_cast<std::uint8_t>(reg), static_cast<std::uint8_t>(value >> 8U),
                          static_cast<std::uint8_t>(value & 0xFFU)};
    return device_->write(data);
}

} // namespace banana::drivers
