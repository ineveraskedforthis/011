#include "data_ids.hpp"
#include <string>
#include "constants.hpp"

void init_simulation();
void simulation_update();
dcon::user_id create_or_get_user(std::string name, uint8_t password_hash[HASHLEN]);

bool request_new_building(dcon::user_id user, dcon::building_type_id building_type);
bool request_settings_change(dcon::user_id user, dcon::building_id building, int i);
bool request_transfer(dcon::user_id user, dcon::storage_id s,  dcon::storage_id t, dcon::commodity_id cid, int volume);

std::string retrieve_user_name(dcon::user_id user);
std::string retrieve_user_report_body(dcon::user_id user);
std::string retrieve_building_report_body(dcon::building_id building);
std::string retrieve_activity_report_body(dcon::activity_id activity);
std::string retrieve_building_type_list();
std::string make_building_type_report(dcon::building_type_id btid);
std::string make_building_report(dcon::building_id bid);