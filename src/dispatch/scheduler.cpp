#include "dispatch/scheduler.hpp"

#include <cctype>
#include <stdexcept>
#include <utility>

namespace dispatch {
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

Scheduler::Scheduler(warehouse::Inventory& inventory,
                     std::unique_ptr<DispatchPolicy> policy,
                     audit::Trace& trace)
    : inventory_(inventory), policy_(std::move(policy)), trace_(trace) {
    if (!policy_) {
        throw std::invalid_argument("dispatch policy cannot be null");
    }
}

DispatchDecision Scheduler::submit(const warehouse::Order& order) {
    if (order.quantity <= 0) {
        return {false, order.id, "", "quantity must be positive"};
    }
    if (!is_valid_identifier(order.id)) {
        return {false, order.id, "", "order id is invalid"};
    }
    if (!is_valid_identifier(order.sku)) {
        return {false, order.id, "", "sku is invalid"};
    }

    const std::vector<warehouse::PickStation> candidates =
        inventory_.available_for(order);
    if (candidates.empty()) {
        return {false, order.id, "", "no candidate station"};
    }

    const std::optional<std::string> selected_station =
        policy_->select(order, candidates);
    if (!selected_station.has_value()) {
        return {false, order.id, "", "policy did not select a station"};
    }
    if (!inventory_.reserve(order, *selected_station)) {
        return {false, order.id, "", "reservation failed"};
    }

    trace_.append("DISPATCH " + order.id + " " + *selected_station);
    return {true, order.id, *selected_station, "accepted"};
}

std::vector<DispatchDecision> Scheduler::submit_all(
    const std::vector<warehouse::Order>& orders) {
    std::vector<DispatchDecision> decisions;
    decisions.reserve(orders.size());

    for (const warehouse::Order& order : orders) {
        decisions.push_back(submit(order));
    }
    return decisions;
}

bool Scheduler::complete(const std::string& station_id,
                         const std::string& order_id,
                         std::string& failure_reason) {
    failure_reason.clear();

    const std::optional<warehouse::PickStation> station =
        inventory_.station(station_id);
    if (!station.has_value()) {
        failure_reason = "station does not exist";
        return false;
    }
    if (station->robot_state != warehouse::RobotState::busy) {
        failure_reason = "station is not busy";
        return false;
    }
    if (station->current_order_id != order_id) {
        failure_reason = "order id does not match";
        return false;
    }
    if (!inventory_.finish(station_id, order_id)) {
        failure_reason = "completion failed";
        return false;
    }

    trace_.append("COMPLETE " + order_id + " " + station_id);
    return true;
}

const warehouse::Inventory& Scheduler::inventory() const {
    return inventory_;
}

const audit::Trace& Scheduler::trace() const {
    return trace_;
}

}  // namespace dispatch
