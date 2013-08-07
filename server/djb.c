#include <libfutil/httpsrv.h>
#include <libfutil/list.h>

#include "rendezvous.h"
#include "shared.h"

#define DJB_WORKERS	8
#define DJB_HOST	"localhost"
#define DJB_PORT	6543

typedef struct {
	hnode_t			node;		/* List node */
	httpsrv_client_t	*hcl;		/* Client for this request */
} djb_req_t;

/* New, unforwarded queries (awaiting 'pull') */
hlist_t lst_proxy_new;

/* Outstanding queries (answer to a 'pull', awaiting 'push') */
hlist_t lst_proxy_out;

/* Requiring transfering of body ('push') */
hlist_t lst_proxy_body;

/* Requests that want a 'pull', waiting for a 'proxy_new' entry */
hlist_t lst_api_pull;

typedef struct {
	char	httpcode[128];
	char	seqno[32];
	char	setcookie[8192];
	char	cookie[8192];
} djb_headers_t;

#define DJBH(h) offsetof(djb_headers_t, h), sizeof (((djb_headers_t *)NULL)->h)

misc_map_t djb_headers[] = {
	{ "DJB-HTTPCode",	DJBH(httpcode)	},
	{ "DJB-SeqNo",		DJBH(seqno)	},
	{ "DJB-Set-Cookie",	DJBH(setcookie)	},
	{ "Cookie",		DJBH(cookie)	},
	{ NULL,			0, 0		}
};

void
djb_html_top(httpsrv_client_t *hcl);
void
djb_html_top(httpsrv_client_t *hcl) {

	/* HTML header */
	conn_put(&hcl->conn,
		"<!doctype html>\n"
		"<html lang=\"en\">\n"
		"<head>\n"
		"<title>JumpBox</title>\n"
		"<link rel=\"icon\" type=\"image/png\" "
		"href=\"http://www.farsightsecurity.com/favicon.ico\">\n"
		"<style type=\"text/css\">\n"
		"label\n"
		"{\n"
		"	float		: left;\n"
		"	width		: 120px;\n"
		"	font-weight	: bold;\n"
		"}\n"
		"\n"
		"input, textarea\n"
		"{\n"
		"	margin-bottom	: 5px;\n"
		"}\n"
		"\n"
		"textarea\n"
		"{\n"
		"	width		: 250px;\n"
		"	height		: 150px;\n"
		"}\n"
		"\n"
		"form input[type=submit]\n"
		"{\n"
		"	margin-left	: 120px;\n"
		"	margin-top	: 5px;\n"
		"	width		: 90px;\n"
		"}\n"
		"form br\n"
		"{\n"
		"	clear		: left;\n"
		"}\n"
		"\n"
		"th\n"
		"{\n"
		"	background-color: #eeeeee;\n"
		"}\n"
		"\n"
		".header, .footer\n"
		"{\n"
		"	background-color: #eeeeee;\n"
		"	width		: 100%;\n"
		"}\n"
		"\n"
		".footer\n"
		"{\n"
		"	margin-top	: 5em;\n"
		"}\n"
		"\n"
		"a\n"
		"{\n"
		"	background-color: transparent;\n"
		"	text-decoration	: none;\n"
		"	border-bottom	: 1px solid #bbbbbb;\n"
		"	color		: black;\n"
		"}\n"
		"\n"
		"a:hover\n"
		"{\n"
		"	color		: #000000;\n"
		"	text-decoration	: none;\n"
		"	background-color: #d1dfd5;\n"
		"	border-bottom	: 1px solid #a8bdae;\n"
		"}\n"
		"\n"
		".right\n"
		"{\n"
		"	text-align	: right\n"
		"}\n"
		"</style>\n"
		"</head>\n"
		"<body>");
}

void
djb_html_tail(httpsrv_client_t *hcl);
void
djb_html_tail(httpsrv_client_t *hcl) {

	/* HTML header */
	conn_put(&hcl->conn,
		"</body>"
		"</html>");
}

