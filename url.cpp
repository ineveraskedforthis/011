#include "url.hpp"
// static constexpr std::string empty_string {};

static std::string BASE_PREFIX = "/";

namespace url_gen {

void set_base_prefix(std::string prefix){
	BASE_PREFIX = prefix;
}

// GET
std::string main_mage() {
	return BASE_PREFIX;
}
std::string building() {
	return BASE_PREFIX + "building";
}
std::string building_type() {
	return (BASE_PREFIX + "building_type");
}
std::string building(int index) {
	return (BASE_PREFIX + "building?id=") + std::to_string(index);
}
std::string building_type(int index) {
	return (BASE_PREFIX + "building_type?id=") + std::to_string(index);
}

// POST
std::string new_user() {
	return BASE_PREFIX + "login";
}
std::string new_building() {
	return BASE_PREFIX + "building/create";
}
std::string set_building() {
	return BASE_PREFIX + "building/set";
}
std::string set_transfer() {
	return BASE_PREFIX + "transfer/set";
}
std::string activity(int index) {
	return BASE_PREFIX + "activity?id=" + std::to_string(index);
}
std::string supply(int index) {
	return BASE_PREFIX + "supply?id=" + std::to_string(index);
}
std::string demand(int index) {
	return BASE_PREFIX + "demand?id=" + std::to_string(index);
}
std::string new_demand() {
	return BASE_PREFIX + "demand/create";
}
std::string new_supply() {
	return BASE_PREFIX + "supply/create";
}
}