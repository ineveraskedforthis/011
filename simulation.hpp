#include "data_ids.hpp"
#include <string>
#include "constants.hpp"

void init_simulation();
dcon::user_id create_or_get_user(std::string name, uint8_t password_hash[HASHLEN]);
std::string retrieve_user_name(dcon::user_id user);
std::string retrieve_user_report_body(dcon::user_id user);
std::string retrieve_building_report_body(dcon::building_id building);
std::string retrieve_activity_report_body(dcon::activity_id activity);
std::string retrieve_building_type_list();
std::string make_building_type_report(dcon::building_type_id btid);