void
djb_pull(httpsrv_client_t *hcl);
void
djb_pull(httpsrv_client_t *hcl){
	djb_req_t *ar;

	/* Proxy request - add it to the requester list */
	ar = mcalloc(sizeof *ar, "djb_req_t *");
	if (!ar) {
		djb_error(hcl, 500, "Out of memory");
		return;
	}

	/* Fill in the details */
	ar->hcl = hcl;

	/* Stop reading from the socket for now */
	httpsrv_silence(hcl);

	/*
	 * Add this request to the queue
	 * The manager will divide the work
	 */
	list_addtail_l(&lst_api_pull, &ar->node);

	logline(log_DEBUG_, "Request added to api_pull");
}

djb_req_t *
djb_find_req(hlist_t *lst, uint64_t id, uint64_t reqid);
djb_req_t *
djb_find_req(hlist_t *lst, uint64_t id, uint64_t reqid) {
	djb_req_t	*r, *rn, *pr = NULL;

	/* Find this Request-ID in our outstanding proxy requests */
	list_lock(lst);
	list_for(lst, r, rn, djb_req_t *) {
		if (r->hcl->id != id ||
		    r->hcl->reqid != reqid) {
			continue;
		}

		/* Gotcha */
		pr = r;

		/* Remove it from this list */
		list_remove(lst, &pr->node);
		break;
	}
	list_unlock(lst);

	return (pr);
}

/* Pull request, look up the SeqNo and handle it */
void
djb_push(httpsrv_client_t *hcl, djb_headers_t *dh);
void
djb_push(httpsrv_client_t *hcl, djb_headers_t *dh) {
	djb_req_t	*pr;
	uint64_t	id, reqid;

	/* We require a DJB-HTTPCode */
	if (strlen(dh->httpcode) == 0) {
		djb_error(hcl, 504, "Missing DJB-HTTPCode");
		return;
	}

	/* Convert the Request-ID */
	if (sscanf(dh->seqno, "%09" PRIx64 "%09" PRIx64, &id, &reqid) != 2) {
		djb_error(hcl, 504, "Missing or malformed DJB-SeqNo");
		return;
	}

	/* Find the request */
	pr = djb_find_req(&lst_proxy_out, id, reqid);

	if (!pr) {
		djb_error(hcl, 404, "No such request outstanding");
		return;
	}

	/* Resume reading/writing from the sockets */
	httpsrv_speak(pr->hcl);

	/* We got an answer, send back what we have already */
	conn_addheaderf(&pr->hcl->conn, "HTTP/1.1 %s OK\r\n", dh->httpcode);

	/* We need to translate the cookie header back */
	if (strlen(dh->cookie) > 0) {
		conn_addheaderf(&pr->hcl->conn, "DJB-Cookie: %s\r\n",
				dh->cookie);
	}

	/* Add all the headers we received */
	conn_addheader(&pr->hcl->conn, buf_buffer(&hcl->the_headers));

	if (hcl->headers.content_length == 0) {
		/* This request is done (after flushing) */
		httpsrv_done(pr->hcl);

		/* Release it */
		free(pr);
		return;
	}

	/* Still need to forward some data */

	/*
	 * Put this on the proxy_body list
	 * As we turned speaking on, this will cause
	 * the rest of the connection to be read
	 * and thus the body to be copied over
	 */
	list_addtail_l(&lst_proxy_body, &pr->node);

	/* Make it forward the body from hcl to pr */
	hcl->bodyfwd = pr->hcl;
	hcl->bodyfwdlen = hcl->headers.content_length;

	/* httpsrv does not need to forward this anymore */
	hcl->headers.content_length = 0;
	/* XXX: abstract the above few lines into a httpsrv_forward(hcl, hcl) */

	/* The Content-Length header is already included in all the headers */
	conn_add_contentlen(&pr->hcl->conn, false);

	logline(log_DEBUG_,
		"Forwarding body from hcl%" PRIu64 " to hcl%" PRIu64,
		pr->hcl->id, hcl->id);
}

