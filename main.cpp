#include <cstddef>
#include <cstdint>
#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <string>
#include <chrono>
#include <fstream>
#include <thread>

#include "argon2.h"

#include "data_ids.hpp"
#include "simulation.hpp"

#include "constants.hpp"
#include "unordered_dense.h"
#include "routing.hpp"
#include "html-gen.hpp"
#include "url.hpp"


static const std::string errorpage =  "<html><body>Error page.</body></html>";

int64_t b10_to_int(std::string in_value) {
	int64_t result = 0;
	for (int i = 0; i < in_value.size(); i++) {
		result = result * 10 + in_value[i] - '0';
	}
	return result;
}

struct common_params {
	int32_t id;
};

MHD_Result parse_common_key_value_pairs (
	void *cls,
	enum MHD_ValueKind kind,
	const char *key,
	const char *value
) {
	common_params * data = (common_params *) cls;
	if (0 == strcmp(key, "id")) {
		data->id = b10_to_int(value);
	}
	printf ("%s: %s\n", key, value);
	return MHD_YES;
}


static char salt[SALTLEN];



static enum MHD_Result
iterate_post_action (
	void *coninfo_cls,
	enum MHD_ValueKind kind,
	const char *key,
	const char *filename,
	const char *content_type,
	const char *transfer_encoding, const char *data,
	uint64_t off,
	size_t size
) {
	connection_info_struct*con_info = (connection_info_struct*) coninfo_cls;

	if (0 == strcmp (key, "id")) {
		con_info->id = b10_to_int(data);
	}

	if (0 == strcmp(key, "id2")) {
		con_info->id2 = b10_to_int(data);
	}

	if (0 == strcmp(key, "volume")) {
		con_info->volume = b10_to_int(data);
	}

	if (0 == strcmp(key, "price")) {
		con_info->price = b10_to_int(data);
	}

	if (0 == strcmp(key, "balance")) {
		con_info->balance = b10_to_int(data);
	}

	return MHD_YES;
}

static enum MHD_Result
iterate_post_new_user (
	void *coninfo_cls,
	enum MHD_ValueKind kind,
	const char *key,
	const char *filename,
	const char *content_type,
	const char *transfer_encoding, const char *data,
	uint64_t off,
	size_t size
) {
	connection_info_struct*con_info = (connection_info_struct*) coninfo_cls;

	if (0 == strcmp (key, "name")) {
		if (strlen(data) < MAXNAMESIZE) {
			con_info->name = data;
			con_info->name_flag = true;
		} else {
			return MHD_NO;
		}
	}
	if (0 == strcmp(key, "password")) {
		uint32_t t_cost = 2;
		uint32_t m_cost = (1<<16);
		uint32_t parallelism = 1;
		argon2i_hash_raw(
			t_cost,
			m_cost,
			parallelism,
			data,
			strlen(data),
			salt,
			SALTLEN,
			con_info->password_hash,
			HASHLEN
		);
		con_info->password_flag = true;
	}

	if (con_info->name_flag && con_info->password_flag) {
		con_info->user = create_or_get_user(con_info->name, con_info->password_hash);
		con_info->answerstring = make_report(con_info->user);
	}

	return MHD_YES;
}

void
request_completed (
	void *cls, struct MHD_Connection *connection,
	void **req_cls,
	enum MHD_RequestTerminationCode toe
) {
	struct connection_info_struct *con_info = (connection_info_struct*) *req_cls;
	if (NULL == con_info) return;

	if (con_info->connectiontype == connection_type::post) {
		MHD_destroy_post_processor (con_info->postprocessor);
	}

	*req_cls = NULL;

	delete con_info;
}

#define POSTBUFFERSIZE  512



#define SESSIONSIZE 64

static std::mutex session_mutex{};
static ankerl::unordered_dense::map<std::string, dcon::user_id> session_to_user;
static ankerl::unordered_dense::map<int32_t, std::string> user_to_session;

