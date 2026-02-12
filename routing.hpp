#include "constants.hpp"
#include <cstdint>
#include <string>
#include "data_ids.hpp"
#include "microhttpd.h"

enum class connection_type {
	post, get
};

struct connection_info_struct
{
	connection_type connectiontype;
	std::string name;
	uint8_t password_hash[HASHLEN];
	std::string answerstring;
	struct MHD_PostProcessor *postprocessor;

	bool name_flag = false;
	bool password_flag = false;
	dcon::user_id user;
	int id;
	int id2;
	int id3;
	int volume;
};

MHD_Result respond_building_type(
	struct MHD_Connection * connection,
	int32_t id
);
MHD_Result respond_building(
	struct MHD_Connection * connection,
	int32_t id
);