void
djb_bodyfwddone(httpsrv_client_t *hcl, void *user);
void
djb_bodyfwddone(httpsrv_client_t *hcl, void *user) {
	djb_headers_t	*dh = (djb_headers_t *)user;
	uint64_t	id, reqid;
	djb_req_t	*pr;

	/* Convert the Request-ID */
	if (sscanf(dh->seqno, "%09" PRIx64 "%09" PRIx64, &id, &reqid) != 2) {
		djb_error(hcl, 504, "Missing or malformed DJB-SeqNo");
		return;
	}

	/* Find the request */
	pr = djb_find_req(&lst_proxy_body, id, reqid);

	if (!pr) {
		/* Cannot happen in theory */
		logline(log_ERR_, "Could not find %" PRIu64 ":%" PRIu64,
			id, reqid);
		assert(false);
		return;
	}

	logline(log_DEBUG_,
		"Done forwarding body from %" PRIu64 " to %" PRIu64,
		hcl->id, pr->hcl->id);

	/* Send a content-length again if there is one */
	conn_add_contentlen(&pr->hcl->conn, true);

	/* The forwarded request has finished */
	httpsrv_done(pr->hcl);

	/* Send back a 200 OK as we forwarded it */

	/* HTTP okay */
	conn_addheaderf(&hcl->conn, "HTTP/1.1 200 OK\r\n");

	conn_addheaderf(&hcl->conn, "Content-Type: text/html\r\n");

	/* Body is just JumpBox (Content-Length is arranged by conn) */
	conn_printf(&hcl->conn, "JumpBox\r\n");

	/* This request is done */
	httpsrv_done(hcl);
}

void
djb_accept(httpsrv_client_t *hcl, void UNUSED *user);
void
djb_accept(httpsrv_client_t *hcl, void UNUSED *user) {

	djb_headers_t *dh;

	logline(log_DEBUG_, "%p", (void *)hcl);

	dh = mcalloc(sizeof *dh, "djb_headers_t *");
	if (!dh) {
		djb_error(hcl, 500, "Out of memory");
		return;
	}

	httpsrv_set_userdata(hcl, dh);
}

void
djb_header(httpsrv_client_t UNUSED *hcl, void *user, char *line);
void
djb_header(httpsrv_client_t UNUSED *hcl, void *user, char *line) {
	djb_headers_t *dh = (djb_headers_t *)user;

	misc_map(line, djb_headers, (char *)dh);
}

static void
djb_status_list(httpsrv_client_t *hcl, hlist_t *lst,
		const char *title, const char *desc);
static void
djb_status_list(httpsrv_client_t *hcl, hlist_t *lst,
		const char *title, const char *desc) {
	djb_req_t	*r, *rn;
	unsigned int	cnt = 0;

	conn_printf(&hcl->conn,
		"<h1>List: %s</h1>\n"
		"<p>\n"
		"%s\n"
		"</p>\n",
		title,
		desc);

	list_lock(lst);
	list_for(lst, r, rn, djb_req_t *) {

		if (cnt == 0) {
			conn_put(&hcl->conn,
				"<table>\n"
				"<tr>\n"
				"<th>ID</th>\n"
				"<th>Host</th>\n"
				"<th>Request</th>\n"
				"</tr>\n");
		}

		conn_printf(&hcl->conn,
			"<tr>"
			"<td>%" PRIu64 "</td>"
			"<td>%s</td>"
			"<td>%s</td>"
			"</tr>\n",
			r->hcl->id,
			r->hcl->headers.hostname,
			r->hcl->the_request);
		cnt++;
	}
	list_unlock(lst);

	if (cnt == 0) {
		conn_put(&hcl->conn,
			"No outstanding request");
	} else {
		conn_put(&hcl->conn,
			"</table>\n");
	}
}

static void
djb_status_threads_cb(	void		*cbdata,
			uint64_t	tid,
			const char	*starttime,
			uint64_t	runningsecs,
			const char	*description,
			bool		thisthread,
			const char	*state,
			uint64_t	served);
static void
djb_status_threads_cb(  void		*cbdata,
			uint64_t	tid,
			const char	*starttime,
			uint64_t	runningsecs,
			const char	*description,
			bool		thisthread,
			const char	*state,
			uint64_t	served)
{
	httpsrv_client_t *hcl = (httpsrv_client_t *)cbdata;

	conn_printf(&hcl->conn,
		"<tr>"
		"<td>tr%" PRIu64 "</td>"
		"<td>%s</td>"
		"<td>%" PRIu64 "</td>"
		"<td>%s</td>"
		"<td>%s</td>"
		"<td>%s</td>"
		"<td>%" PRIu64 "</td>"
		"</tr>\n",
		tid,
		starttime,
		runningsecs,
		description,
		yesno(thisthread),
		state,
		served);
}

