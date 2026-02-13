#pragma once
#include <string>

namespace url_gen {

void set_base_prefix(std::string prefix);

// GET
std::string main_mage();
std::string building();
std::string building_type();
std::string building(int index);
std::string building_type(int index);

// POST
std::string new_user();
std::string new_building();
std::string set_building();
std::string set_transfer();
std::string activity(int index);
std::string supply(int index);
std::string demand(int index);
std::string new_demand();
std::string new_supply();
}