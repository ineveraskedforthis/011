#include "constants.hpp"
#include "data.hpp"
#include "data_ids.hpp"
#include "unordered_dense.h"
#include <cmath>
#include <cstdint>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <vector>
#include "simulation.hpp"

static dcon::data_container state {};

std::mutex savings_mutex;

static constexpr uint8_t max_inputs = 8;
static constexpr uint8_t max_outputs = 8;
static constexpr uint8_t max_activities = 8;


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
		state.building_resize_stockpile(state.commodity_size());

		// buildings and activities

		auto refine_basic = state.create_activity();
		state.activity_set_name(refine_basic, new_text(all_text, "Refine basic ore"));
		state.activity_set_input(refine_basic, 0, ore_basic);
		state.activity_set_input_amount(refine_basic, 0, 1);
		state.activity_set_output(refine_basic, 0, material_basic);
		state.activity_set_output_amount(refine_basic, 0, 1);

		state.building_type_resize_activities(16);

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
std::string retrieve_building_type_list() {
	std::string result;
	result += "<ul>";
	state.for_each_building_type([&](auto btid){
		result += "<li><a href=\"/building_type?id=" + std::to_string(btid.index())+ "\">" + get_text(all_text, state.building_type_get_name(btid)) + "</a></li>";
	});
	result += "</ul>";

	return result;
}

std::string make_building_type_report(dcon::building_type_id btid) {
	if(!state.building_type_is_valid(btid)) {
		return "<html><head><title>Error</title></head><body>Invalid id</body></html>";
	}
	std::string result =
		"<html><head><title>"
		+ get_text(all_text, state.building_type_get_name(btid))
		+ "</title></head>";

	result += "<body>";

	result += "<header><h1>" + get_text(all_text, state.building_type_get_name(btid)) + "</h1></header>";

	result += "<h2>Activities</h2>";
	result += "<ul>";
	for (int i = 0; i < max_activities; i++) {
		auto activity = state.building_type_get_activities(btid, i);
		if (!activity) break;
		result += "<li><a href=\"/activity?id=" + std::to_string(activity.id.index())+ "\">"
		+ get_text(all_text, state.activity_get_name(activity))
		+ "</a></li>";
	}
	result += "</ul>";

	result += "</body></html>";

	return result;
}

template<typename T>
struct safe_ring_queue {
	std::mutex mtx;
	std::array<T, 256> items;
	uint8_t left;
	uint8_t right;

	bool push(T item) {
		mtx.lock();
		if (right + 1 == left) {
			return false;
		}
		items[right] = item;
		right++;
		mtx.unlock();
		return true;
	}
};

struct construction_request {
	dcon::user_id user;
	dcon::building_type_id building_type;
};

safe_ring_queue<construction_request> construction_requests_queue {};

bool request_new_building(dcon::user_id user, dcon::building_type_id building_type) {
	return construction_requests_queue.push({user, building_type});
}

void simulation_update() {
	construction_requests_queue.mtx.lock();
	for (uint8_t i = construction_requests_queue.left; i < construction_requests_queue.right; i++) {
		auto& item = construction_requests_queue.items[i];
		auto bid = state.create_building();
		state.building_set_building_type(bid, item.building_type);
		state.force_create_ownership(bid, item.user);
	}
	construction_requests_queue.mtx.unlock();

	state.for_each_building([&](dcon::building_id building){
		auto user = state.building_get_owner_from_ownership(building);
		auto activity = state.building_get_activity(building);
		bool inputs_ready = true;
		for (int i = 0; i < 8; i++) {
			auto input = state.activity_get_input(activity, i);
			if(!input) break;
			auto input_amount = state.activity_get_input_amount(activity, i);
			auto stockpile = state.building_get_stockpile(building, input);
			if (stockpile < input_amount) {
				inputs_ready = false;
				break;
			}
		}

		if (inputs_ready) {
			for (int i = 0; i < 8; i++) {
				auto input = state.activity_get_input(activity, i);
				if(!input) break;

				auto input_amount = state.activity_get_input_amount(activity, i);
				auto stockpile = state.building_get_stockpile(building, input);
				state.building_set_stockpile(building, input, stockpile - input_amount);
			}

			for (int i = 0; i < 8; i++) {
				auto output = state.activity_get_output(activity, i);
				if(!output) break;
				auto output_amount = state.activity_get_output_amount(activity, i);
				auto stockpile = state.building_get_stockpile(building, output);
				state.building_set_stockpile(building, output, stockpile + output_amount);
			}
		}
	});
}