static void
djb_status_threads(httpsrv_client_t *hcl);
static void
djb_status_threads(httpsrv_client_t *hcl) {
	unsigned int cnt;

	conn_put(&hcl->conn,
		"<h1>Threads</h1>\n"
		"<p>\n"
		"Threads running inside Jumpbox.\n"
		"</p>\n"
		"<table>\n"
		"<tr>\n"
		"<th>TID</th>\n"
		"<th>Start Time</th>\n"
		"<th>Running seconds</th>\n"
		"<th>Description</th>\n"
		"<th>This Thread</th>\n"
		"<th>State</th>\n"
		"<th>Served</th>\n"
		"</tr>\n");

	cnt = thread_list(djb_status_threads_cb, hcl);

	conn_put(&hcl->conn,
			"</table>\n");

	if (cnt == 0) {
		conn_put(&hcl->conn,
				"Odd, no threads where found running!?");
	}
}

static void
djb_status_version(httpsrv_client_t *hcl);
static void
djb_status_version(httpsrv_client_t *hcl) {
	conn_put(&hcl->conn,
		"<h2>Jumpbox Information</h2>\n"
		"<p>\n"
		"Following details are available about this Jumpbox (djb).\n"
		"</p>\n"
		"\n"
		"<table>\n"
		"<tr><th>Version:</th><td>" STR(PROJECT_VERSION) "</td></tr>"
		"<tr><th>Buildtime:</th><td>" STR(PROJECT_BUILDTIME) "</td></tr>"
		"<tr><th>GIT hash:</th><td>" STR(PROJECT_GIT) "</td></tr>"
		"</table>\n");
}

void
djb_status(httpsrv_client_t *hcl);
void
djb_status(httpsrv_client_t *hcl) {
	conn_addheaderf(&hcl->conn, "HTTP/1.1 200 OK\r\n");
	conn_addheaderf(&hcl->conn, "Content-Type: text/html\r\n");

	/* Body is just JumpBox (Content-Length is arranged by conn) */
	djb_html_top(hcl);

	djb_status_version(hcl);
	djb_status_threads(hcl);

	djb_status_list(hcl, &lst_proxy_new,
			"Proxy New",
			"New unforwarded requests");

	djb_status_list(hcl, &lst_proxy_out,
			"Proxy Out",
			"Outstanding queries "
			"(answer to pull, waiting for a push)");

	djb_status_list(hcl, &lst_proxy_body,
			"Proxy Body",
			"Requiring transfering of body (push)");

	djb_status_list(hcl, &lst_api_pull,
			"API Pull",
			"Requests that want a pull, "
			"waiting for proxy_new entry");

	djb_html_tail(hcl);

	/* This request is done */
	httpsrv_done(hcl);
}

void
djb_handle_api(httpsrv_client_t *hcl, djb_headers_t *dh);
void
djb_handle_api(httpsrv_client_t *hcl, djb_headers_t *dh) {

	/* A DJB API request */
	logline(log_DEBUG_, "DJB API request: %s", hcl->headers.uri);

	/* Our API URIs */
	if (strcasecmp(hcl->headers.uri, "/pull/") == 0) {
		djb_pull(hcl);
		return;

	} else if (strcasecmp(hcl->headers.uri, "/push/") == 0) {
		djb_push(hcl, dh);
		return;

	} else if (strcasecmp(hcl->headers.uri, "/shutdown/") == 0) {
		djb_error(hcl, 200, "Shutting down");
		thread_stop_running();
		return;

	} else if (strcasecmp(hcl->headers.uri, "/acs/") == 0) {
                djb_error(hcl, 500, "Not implemented yet");
                return;

	} else if (strncasecmp(hcl->headers.uri, "/rendezvous/", strlen("/rendezvous/")) == 0) {
                rendezvous(hcl);
                return;

	} else if (strcasecmp(hcl->headers.uri, "/") == 0) {
		djb_status(hcl);
                return;
	}

	/* Not a valid API request */
	djb_error(hcl, 500, "Not a DJB API request");
	return;
}

