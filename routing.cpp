#include "routing.hpp"
#include "microhttpd.h"
#include "data_ids.hpp"
#include "simulation.hpp"
#include "html-gen.hpp"
#include "url.hpp"
#include <format>

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

MHD_Result invalid_value(struct MHD_Connection * connection) {
	return send_page_from_memory(
		connection,
		errorpage.c_str(),
		MHD_HTTP_BAD_REQUEST
	);
}

std::string inline wrap_into_anchor(std::string url, std::string text) {
	return "<a href=\"" + url + "\">" + text + "</a>";
}

static enum MHD_Result
send_link_to_current_page (
	struct MHD_Connection *connection,
	connection_info_struct * con_info,
	int status_code
) {
	std::string result = "<html><head><title>Request accepted</title></head><body><h1>Request accepted</h1>Meanwhile, you can ";
	auto& page = con_info->current_page;
	switch (page.page) {
	case page_type::main:
		result += wrap_into_anchor(url_gen::main_mage(), "return back to the main menu");
		// return send_main_page(connection, page, con_info->user);
		break;
	case page_type::building:
		// result += wrap_into_anchor(url_gen::building(page.id.value()), "return back to the building page") + " or ";
		result += wrap_into_anchor(url_gen::main_mage(), "go to the main menu");
		break;
	case page_type::building_type:
		// result += wrap_into_anchor(url_gen::building_type(page.id.value()), "return back to the building page") + " or ";
		result += wrap_into_anchor(url_gen::main_mage(), "go to the main menu");
		break;
	}
	return send_page_copy(connection, result.c_str(), status_code);
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
	return send_link_to_current_page(connection, con_info, MHD_HTTP_ACCEPTED);
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
	return send_link_to_current_page(connection, con_info, MHD_HTTP_ACCEPTED);
}

MHD_Result send_main_page(
	struct MHD_Connection * connection,
	page_ref& current_page,
	dcon::user_id user
) {
	current_page.page = page_type::main;
	auto page = make_report(user);
	return send_page_copy(connection, page.c_str(), MHD_HTTP_OK);
}

MHD_Result send_building_type_page(
	struct MHD_Connection * connection,
	page_ref& current_page,
	int32_t id
) {
	current_page.page = page_type::building;
	current_page.id = id;
	auto page = make_building_type_report(
		dcon::building_type_id{
			(dcon::building_type_id::value_base_t)id
		}
	);
	return send_page_copy(connection, page.c_str(), MHD_HTTP_OK);
}

MHD_Result send_building_page(
	struct MHD_Connection * connection,
	page_ref& current_page,
	int32_t id
) {
	current_page.page = page_type::building;
	current_page.id = id;
	auto page = make_building_report(
		dcon::building_id{
			(dcon::building_id::value_base_t)id
		}
	);
	return send_page_copy(connection, page.c_str(), MHD_HTTP_OK);
}