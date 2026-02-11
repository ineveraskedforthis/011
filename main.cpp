#include <cstddef>
#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <string>
#include <chrono>
#include <fstream>

#include "argon2.h"

#include "data_ids.hpp"
#include "simulation.hpp"

#include "constants.hpp"
#include "unordered_dense.h"

static const std::string errorpage =  "<html><body>Error page.</body></html>";

static const std::string MAIN_STATIC_PAGE = "<html><head><title>Consent required.</title></head><body>We have to store your data to link your session cookie with in-game entity. We use data only for the in-game purposes. If you agree, pressing the  login button will generate a cookie and will allow you to interact with the game.<form action=\"/new_user\" method=\"post\"><label for=\"name\">Username:</label><input type=\"text\" name=\"name\" required /><label for=\"password\">Password</label><input type=\"password\" name=\"password\" required /><input type=\"submit\" value=\"Sign in\"/></form> </body></html>";

static const std::string SUCCESS_NEW_USER_PAGE = "<html><head><title>Consent granted</title></head><body><br><form action=\"/report\" method=\"get\"><input type=\"submit\" value=\"Get back to report\"/></form> </body></html>";

std::string make_report(dcon::user_id user) {
	if(!user) {
		return "<html><head><title>Error</title></head><body>Invalid credentials</body></html>";
	}

	const auto now = std::chrono::system_clock::now();
	auto time_string = std::format("{:%Y-%m-%d %H:%M}", now);

	std::string result = "<html><head><title>Control panel</title></head><body>";
	result +=  "<h1>Welcome, " + retrieve_user_name(user) + "</h1>";
	result += retrieve_user_report_body(user);
	result += "<h2>Available buildings</h2>" + retrieve_building_type_list();
	return result + "<footer> Report generated at <time>" + time_string + "</time> </footer></body></html>";
}

int b10_to_int(std::string in_value) {
	auto result = 0;
	for (int i = 0; i < in_value.size(); i++) {
		result = result * 10 + in_value[i];
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
};



static enum MHD_Result
iterate_post (
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

static enum MHD_Result
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

#define SESSIONSIZE 64

static std::mutex session_mutex{};
static ankerl::unordered_dense::map<std::string, dcon::user_id> session_to_user;

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
	session_to_user[session_string] = uid;
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
			con_info->postprocessor = MHD_create_post_processor (
				connection,
				POSTBUFFERSIZE,
				iterate_post,
				(void*) con_info
			);
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

	const char * session = MHD_lookup_connection_value(
		connection,
		MHD_COOKIE_KIND,
		"SESSION"
	);

	printf("session detected\n");
	printf("%s", session);

	if (session) {
		std::string session_string = session;
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
		MHD_COOKIE_KIND,
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


		// auto page = make_main_page();
		if (con_info->user) {
			if (0 == strcmp(url, "/building_type")) {
				auto page = make_building_type_report(
					dcon::building_type_id{
						(dcon::building_type_id::value_base_t)common_keys.id
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
			auto page = make_report(con_info->user);
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
		} else {
			response = MHD_create_response_from_buffer (
				strlen(MAIN_STATIC_PAGE.c_str()),
				(void*) MAIN_STATIC_PAGE.c_str(),
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
	} else if (is_post) {
		if (strcmp(url, "/new_user") == 0) {
			if (*upload_data_size != 0) {
				MHD_post_process (
					con_info->postprocessor,
					upload_data,
					*upload_data_size
				);
				*upload_data_size = 0;
				return MHD_YES;
			}
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

	FILE * salt_container = fopen(".salt", "r");
	if( salt_container ) {
		fread(salt, 1, 10, salt_container);
	} else {
		printf("No salt file");
		return 1;
	}

	struct MHD_Daemon * d;

	if (argc != 2)
	{
		printf("%s PORT\n",
		argv[0]);
		return 1;
	}
	d = MHD_start_daemon(
		MHD_USE_EPOLL | MHD_USE_INTERNAL_POLLING_THREAD,
		atoi(argv[1]),
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