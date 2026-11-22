#include "warehouse/domain.hpp"

#include <cctype>
#include <stdexcept>

namespace warehouse {
namespace {

bool is_space(char character) {
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

bool has_outer_whitespace(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    return is_space(text.front()) || is_space(text.back());
}

bool is_valid_identifier(const std::string& text) {
    return !text.empty() && !has_outer_whitespace(text);
}

}  // namespace

void Inventory::add_station(const std::string& station_id, int stock) {
    if (!is_valid_identifier(station_id)) {
        throw std::invalid_argument(
            "station id must not be empty or have outer whitespace");
    }
    if (stock < 0) {
        throw std::invalid_argument("station stock must not be negative");
    }
    if (stations_.find(station_id) != stations_.end()) {
        throw std::invalid_argument("station id already exists");
    }

    stations_.emplace(
        station_id,
        PickStation{station_id, stock, RobotState::idle, ""});
}

std::optional<PickStation> Inventory::station(
    const std::string& station_id) const {
    const auto station_position = stations_.find(station_id);
    if (station_position == stations_.end()) {
        return std::nullopt;
    }
    return station_position->second;
}

std::vector<PickStation> Inventory::stations() const {
    std::vector<PickStation> result;
    result.reserve(stations_.size());

    for (const auto& entry : stations_) {
        result.push_back(entry.second);
    }
    return result;
}

std::vector<PickStation> Inventory::available_for(const Order& order) const {
    std::vector<PickStation> candidates;
    if (order.quantity <= 0) {
        return candidates;
    }

    for (const auto& entry : stations_) {
        const PickStation& station = entry.second;
        if (station.robot_state != RobotState::idle) {
            continue;
        }
        if (station.stock < order.quantity) {
            continue;
        }
        candidates.push_back(station);
    }
    return candidates;
}

bool Inventory::reserve(const Order& order, const std::string& station_id) {
    if (!is_valid_identifier(order.id) ||
        !is_valid_identifier(order.sku) ||
        order.quantity <= 0) {
        return false;
    }
    if (!is_valid_identifier(station_id)) {
        return false;
    }

    auto station_position = stations_.find(station_id);
    if (station_position == stations_.end()) {
        return false;
    }

    PickStation& station = station_position->second;
    if (station.robot_state != RobotState::idle) {
        return false;
    }
    if (station.stock < order.quantity) {
        return false;
    }

    station.stock -= order.quantity;
    station.robot_state = RobotState::busy;
    station.current_order_id = order.id;
    return true;
}

bool Inventory::finish(const std::string& station_id,
                       const std::string& order_id) {
    auto station_position = stations_.find(station_id);
    if (station_position == stations_.end()) {
        return false;
    }

    PickStation& station = station_position->second;
    if (station.robot_state != RobotState::busy) {
        return false;
    }
    if (station.current_order_id != order_id) {
        return false;
    }

    station.robot_state = RobotState::idle;
    station.current_order_id.clear();
    return true;
}

}  // namespace warehouse
