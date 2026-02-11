#include "microhttpd.h"
#include "data_ids.hpp"
#include "simulation.hpp"

MHD_Result respond_building_type(
	struct MHD_Connection * connection,
	int32_t id
) {
	auto page = make_building_type_report(
		dcon::building_type_id{
			(dcon::building_type_id::value_base_t)id
		}
	);
	auto response = MHD_create_response_from_buffer (
		strlen(page.c_str()),
		(void*) page.c_str(),
		MHD_RESPMEM_MUST_COPY
	);
	auto ret = MHD_queue_response(
		connection,
		MHD_HTTP_OK,
		response
	);
	MHD_destroy_response(response);
	return ret;
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
	auto response = MHD_create_response_from_buffer (
		strlen(page.c_str()),
		(void*) page.c_str(),
		MHD_RESPMEM_MUST_COPY
	);
	auto ret = MHD_queue_response(
		connection,
		MHD_HTTP_OK,
		response
	);
	MHD_destroy_response(response);
	return ret;
}