#include "logic/power.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace {
namespace fs = std::filesystem;

std::string read_text(const fs::path& path) {
    std::ifstream input(path);
    std::string value;
    std::getline(input, value);
    return value;
}

// Для отсутствующего показания используем -1. Ток и мощность нужны по модулю.
double number(const fs::path& path, bool absolute = false) {
    std::ifstream input(path);
    double value;
    if (!(input >> value) || !std::isfinite(value)) return -1;
    if (absolute) return std::abs(value);
    return value;
}

// now и full должны быть в одинаковых единицах.
double charge_percent(double now, double full) {
    if (now < 0 || full <= 0) return -1;
    return std::clamp(100 * now / full, 0.0, 100.0);
}

double remaining_minutes(double now, double rate) {
    if (now < 0 || rate <= 0) return -1;
    const double minutes = 60 * now / rate;
    if (!std::isfinite(minutes)) return -1;
    return minutes;
}

Battery read_battery(const fs::path& path) {
    Battery battery;
    battery.name = path.filename().string();
    battery.status = read_text(path / "status");
    if (battery.status.empty()) battery.status = "Unknown";
    const double energy = number(path / "energy_now");
    const double charge = number(path / "charge_now");
    battery.percent = number(path / "capacity");
    if (battery.percent < 0 || battery.percent > 100)
        battery.percent = charge_percent(energy, number(path / "energy_full"));
    if (battery.percent < 0)
        battery.percent = charge_percent(charge, number(path / "charge_full"));

    if (battery.status == "Discharging") {
        battery.minutes = remaining_minutes(energy, number(path / "power_now", true));
        if (battery.minutes < 0)
            battery.minutes = remaining_minutes(charge, number(path / "current_now", true));
    }
    return battery;
}

} // namespace

PowerState read_power() {
    PowerState state;
    std::error_code error;
    fs::directory_iterator it("/sys/class/power_supply", error), end;
    if (error) {
        state.error = "Не удалось прочитать power_supply: " + error.message();
        return state;
    }
    bool adapter_seen = false, adapter_unknown = false, adapter_online = false;
    bool discharging = false, charging = false;
    for (; it != end; it.increment(error)) {
        if (error) break;
        const auto path = it->path();
        // Мыши и прочие периферийные устройства не питают компьютер.
        if (read_text(path / "scope") == "Device") continue;
        const auto type = read_text(path / "type");
        if (type != "Battery") {
            if (type == "Mains" || type.rfind("USB", 0) == 0 || type == "Wireless") {
                adapter_seen = true;
                auto online = number(path / "online");
                if (online == 1) adapter_online = true;
                else if (online != 0) adapter_unknown = true;
            }
            continue;
        }
        if (number(path / "present") == 0) continue;
        auto battery = read_battery(path);
        discharging = discharging || battery.status == "Discharging";
        charging = charging || battery.status == "Charging";
        state.batteries.push_back(battery);
    }
    if (error) state.error = "Ошибка перечисления power_supply: " + error.message();
    if (adapter_online) state.online = 1;
    else if (adapter_seen && !adapter_unknown) state.online = 0;
    else if (charging && !discharging) state.online = 1;
    else if (discharging && !charging) state.online = 0;
    std::sort(state.batteries.begin(), state.batteries.end(),
              [](const Battery& a, const Battery& b) { return a.name < b.name; });
    return state;
}

const Battery* find_battery(const PowerState& state, const std::string& name) {
    for (const Battery& battery : state.batteries) {
        if (battery.name == name) return &battery;
    }
    return nullptr;
}
