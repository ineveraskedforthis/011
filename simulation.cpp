#include "constants.hpp"
#include "data.hpp"
#include "data_ids.hpp"
#include "unordered_dense.h"
#include <cmath>
#include <cstdint>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <vector>
#include "simulation.hpp"

static dcon::data_container state {};

std::mutex buildings_mutex;

std::mutex savings_mutex;

static constexpr uint8_t max_inputs = 8;
static constexpr uint8_t max_outputs = 8;
static constexpr uint8_t max_activities = 8;

static constexpr __uint128_t building_permission_cost = 100;


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
		state.activity_resize_input(max_inputs);
		state.activity_resize_input_amount(max_inputs);
		state.activity_resize_output(max_outputs);
		state.activity_resize_output_amount(max_outputs);
		state.building_type_resize_construction_amount(max_inputs);
		state.building_type_resize_construction(max_inputs);
		state.building_resize_stockpile(state.commodity_size());

		// buildings and activities

		auto refine_basic = state.create_activity();
		state.activity_set_name(refine_basic, new_text(all_text, "Refine basic ore"));
		state.activity_set_input(refine_basic, 0, ore_basic);
		state.activity_set_input_amount(refine_basic, 0, 1);
		state.activity_set_output(refine_basic, 0, material_basic);
		state.activity_set_output_amount(refine_basic, 0, 1);

		state.building_type_resize_activities(max_activities);

		auto refinery = state.create_building_type();
		state.building_type_set_name(refinery, new_text(all_text, "Refinery"));
		state.building_type_set_activities(refinery, 0, refine_basic);
		state.building_type_set_construction(refinery, 0, ore_basic);
		state.building_type_set_construction_amount(refinery, 0, 50);
	}
}