void
djb_handle_proxy(httpsrv_client_t *hcl);
void
djb_handle_proxy(httpsrv_client_t *hcl) {
	djb_req_t	*pr;

	/* Proxy request - add it to the requester list */
	pr = mcalloc(sizeof *pr, "djb_req_t *");
	if (!pr) {
		djb_error(hcl, 500, "Out of memory");
		return;
	}

	/* Fill in the details */
	pr->hcl = hcl;

	/* For now, silence a bit */
	httpsrv_silence(hcl);

	/*
	 * Add this request to the queue
	 * The manager will divide the work
	 */
	list_addtail_l(&lst_proxy_new, &pr->node);

	logline(log_DEBUG_, "Request added to proxy_new");
}

void
djb_handle(httpsrv_client_t *hcl, void *user);
void
djb_handle(httpsrv_client_t *hcl, void *user) {
	djb_headers_t	*dh = (djb_headers_t *)user;

	logline(log_DEBUG_, "hostname: %s uri: %s", hcl->headers.hostname, hcl->headers.uri);

	/* Parse the request */
	if (!httpsrv_parse_request(hcl)) {
		/* function has added error already */
		return;
	}

	/* Is this a DJB API or a proxy request? */
	if (	strcmp(hcl->headers.hostname, "127.0.0.1:6543"	) == 0 ||
		strcmp(hcl->headers.hostname, "[::1]:6543"	) == 0 ||
		strcmp(hcl->headers.hostname, "localhost:6543"	) == 0) {

		/* API request */
		djb_handle_api(hcl, dh);
		return;
	}

	/* Proxied request */
	djb_handle_proxy(hcl);
}

void
djb_freereadbody(httpsrv_client_t *hcl) {
	logline(log_DEBUG_, "hcl = %p, readbody = %p", (void *)hcl, (void *)hcl->readbody);

	if (hcl->readbody) {
		mfree(hcl->readbody, hcl->readbodylen + hcl->readbodyoff, "HTTPBODY");
		hcl->readbody = NULL;
		hcl->readbodylen = 0;
		hcl->readbodyoff = 0;
	}
}

void
djb_done(httpsrv_client_t *hcl, void *user);
void
djb_done(httpsrv_client_t *hcl, void *user) {
	djb_headers_t  *dh = (djb_headers_t *)user;

	logline(log_DEBUG_, "%p", (void *)hcl);

	djb_freereadbody(hcl);

	memzero(dh, sizeof *dh);
}

void
djb_close(httpsrv_client_t *hcl, void UNUSED *user);
void
djb_close(httpsrv_client_t *hcl, void UNUSED *user) {

	logline(log_DEBUG_, "%p", (void *)hcl);
}

static void
djb_pass_pollers(void);
static void
djb_pass_pollers(void) {
	const char	*e = NULL;
	djb_req_t	*pr, *ar;
	djb_headers_t	*dh;

	logline(log_DEBUG_, "...");

	/* Forcing the hostname to something else than the requestor wants? */
	e = getenv("DJB_FORCED_HOSTNAME");

	while (thread_keep_running()) {
		logline(log_DEBUG_, "waiting for proxy request");

		/* Get a new proxy request */
		thread_setstate(thread_state_io_next);
		pr = (djb_req_t *)list_getnext(&lst_proxy_new);
		thread_setstate(thread_state_running);

		if (pr == NULL) {
			if (thread_keep_running()) {
				logline(log_ERR_,
					" get_next(proxy_new) failed...");
			}
			break;
		}

		logline(log_DEBUG_, "got request, getting poller");

		/* We got a request, pass it to a puller */
		thread_setstate(thread_state_io_next);
		ar = (djb_req_t *)list_getnext(&lst_api_pull);
		thread_setstate(thread_state_running);

		if (ar == NULL) {
			if (thread_keep_running()) {
				logline(log_ERR_,
					" get_next(api_pull) failed...");
			}
			break;
		}

		logline(log_DEBUG_, "got request, got puller");

		/* Resume reading from the sockets */
		httpsrv_speak(pr->hcl);
		httpsrv_speak(ar->hcl);

		/* pr's headers */
		dh = (djb_headers_t *)pr->hcl->user;

		/* HTTP okay */
		conn_addheaderf(&ar->hcl->conn, "HTTP/1.1 200 OK\r\n");

		/* DJB headers */
		conn_addheaderf(&ar->hcl->conn, "DJB-URI: http://%s%s%s%s\r\n",
				e ? e : pr->hcl->headers.hostname,
				pr->hcl->headers.uri,
				(strlen(pr->hcl->headers.args) > 0) ? "?" : "",
				pr->hcl->headers.args);

		conn_addheaderf(&ar->hcl->conn, "DJB-Method: %s\r\n",
				httpsrv_methodname(pr->hcl->method));

		conn_addheaderf(&ar->hcl->conn, "DJB-SeqNo: %09" PRIx64 "%09" PRIx64 "\r\n",
				pr->hcl->id, pr->hcl->reqid);

		/* We need to translate the cookie header back */
		if (strlen(dh->setcookie) > 0) {
			conn_addheaderf(&ar->hcl->conn, "DJB-Set-Cookie: %s\r\n",
					dh->setcookie);
		}

		if (strlen(dh->cookie) > 0) {
			conn_addheaderf(&ar->hcl->conn, "DJB-Cookie: %s\r\n",
					dh->cookie);
		}

		conn_addheaderf(&ar->hcl->conn, "Content-Type: text/html\r\n");

		/* Body is just JumpBox (Content-Length is arranged by conn) */
		conn_printf(&ar->hcl->conn, "JumpBox\r\n");

		/* This request is done */
		httpsrv_done(ar->hcl);

		/* Release it */
		free(ar);

		/* Put this on the proxy_out list now it is being handled */
		list_addtail_l(&lst_proxy_out, &pr->node);

		/* Served another one */
		thread_serve();
	}

	logline(log_DEBUG_, "exit");
}