std::string generate_session(dcon::user_id uid) {
	std::random_device r;
	std::seed_seq seed2{r(), r(), r(), r(), r(), r(), r(), r()};
	std::mt19937 engine(seed2);
	std::uniform_int_distribution<> dist(
		0, 25
	);

	std::string session_string {};
	for(int i = 0; i < SESSIONSIZE; i++) {
		session_string += ('A' + dist(engine));
	}

	session_mutex.lock();
	auto it = user_to_session.find(uid.index());
	if (it == user_to_session.end()) {
		session_to_user.erase(it->second);
	}
	session_to_user[session_string] = uid;
	user_to_session[uid.index()] = session_string;
	session_mutex.unlock();

	return session_string;
}

static enum MHD_Result
ahc_echo(
	void * cls,
	struct MHD_Connection * connection,
	const char * url,
	const char * method,
	const char * version,
	const char * upload_data,
	size_t * upload_data_size,
	void ** req_cls
) {

	if (NULL == *req_cls) {
		// set up connection info
		auto con_info = new connection_info_struct;

		if (0 == strcmp (method, "POST")) {
			if (strcmp(url, url_gen::new_user().c_str()) == 0) {
				con_info->postprocessor = MHD_create_post_processor (
					connection,
					POSTBUFFERSIZE,
					iterate_post_new_user,
					(void*) con_info
				);
			} else {
				con_info->postprocessor = MHD_create_post_processor (
					connection,
					POSTBUFFERSIZE,
					iterate_post_action,
					(void*) con_info
				);
			}
			if (NULL == con_info->postprocessor) {
				delete con_info;
				return MHD_NO;
			}
			con_info->connectiontype = connection_type::post;
		} else if (0 == strcmp (method, "GET")) {
			con_info->connectiontype = connection_type::get;
		}

		*req_cls = (void*) con_info;
		return MHD_YES;
	}


	connection_info_struct *con_info = (connection_info_struct*) *req_cls;

	const char * detected_session = MHD_lookup_connection_value(
		connection,
		MHD_COOKIE_KIND,
		"SESSION"
	);

	printf("session detected\n");
	printf("%s", detected_session);

	if (detected_session) {
		std::string session_string = detected_session;
		auto iterator = session_to_user.find(session_string);
		if (iterator != session_to_user.end()){
			con_info->user = iterator->second;
		}
	}

	struct MHD_Response *response;
	enum MHD_Result ret;

	printf ("New %s request for %s using version %s\n", method, url, version);
	bool is_post = 0 == strcmp(method, "POST");
	bool is_get = 0 == strcmp(method, "GET");

	common_params common_keys{};

	const char * user_index = MHD_lookup_connection_value(
		connection,
		MHD_GET_ARGUMENT_KIND,
		"id"
	);
	if (user_index) {
		common_keys.id = b10_to_int(user_index);
	} else {
		common_keys.id = 0;
	}

	// MHD_get_connection_values (
	// 	connection,
	// 	MHD_HEADER_KIND,
	// 	&parse_common_key_value_pairs,
	// 	&common_keys
	// );

	if (is_get) {
		if (0 != *upload_data_size)
			return MHD_NO; /* upload data in a GET!? */
		if (con_info->user) {
			if (0 == strcmp(url, url_gen::building_type().c_str())) {
				return send_building_type_page(connection, con_info->current_page, common_keys.id);
			}
			if (0 == strcmp(url, url_gen::building().c_str())) {
				return send_building_page(connection, con_info->current_page, common_keys.id);
			}
			return send_main_page(connection, con_info->current_page, con_info->user);
		} else {
			auto page = login_page();
			response = MHD_create_response_from_buffer (
				strlen(page.c_str()),
				(void*) page.c_str(),
				MHD_RESPMEM_MUST_COPY
			);
			ret = MHD_queue_response(
				connection,
				MHD_HTTP_OK,
				response
			);
			MHD_destroy_response(response);
			return ret;
		}
	} else if (is_post) {
		if (*upload_data_size != 0) {
			MHD_post_process (
				con_info->postprocessor,
				upload_data,
				*upload_data_size
			);
			*upload_data_size = 0;
			return MHD_YES;
		}
		if (strcmp(url, url_gen::set_transfer().c_str()) == 0) {
			return POST_request_transfer(connection, con_info);
		} else if (strcmp(url, url_gen::new_demand().c_str()) == 0) {
			return POST_request_demand(connection, con_info);
		} else if (strcmp(url, url_gen::new_building().c_str()) == 0) {
			if(!con_info->user) {
				return send_page_from_memory(
					connection,
					errorpage.c_str(),
					MHD_HTTP_UNAUTHORIZED
				);
			}
			auto result = request_new_building(
				con_info->user,
				dcon::building_type_id{
					(dcon::building_type_id::value_base_t)con_info->id
				}
			);

			if (!result) {
				return send_page_from_memory(
					connection,
					errorpage.c_str(),
					MHD_HTTP_INSUFFICIENT_STORAGE
				);
			}

			auto page = make_building_type_report(
			dcon::building_type_id{
					(dcon::building_type_id::value_base_t)con_info->id
				}
			);
			response = MHD_create_response_from_buffer (
				strlen(page.c_str()),
				(void*) page.c_str(),
				MHD_RESPMEM_MUST_COPY
			);
			ret = MHD_queue_response(
				connection,
				MHD_HTTP_OK,
				response
			);
			MHD_destroy_response(response);
			return ret;
		}
		if (strcmp(url, url_gen::new_user().c_str()) == 0) {
			if (con_info->user) {
				auto session = generate_session(con_info->user);
				char key_value[SESSIONSIZE+16];
				snprintf(
					key_value, sizeof(key_value),
					"%s=%s",
					"SESSION",
					session.c_str()
				);
				// printf("new session %s\n", key_value);

				response = MHD_create_response_from_buffer (
					strlen(con_info->answerstring.c_str()),
					(void*) con_info->answerstring.c_str(),
					MHD_RESPMEM_PERSISTENT
				);

				if (!response) {
					return MHD_NO;
				}

				MHD_add_response_header(
					response,
					MHD_HTTP_HEADER_SET_COOKIE,
					key_value
				);

				ret = MHD_queue_response(
					connection,
					MHD_HTTP_OK,
					response
				);
				MHD_destroy_response(response);
				return ret;
			}  else {
				return send_page_from_memory(
					connection,
					errorpage.c_str(),
					MHD_HTTP_BAD_REQUEST
				);
			}

		}
		return MHD_NO;
	} else {
		response = MHD_create_response_from_buffer (
			strlen(errorpage.c_str()),
			(void*) errorpage.c_str(),
			MHD_RESPMEM_PERSISTENT
		);
		ret = MHD_queue_response(
			connection,
			MHD_HTTP_OK,
			response
		);
		MHD_destroy_response(response);
		return ret;
	}
}