std::string retrieve_balance(dcon::user_id user) {
	savings_mutex.lock();
	auto savings = state.user_get_wealth(user);
	savings_mutex.unlock();

	std::string representation {};
	for (int i = 0; i < 129 * std::log10(2); i++) {
		auto digit =  (uint8_t) (savings % 10);
		savings = savings / 10;
		if (i == 10) {
			representation = "." + representation;
		}
		representation = std::to_string(digit) + representation;
		if (savings == 0 && i >= 10) {
			break;
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
		state.user_set_wealth(user, 1000);
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
	result += "<p>Savings: " + retrieve_balance(user) + "</p>";

	result += "<h2>Stockpiles</h2>";
	result += "<ul>";
	state.for_each_commodity([&](auto cid){
		result += "<li>";
		result += get_text(all_text, state.commodity_get_name(cid));
		result += " ";
		result += std::to_string(state.user_get_storage(user, cid));
		result += "</li>";
	});
	result += "</ul>";

	result += "<h2>Ownership</h2>";
	bool owns_buildings = false;
	result += "<ul>";
	state.user_for_each_ownership(user, [&](dcon::ownership_id ownership){
		owns_buildings = true;
		auto building = state.ownership_get_owned(ownership);
		auto building_type = state.building_get_building_type(building);
		auto activity = state.building_get_activity(building);
		std::string activity_string = "";
		if (activity)
			activity_string =
			"("
			+ get_text(all_text, state.activity_get_name(activity))
			+ ")";
		result += "<li><a href=\"/building?id=" + std::to_string(building.index()) + "\">"
			+ get_text(all_text, state.building_type_get_name(building_type))
			+ activity_string + "</a></li>";
	});
	result += "</ul>";

	if (!owns_buildings) {
		result += "None";
	}

	return result;
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

std::string navigation_header() {
	return "<header><h1>Navigation</h1><ul><li>Go <a href=\"/\">back to main page</a></li></ul></header>";
}

std::string footer() {
	const auto now = std::chrono::system_clock::now();
	auto time_string = std::format("{:%Y-%m-%d %H:%M}", now);
	return  "<footer> Report generated at <time>" + time_string + "</time> </footer>";
}

std::string make_building_report(dcon::building_id bid) {
	if(!state.building_is_valid(bid)) {
		return "<html><head><title>Error</title></head><body>Invalid id</body></html>";
	}

	auto btid = state.building_get_building_type(bid);
	auto activity = state.building_get_activity(bid);
	auto activity_string = activity ? get_text(all_text, state.activity_get_name(activity)) : "";
	auto building_name_string = get_text(all_text, state.building_type_get_name(btid));
	std::string result =
		"<html><head><title>"
		+ building_name_string
		+ ": "
		+ activity_string
		+ "</title></head>";


	result += "<body>";

	result += navigation_header();

	result += "<h1>" + building_name_string + "</h1>";
	result += "Current action: " + (activity ? activity_string : "None");

	auto in_construction = !state.building_get_constructed(bid);
	if (in_construction) {
		result += "<h2>Under construction</h2>";
		result += "<ul>";
		auto total = 0;
		auto total_current = 0;
		for (int i = 0; i < max_inputs; i++) {
			auto required_commodity = state.building_type_get_construction(btid, i);
			if (!required_commodity) break;
			auto required = state.building_type_get_construction_amount(btid, i);
			auto current = state.building_get_stockpile(bid, required_commodity);
			total += required;
			total_current += current;
			result += "<li>";
			result += "<label for=progress-" + std::to_string(i) + ">";
			result += get_text(all_text, state.commodity_get_name(required_commodity));
			result += " (" + std::to_string(current) + " out of " + std::to_string(required) + ")";
			result += "</label><br>";
			result += "<progress id=\"progress-" + std::to_string(i) +  "\" max=\"" + std::to_string(required) + "\" value=\""+  std::to_string(current) + "\"></progress>";
			result += "</li>";
		}
		result += "</ul>";
		result += "<label for=progress-total>Total construction progress</label><br>";
		result += "<progress id=\"progress-total\" max=\"" + std::to_string(total) + "\" value=\""+  std::to_string(total_current) + "\"></progress>";
	} else {
		result += "<h2>Operation control</h2>";

		result += "<form action=\"/edit_building\" method=\"post\">";
		result += "<input name=\"id\" type=\"hidden\" value=\"" + std::to_string(bid.index()) + "\"><br>";
		result += "<label for=\"activity_select\">Select activity of the building</label>";
		result += "<select name=\"id2\" id=\"activity_select\">";

		for (int i = 0; i < max_activities; i++) {
			auto activity = state.building_type_get_activities(btid, i);
			if (!activity) break;
			result += "<option value=\"" + std::to_string(i) + "\">";
			result += get_text(all_text, state.activity_get_name(activity));
			result += "</option>";
		}

		result += "</select>";

		result += "<p><button type=\"submit\">Request construction</button></p>";
		result += "</form>";
	}
	// result += "<h2>Control<>"

	result += "</body>";

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

	result += "<body>" + navigation_header();

	result += "<h1>" + get_text(all_text, state.building_type_get_name(btid)) + "</h1>";


	result += "<h2>Construction</h2>";

	result += "<form action=\"/build?id=" + std::to_string(btid.index()) + "/\" method=\"post\"><p><button type=\"submit\">Request construction</button></p></form>";

	result += "<h2>Potential activities</h2>";
	result += "<ul>";
	for (int i = 0; i < max_activities; i++) {
		auto activity = state.building_type_get_activities(btid, i);
		if (!activity) break;
		result += "<li><a href=\"/activity?id=" + std::to_string(activity.id.index())+ "\">"
		+ get_text(all_text, state.activity_get_name(activity))
		+ "</a></li>";
	}
	result += "</ul>";

	result += footer();
	result +=  "</body></html>";
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
			mtx.unlock();
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
	std::lock_guard<std::mutex> lock {buildings_mutex};

	if (!state.building_type_is_valid(building_type)) return false;

	auto count = 0;
	state.user_for_each_ownership(user, [&](auto o){count++;});

	if (count > 1000) return false;
	if (state.building_size() > 10000) return false;

	return construction_requests_queue.push({user, building_type});
}

struct building_settings_request {
	dcon::user_id user;
	dcon::building_id bid;
	dcon::activity_id aid;
};
safe_ring_queue<building_settings_request> building_settings_queue {};
bool request_settings_change(dcon::user_id user, dcon::building_id building, int i) {
	std::lock_guard<std::mutex> lock {buildings_mutex};

	if (i < 0) return false;
	if (i >= max_activities) return false;
	if (!state.building_is_valid(building)) return false;
	if (!state.user_is_valid(user)) return false;
	auto ownership = state.get_ownership_by_ownership_pair(building, user);
	if (!ownership) return false;
	auto btid = state.building_get_building_type(building);
	auto activity = state.building_type_get_activities(btid, i);
	if (!activity) return false;
	return building_settings_queue.push({user, building, activity});
}


void simulation_update() {
	construction_requests_queue.mtx.lock();
	for (uint8_t i = construction_requests_queue.left; i != construction_requests_queue.right; i++) {
		std::lock_guard<std::mutex> lock {buildings_mutex};
		auto& item = construction_requests_queue.items[i];
		auto w  = state.user_get_wealth(item.user);
		if (w < building_permission_cost) {
			continue;
		}

		auto bid = state.create_building();
		state.building_set_building_type(bid, item.building_type);
		state.force_create_ownership(bid, item.user);
		state.building_set_constructed(bid, false);
		savings_mutex.lock();
		state.user_set_wealth(item.user, w  - building_permission_cost);
		savings_mutex.unlock();
	}
	construction_requests_queue.left = construction_requests_queue.right;
	construction_requests_queue.mtx.unlock();

	building_settings_queue.mtx.lock();
	for (uint8_t i = building_settings_queue.left; i != building_settings_queue.right; i++) {
		std::lock_guard<std::mutex> lock {buildings_mutex};
		auto& item = building_settings_queue.items[i];
		state.building_set_activity(item.bid, item.aid);
	}
	building_settings_queue.left = building_settings_queue.right;
	building_settings_queue.mtx.unlock();

	state.for_each_building([&](dcon::building_id building){
		std::lock_guard<std::mutex> lock {buildings_mutex};
		if (!state.building_get_constructed(building)) {
			return;
		}
		auto user = state.building_get_owner_from_ownership(building);
		auto activity = state.building_get_activity(building);
		bool inputs_ready = true;
		for (int i = 0; i < max_inputs; i++) {
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
			for (int i = 0; i < max_inputs; i++) {
				auto input = state.activity_get_input(activity, i);
				if(!input) break;

				auto input_amount = state.activity_get_input_amount(activity, i);
				auto stockpile = state.building_get_stockpile(building, input);
				state.building_set_stockpile(building, input, stockpile - input_amount);
			}

			for (int i = 0; i < max_outputs; i++) {
				auto output = state.activity_get_output(activity, i);
				if(!output) break;
				auto output_amount = state.activity_get_output_amount(activity, i);
				auto stockpile = state.building_get_stockpile(building, output);
				state.building_set_stockpile(building, output, stockpile + output_amount);
			}
		}
	});
}