static int
djb_run(void);
static int
djb_run(void) {
	httpsrv_t	*hs = NULL;
	int		ret = 0;

	/* Create out DGW structure */
	hs = (httpsrv_t *)mcalloc(sizeof *hs, "httpsrv_t");
	if (hs == NULL) {
		logline(log_CRIT_, "No memory for HTTP Server");
		return (-1);
	}

	while (true) {
		/* Initialize a HTTP Server */
		if (!httpsrv_init(hs, NULL,
				  djb_accept,
				  djb_header,
				  djb_handle,
				  djb_bodyfwddone,
				  djb_done,
				  djb_close)) {
			logline(log_CRIT_, "Could not initialize HTTP server");
			ret = -1;
			break;
		}

		/* Fire up an HTTP server */
		if (!httpsrv_start(hs, DJB_HOST, DJB_PORT, DJB_WORKERS)) {
			logline(log_CRIT_, "HTTP Server failed");
			ret = -1;
			break;
		}

		/* Nothing more to set up */
		break;
	}

	/* Handle pollers in the main thread */
	djb_pass_pollers();

	/* Cleanup time as the mainloop ended */
	logline(log_DEBUG_, "Cleanup Time...(main ret = %d)", ret);

	/* Make sure that our threads are done */
	thread_stopall(false);

	/* Clean up the http object */
	if (hs)
		httpsrv_exit(hs);

	return (ret);
}

static void
usage(const char *progname);
static void
usage(const char *progname) {
	fprintf(stderr, "Usage: %s [<command>]\n", progname);
	fprintf(stderr, "\n");
	fprintf(stderr, "run        = run the server\n");
	fprintf(stderr, "daemonize [<pidfilename>] [<logfilename>]\n");
	fprintf(stderr, "           = daemonize the server into\n");
	fprintf(stderr, "             the background\n");
}

int
main(int argc, const char *argv[]) {
	int ret = 0;

	/* Setup logging */
	log_setup("djb", stderr);

	if (!thread_init())
		return (-1);

	list_init(&lst_proxy_new);
	list_init(&lst_proxy_out);
	list_init(&lst_proxy_body);
	list_init(&lst_api_pull);

	if (argc < 2) {
		usage(argv[0]);
		ret = -1;
	} else if (strcasecmp(argv[1], "daemonize") == 0 &&
			(argc >= 2 || argc <= 4)) {
		if (argc == 4) {
			if (!log_set(argv[3]))
				ret = -1;
		}

		if (ret != -1) {
			ret = futil_daemonize(argc >= 1 ? argv[2] : NULL);
			if (ret > 0)
				ret = djb_run();
		}
	} else if (strcasecmp(argv[1], "run") == 0 && argc == 2) {
		ret = djb_run();
	} else {
		usage(argv[0]);
		ret = -1;
	}

	logline(log_DEBUG_, "It's all over... (%d)", ret);

	thread_exit();

	/* All okay */
	return (ret);
}