int
main(
	int argc,
	char ** argv
) {
	init_simulation();
	float timer;
	auto now = std::chrono::system_clock::now();
	auto then = std::chrono::system_clock::now();

	std::thread game_loop ([&]() {
		while (true) {
			then = std::chrono::system_clock::now();
			auto duration = then - now;
			auto milliseconds =
				std::chrono::duration_cast<std::chrono::milliseconds>(duration);
			if (milliseconds.count() > 500) {
				simulation_update();
				now = then;
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	});

	game_loop.detach();

	FILE * salt_container = fopen(".salt", "r");
	if( salt_container ) {
		fread(salt, 1, 10, salt_container);
	} else {
		printf("No salt file");
		return 1;
	}

	struct MHD_Daemon * d;

	if (argc != 3)
	{
		printf(
			"%s URL_PREFIX PORT\n",
			argv[0]
		);
		return 1;
	}

	url_gen::set_base_prefix(argv[1]);

	d = MHD_start_daemon(
		MHD_USE_EPOLL | MHD_USE_INTERNAL_POLLING_THREAD,
		atoi(argv[2]),
		NULL,
		NULL,
		&ahc_echo,
		(void**)NULL,
		MHD_OPTION_NOTIFY_COMPLETED,
		&request_completed,
		NULL,
		MHD_OPTION_END
	);

	if (NULL == d)
		return 1;
	(void) getc (stdin);
	MHD_stop_daemon(d);
	return 0;
}