#pragma once
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
	int cid;
	int64_t price;
	int64_t balance;
};

enum MHD_Result
send_page_from_memory (
	struct MHD_Connection *connection,
	const char* page,
	int status_code
);
MHD_Result POST_request_transfer(
	struct MHD_Connection * connection,
	connection_info_struct * con_info
);
MHD_Result respond_building_type(
	struct MHD_Connection * connection,
	int32_t id
);
MHD_Result respond_building(
	struct MHD_Connection * connection,
	int32_t id
);
MHD_Result POST_request_demand(
	struct MHD_Connection * connection,
	connection_info_struct * con_info
);
