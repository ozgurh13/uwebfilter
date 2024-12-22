#include <string.h>

#include <curl/curl.h>
#include <json-c/json.h>

#include "config.h"
#include "logger.h"
#include "uwebfilterlog.h"



static const char* authheader_get(void);

static bool
logs_send_to_cloud_server(const char* payload)
{
	CURLcode ret;
	CURL *hnd;
	struct curl_slist *slist1;

	const char* authheader = authheader_get();

	slist1 = NULL;
	slist1 = curl_slist_append(slist1, authheader);
	slist1 = curl_slist_append(slist1, "content-type: application/json");

	const char* cloudlogging_addr = config_get_cloudlogging_addr();

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, cloudlogging_addr);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/8.10.1");
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
	curl_easy_setopt(hnd, CURLOPT_FTP_SKIP_PASV_IP, 1L);
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYHOST, 0L);

	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);

	curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, payload);

	ret = curl_easy_perform(hnd);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_slist_free_all(slist1);
	slist1 = NULL;

	return ret == CURLE_OK;
}



static json_object*
uwebfilterlog_to_json_object(uwebfilterlog_t *uwebfilterlog)
{
	json_object *root = json_object_new_object();

	json_object_object_add(root, "srcip",
		json_object_new_string(uwebfilterlog->srcip));
	json_object_object_add(root, "dstip",
		json_object_new_string(uwebfilterlog->dstip));
	json_object_object_add(root, "srcport",
		json_object_new_int(uwebfilterlog->srcport));
	json_object_object_add(root, "dstport",
		json_object_new_int(uwebfilterlog->dstport));
	if (uwebfilterlog->proto != NULL) {
		json_object_object_add(root, "proto",
			json_object_new_string(uwebfilterlog->proto));
	}
	json_object_object_add(root, "domain",
		json_object_new_string(uwebfilterlog->domain));
	json_object_object_add(root, "path",
		json_object_new_string(uwebfilterlog->path));
	if (uwebfilterlog->action != NULL) {
		json_object_object_add(root, "action",
			json_object_new_string(uwebfilterlog->action));
	}
	json_object_object_add(root, "category",
		json_object_new_int(uwebfilterlog->category));
	json_object_object_add(root, "application",
		json_object_new_int(uwebfilterlog->application));
	json_object_object_add(root, "blocktype",
		json_object_new_int(uwebfilterlog->blocktype));
	json_object_object_add(root, "logtime",
		json_object_new_int64(uwebfilterlog->logtime));

	return root;
}


#define    LOGBUFFER_MAXSIZE              4096
#define    LOGBUFFER_INTERVAL               30
static struct {
	json_object* array;
	time_t last_send;
} logbuffer;

static void __attribute__((constructor))
logbuffer_init(void)
{
	logbuffer.array = json_object_new_array();
	logbuffer.last_send = 0;
}

void
_uwebfilterlog_sendto_cloud(uwebfilterlog_t *uwebfilterlog)
{
	time_t now = uwebfilterlog->logtime;

	json_object *log = uwebfilterlog_to_json_object(uwebfilterlog);
	json_object_array_add(logbuffer.array, log);

	if ( (logbuffer.last_send + LOGBUFFER_INTERVAL) < now
	  || json_object_array_length(logbuffer.array) >= LOGBUFFER_MAXSIZE
	   ) {
		logger_debug("uwebfilterlog: sending logs to cloud server");

		json_object *root = json_object_new_object();

		json_object_object_add(root, "version", json_object_new_int(1));
		json_object_object_add(root, "logs", logbuffer.array);

		const char* payload = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
		if (!logs_send_to_cloud_server(payload)) {
			logger_error("uwebfilterlog_sendto_cloud: failed to send");
		}

		json_object_put(root);

		logbuffer.array = json_object_new_array();
		logbuffer.last_send = now;
	}
}

static const char* credentials = NULL;
static void
credentials_init(void)
{
	json_object *root = json_object_new_object();

	json_object_object_add(root, "username",
		json_object_new_string(config_get_cloudlogging_user())
	);
	json_object_object_add(root, "password",
		json_object_new_string(config_get_cloudlogging_pass())
	);

	credentials = strdup(
		json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN)
	);

	json_object_put(root);
}


typedef struct {
	json_tokener *tok;
	json_object *obj;
} curl_json_buffer_t;

static void
curl_json_buffer_cleanup(curl_json_buffer_t *b)
{
	if (b->obj != NULL) {
		json_object_put(b->obj);
	}
	json_tokener_free(b->tok);
}

static size_t
curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
	const size_t realsize = size * nmemb;
	curl_json_buffer_t *b = userp;
	b->obj = json_tokener_parse_ex(b->tok, contents, realsize);

	return realsize;
}


static bool
jwttoken_get_cloud(curl_json_buffer_t *buffer)
{
	CURLcode ret;
	CURL *hnd;

	if (credentials == NULL) {
		credentials_init();
	}

	struct curl_slist *slist = NULL;
	slist = curl_slist_append(slist, "content-type: application/json");

	const char* cloudlogging_auth = config_get_cloudlogging_auth();

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, cloudlogging_auth);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/8.10.1");
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
	curl_easy_setopt(hnd, CURLOPT_FTP_SKIP_PASV_IP, 1L);
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYHOST, 0L);

	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist);

	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, credentials);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, buffer);

	ret = curl_easy_perform(hnd);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_slist_free_all(slist);
	slist = NULL;

	return ret == CURLE_OK;
}


#define   AUTHHEADER_LENGTH               8191
#define   AUTHHEADER_TTL         (6 * 60 * 60)     /*  6 hours  */
static char authheader[AUTHHEADER_LENGTH + 1] = {0};
static time_t authheader_timestamp = 0;

static int
jwttoken_get(void)
{
	__attribute__((cleanup(curl_json_buffer_cleanup)))
	curl_json_buffer_t buffer = {
		.tok = json_tokener_new(),
		.obj = NULL
	};

	const bool success = jwttoken_get_cloud(&buffer);
	if (!success || buffer.obj == NULL) {
		return -1;
	}

	const char* token = json_object_get_string(
		json_object_object_get(buffer.obj, "token")
	);

	return snprintf(authheader, AUTHHEADER_LENGTH, "Authorization: Bearer %s", token);
}


static const char*
authheader_get(void)
{
	time_t now = time(NULL);
	if ((authheader_timestamp + AUTHHEADER_TTL) < now) {
		memset(authheader, 0, AUTHHEADER_LENGTH);
		if (jwttoken_get() < 0) {
			logger_error("jwttoken_get: failed to get JWT token");
			return authheader;
		}
		authheader_timestamp = now;
	}

	return authheader;
}
