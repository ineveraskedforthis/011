#include <cstddef>
#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <string>
#include <chrono>
#include <fstream>

#include "argon2.h"

static const std::string errorpage =  "<html><body>Error page.</body></html>";

static const std::string MAIN_STATIC_PAGE = "<html><head><title>Consent required.</title></head><body>We have to store your data to link your session cookie with in-game entity. We use data only for the in-game purposes. If you agree, pressing the  login button will generate a cookie and will allow you to interact with the game.<br><form action=\"/new_user\" method=\"post\"><label for=\"name\">Username:</label><input type=\"text\" name=\"name\" required /><br><label for=\"password\">Password</label><input type=\"password\" name=\"password\" required /><br><input type=\"submit\" value=\"Sign in\"/></form> </body></html>";

static const std::string SUCCESS_NEW_USER_PAGE = "<html><head><title>Consent granted.</title></head><body><br><form action=\"/report\" method=\"get\"><input type=\"submit\" value=\"Get back to report\"/></form> </body></html>";

std::string make_report(std::string name) {
	const auto now = std::chrono::system_clock::now();
	auto time_string = std::format("{:%Y-%m-%d %H:%M}", now);

	return "<html><head><title>Control panel.</title></head><body><h1>Welcome, " + name + "</h1><h2>Production report (per tick)</h2>20 units of Basic Ore<br>30 units of Basic Material<br> Report generated at " + time_string + " </body></html>";
}

MHD_Result print_out_key (
	void *cls,
	enum MHD_ValueKind kind,
	const char *key,
	const char *value
) {
	printf ("%s: %s\n", key, value);
	return MHD_YES;
}


static constexpr size_t HASHLEN = 32;
static constexpr size_t SALTLEN = 16;
static char salt[SALTLEN];
static constexpr size_t MAXNAMESIZE = 32;

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

	bool name_flag;
	bool password_flag;
	bool user_created;
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
		con_info->user_created = true;
		con_info->answerstring = make_report(con_info->name);
  		return MHD_NO;
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

	struct MHD_Response *response;
	enum MHD_Result ret;

	printf ("New %s request for %s using version %s\n", method, url, version);
	bool is_post = 0 == strcmp(method, "POST");
	bool is_get = 0 == strcmp(method, "GET");

	MHD_get_connection_values (
		connection,
		MHD_HEADER_KIND,
		&print_out_key,
		NULL
	);

	if (is_get) {
		if (0 != *upload_data_size)
			return MHD_NO; /* upload data in a GET!? */

		connection_info_struct *con_info = (connection_info_struct*) *req_cls;

		// auto page = make_main_page();
		if (con_info->user_created) {
			auto page = make_report(con_info->name);
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
			connection_info_struct *con_info = (connection_info_struct*) *req_cls;
			if (*upload_data_size != 0) {
				MHD_post_process (
					con_info->postprocessor,
					upload_data,
					*upload_data_size
				);
				*upload_data_size = 0;
				return MHD_YES;
			} else if (con_info->user_created) {
				response = MHD_create_response_from_buffer (
					strlen(con_info->answerstring.c_str()),
					(void*) con_info->answerstring.c_str(),
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