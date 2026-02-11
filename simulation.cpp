#include "constants.hpp"
#include "data.hpp"
#include "data_ids.hpp"
#include "unordered_dense.h"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "simulation.hpp"

static dcon::data_container state {};


struct text_collection {
	std::vector<char> text;
	std::vector<uint32_t> word_start;
	std::vector<uint32_t> word_length;
	uint32_t available_key;
};

ankerl::unordered_dense::map<std::string, dcon::user_id> name_to_user;

std::string get_text(text_collection& collection, uint32_t key) {
	return std::string {collection.text.data() + collection.word_start[key]};
}

uint32_t new_text(text_collection& collection, std::string data) {
	auto new_start = collection.text.size();
	auto key = collection.available_key;
	collection.text.resize(collection.text.size() + data.size() + 1);
	std::copy(data.c_str(), data.c_str() + data.size(), collection.text.data() + new_start);
	collection.word_start.push_back(new_start);
	collection.word_length.push_back(data.size());
	collection.available_key++;
	return key;
}

text_collection all_text {};

void init_simulation() {
	state.user_resize_pwd_hash(HASHLEN);

	{
		// commodities

		auto ore_basic = state.create_commodity();
		state.commodity_set_name(ore_basic, new_text(all_text, "Basic ore"));
		state.commodity_set_inversed_density(ore_basic, 125);

		auto fuel_basic = state.create_commodity();
		state.commodity_set_name(fuel_basic, new_text(all_text, "Basic fuel"));
		state.commodity_set_inversed_density(fuel_basic, 1000);

		auto material_basic = state.create_commodity();
		state.commodity_set_name(material_basic, new_text(all_text, "Basic material"));
		state.commodity_set_inversed_density(material_basic, 100);


		state.user_resize_max_storage(state.commodity_size());
		state.user_resize_storage(state.commodity_size());
		state.activity_resize_input(state.commodity_size());
		state.activity_resize_input_amount(state.commodity_size());
		state.activity_resize_output(state.commodity_size());
		state.activity_resize_output_amount(state.commodity_size());
		state.building_type_resize_construction_amount(state.commodity_size());
		state.building_type_resize_construction(state.commodity_size());
		state.building_resize_construction_progress(state.commodity_size());

		// buildings and activities

		auto refine_basic = state.create_activity();
		state.activity_set_name(refine_basic, new_text(all_text, "Refine basic ore"));
		state.activity_set_input(refine_basic, 0, ore_basic);
		state.activity_set_input_amount(refine_basic, 0, 1);
		state.activity_set_output(refine_basic, 0, material_basic);
		state.activity_set_output_amount(refine_basic, 0, 1);

		state.building_type_resize_activities(state.activity_size());

		auto refinery = state.create_building_type();
		state.building_type_set_name(refinery, new_text(all_text, "Refinery"));
		state.building_type_set_activities(refinery, 0, refine_basic);
	}
}


std::string retrieve_balance(dcon::user_id user) {
	auto savings = state.user_get_wealth(user);
	std::string representation {};
	for (int i = 0; i < 128 * std::log10(2); i++) {
		auto digit =  (uint8_t) savings % 10;
		representation = std::to_string(digit) + representation;
		savings = savings / 10;
		if (savings == 0) {
			if (i == 10) {
				representation = "0." + representation;
			} else if (i > 10) {
				break;
			}
		}
	}

	return representation;
}


std::string retrieve_user_name(dcon::user_id user){
	return get_text(all_text, state.user_get_name(user));
}

dcon::user_id create_or_get_user(std::string name, uint8_t password_hash[HASHLEN]) {
	auto it = name_to_user.find(name);
	if (it == name_to_user.end()) {
		auto user = state.create_user();
		name_to_user[name] = user;
		state.user_set_name(user, new_text(all_text, name));

		for (uint8_t i = 0; i < HASHLEN; i++) {
			state.user_set_pwd_hash(user, i, password_hash[i]);
		}
		return user;
	} else {
		auto user = it->second;

		bool hash_equal = true;
		for (uint8_t i = 0; i < HASHLEN; i++) {
			hash_equal = hash_equal && state.user_get_pwd_hash(user, i) == password_hash[i];
		}

		if (hash_equal) {
			return user;
		} else {
			return dcon::user_id{};
		}
	}
}

std::string retrieve_user_report_body(dcon::user_id user) {
	std::string result;
	result += "<h2>Balance</h2>";
	result += "<p>Savings: " + retrieve_balance(user) + ".</p>";

	result += "<h2>Ownership</h2>";
	bool owns_buildings = false;
	state.user_for_each_ownership(user, [&](dcon::ownership_id ownership){
		owns_buildings = true;
		auto building = state.ownership_get_owned(ownership);
		auto building_type = state.building_get_building_type(building);
		result += "<p>" + get_text(all_text, state.building_type_get_name(building_type)) + "</p>";
	});

	if (!owns_buildings) {
		result += "None";
	}

	return result;
}

std::string retrieve_building_report_body(dcon::building_id building) {

}
std::string retrieve_activity_report_body(dcon::activity_id activity) {

}

void simulation_update() {

}