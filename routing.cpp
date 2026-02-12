#include "routing.hpp"
#include "microhttpd.h"
#include "data_ids.hpp"
#include "simulation.hpp"

static const std::string errorpage =  "<html><body>Error page.</body></html>";
static const std::string successpage =  "<html><body>Success.</body></html>";

enum MHD_Result
send_page_from_memory (
	struct MHD_Connection *connection,
	const char* page,
	int status_code
) {
	enum MHD_Result ret;
	struct MHD_Response *response;
	response = MHD_create_response_from_buffer (
		strlen (page),
		(void*) page,
		MHD_RESPMEM_PERSISTENT
	);
	if (!response) return MHD_NO;
	ret = MHD_queue_response (connection, status_code, response);
	MHD_destroy_response (response);
	return ret;
}

static enum MHD_Result
send_page_copy (
	struct MHD_Connection *connection,
	const char* page,
	int status_code
) {
	enum MHD_Result ret;
	struct MHD_Response *response;
	response = MHD_create_response_from_buffer (
		strlen (page),
		(void*) page,
		MHD_RESPMEM_MUST_COPY
	);
	if (!response) return MHD_NO;
	ret = MHD_queue_response (connection, status_code, response);
	MHD_destroy_response (response);
	return ret;
}

MHD_Result not_logged_in(struct MHD_Connection * connection) {
	return send_page_from_memory(
		connection,
		errorpage.c_str(),
		MHD_HTTP_UNAUTHORIZED
	);
}

MHD_Result lack_of_storage(struct MHD_Connection * connection) {
	return send_page_from_memory(
		connection,
		errorpage.c_str(),
		MHD_HTTP_INSUFFICIENT_STORAGE
	);
}

MHD_Result POST_request_demand(
	struct MHD_Connection * connection,
	connection_info_struct * con_info
) {
	if(!con_info->user) return not_logged_in(connection);
	auto result = request_demand(
		con_info->user,
		dcon::commodity_id {dcon::commodity_id::value_base_t (con_info->cid)},
		con_info->price,
		con_info->volume
	);
	if (!result) lack_of_storage(connection);
	return send_page_copy(connection, successpage.c_str(), MHD_HTTP_OK);
}

MHD_Result POST_request_transfer(
	struct MHD_Connection * connection,
	connection_info_struct * con_info
) {
	if(!con_info->user) return not_logged_in(connection);
	auto result = request_transfer(
		con_info->user,
		dcon::storage_id {dcon::storage_id::value_base_t(con_info->id)},
		dcon::storage_id {dcon::storage_id::value_base_t(con_info->id2)},
		dcon::commodity_id {dcon::commodity_id::value_base_t (con_info->id3)},
		con_info->volume
	);
	if (!result) lack_of_storage(connection);
	return send_page_copy(connection, successpage.c_str(), MHD_HTTP_OK);
}

MHD_Result respond_building_type(
	struct MHD_Connection * connection,
	int32_t id
) {
	auto page = make_building_type_report(
		dcon::building_type_id{
			(dcon::building_type_id::value_base_t)id
		}
	);
	return send_page_copy(connection, page.c_str(), MHD_HTTP_OK);
}

MHD_Result respond_building(
	struct MHD_Connection * connection,
	int32_t id
) {
	auto page = make_building_report(
		dcon::building_id{
			(dcon::building_id::value_base_t)id
		}
	);
	return send_page_copy(connection, page.c_str(), MHD_HTTP_OK);
}