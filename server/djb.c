#include <libfutil/httpsrv.h>
#include <libfutil/list.h>

#include "djb.h"
#include "rendezvous.h"

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

	/* Server -> Client */
	{ "DJB-Set-Cookie",	DJBH(setcookie)	},

	/* Client -> Server */
	{ "Cookie",		DJBH(cookie)	},
	{ NULL,			0, 0		}
};

void
djb_html_css(httpsrv_client_t *hcl);
void
djb_html_css(httpsrv_client_t *hcl) {
	/* HTML header */
	conn_put(&hcl->conn,
		"/* JumpBox CSS */\n"
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
		"}\n");
}

void
djb_html_top(httpsrv_client_t *hcl, void UNUSED *user);
void
djb_html_top(httpsrv_client_t *hcl, void UNUSED *user) {

	/* HTML header */
	conn_put(&hcl->conn,
		"<!doctype html>\n"
		"<html lang=\"en\">\n"
		"<head>\n"
		"<title>DEFIANCE JumpBox</title>\n"
		"<link rel=\"icon\" type=\"image/png\" "
		"href=\"http://www.farsightsecurity.com/favicon.ico\">\n"
		"<link rel=\"stylesheet\" type=\"text/css\" href=\"/djb.css\" />\n"
		"</head>\n"
		"<body>\n"
		"<div class=\"header\">\n"
		"SAFER DEFIANCE :: JumpBox\n"
		"</div>\n");
}

void
djb_html_tail(httpsrv_client_t *hcl, void UNUSED *user);
void
djb_html_tail(httpsrv_client_t *hcl, void UNUSED *user) {

	/* HTML header */
	conn_put(&hcl->conn,
		"<div class=\"footer\">\n"
		"SAFER DEFIANCE :: Defiance JumpBox (djb) by "
		"<a href=\"http://www.farsightsecurity.com\">"
		"Farsight Security, Inc"
		"</a>.\n"
		"</div>\n"
		"</body>\n"
		"</html>\n");
}

void
djb_httpanswer(httpsrv_client_t *hcl, unsigned int code, const char *msg);
void
djb_httpanswer(httpsrv_client_t *hcl, unsigned int code, const char *msg) {
	conn_addheaderf(&hcl->conn, "HTTP/1.1 %u %s", code, msg);

#if 0
	/* Note it is us, very helpful while debugging pcap traces */
	conn_addheaderf(&hcl->conn, "X-JumpBox: Yes");
#endif
}

void
djb_error(httpsrv_client_t *hcl, unsigned int code, const char *msg) {
	djb_httpanswer(hcl, code, msg);

	djb_html_top(hcl, NULL);
	conn_printf(&hcl->conn,
		    "<h1>Error %u</h1>\n"
		    "<p>\n"
		    "%s\n"
		    "</p>\n",
		    code, msg);
	djb_html_tail(hcl, NULL);

	httpsrv_done(hcl);
}

/* API-Pull, requests a new Proxy-request */
djb_req_t *
djb_find_hcl(hlist_t *lst, httpsrv_client_t *hcl);
djb_req_t *
djb_find_hcl(hlist_t *lst, httpsrv_client_t *hcl) {
	djb_req_t	*r, *rn, *pr = NULL;

	fassert(lst != NULL);
	fassert(hcl != NULL);

	/* Find this HCL in our outstanding proxy requests */
	list_lock(lst);
	list_for(lst, r, rn, djb_req_t *) {
		if (r->hcl != hcl) {
			continue;
		}

		/* Gotcha */
		pr = r;

		/* Remove it from this list */
		list_remove(lst, &pr->node);
		break;
	}
	list_unlock(lst);

	if (pr != NULL) {
		logline(log_DEBUG_, HCL_ID, pr->hcl->id);
	} else {
		logline(log_WARNING_,
			"No such HCL (" HCL_ID ") found!?",
			hcl->id);
	}

	return (pr);
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

	if (pr != NULL) {
		logline(log_DEBUG_,
			"%" PRIu64 ":%" PRIu64 " = " HCL_ID,
			id, reqid, pr->hcl->id);
	} else {
		logline(log_WARNING_,
			"No such HCL (%" PRIu64 ":%" PRIu64 " found!?",
			id, reqid);
	}

	return (pr);
}

djb_req_t *
djb_find_req_dh(httpsrv_client_t *hcl, hlist_t *lst, djb_headers_t *dh);
djb_req_t *
djb_find_req_dh(httpsrv_client_t *hcl, hlist_t *lst, djb_headers_t *dh) {
	djb_req_t	*pr;
	uint64_t	id, reqid;

	/* We require a DJB-HTTPCode */
	if (strlen(dh->httpcode) == 0) {
		djb_error(hcl, 504, "Missing DJB-HTTPCode");
		return NULL;
	}

	/* Convert the Request-ID */
	if (sscanf(dh->seqno, "%09" PRIx64 "%09" PRIx64, &id, &reqid) != 2) {
		djb_error(hcl, 504, "Missing or malformed DJB-SeqNo");
		return NULL;
	}

	pr = djb_find_req(lst, id, reqid);
	if (!pr) {
		djb_error(hcl, 404, "No such request outstanding");
	}

	return (pr);
}

bool
djb_pull(httpsrv_client_t *hcl);
bool
djb_pull(httpsrv_client_t *hcl){
	djb_req_t *ar;

	logline(log_DEBUG_, HCL_ID, hcl->id);

	/* Proxy request - add it to the requester list */
	ar = mcalloc(sizeof *ar, "djb_req_t *");
	if (!ar) {
		djb_error(hcl, 500, "Out of memory");
		return (true);
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

	logline(log_DEBUG_,
		HCL_ID " Request added to api_pull",
		hcl->id);

	/* No need to read from it further for the moment */
	return (true);
}


/* Push request, answer to a pull request */
bool
djb_push(httpsrv_client_t *hcl, djb_headers_t *dh);
bool
djb_push(httpsrv_client_t *hcl, djb_headers_t *dh) {
	djb_req_t *pr;

	logline(log_DEBUG_, HCL_ID, hcl->id);

	/* Find the request */
	pr = djb_find_req_dh(hcl, &lst_proxy_out, dh);
	if (pr == NULL) {
		/* Can happen if the request timed out etc */
		return (true);
	}

	/* Connections might close before the answer is returned */
	if (!conn_is_valid(&pr->hcl->conn)) {
		logline(log_DEBUG_,
			HCL_ID " " CONN_ID " closed",
			hcl->id, conn_id(&pr->hcl->conn));
		return (true);
	}

	/* Resume reading/writing from the sockets */
	httpsrv_speak(pr->hcl);

	/* We got an answer, send back what we have already */
	djb_httpanswer(pr->hcl, atoi(dh->httpcode), "OK");

	/* Server to Client */
	if (strlen(dh->setcookie) > 0) {
		conn_addheaderf(&pr->hcl->conn, "Set-Cookie: %s",
				dh->setcookie);
	}

	/* Client to Server */
	if (strlen(dh->cookie) > 0) {
		conn_addheaderf(&pr->hcl->conn, "DJB-Cookie: %s",
				dh->cookie);
	}

	/* Add all the headers we received */
	/* XXX: We should scrub DJB-SeqNo */
	conn_addheaders(&pr->hcl->conn, buf_buffer(&hcl->the_headers));

	if (hcl->headers.content_length == 0) {
		/* This request is done (after flushing) */
		httpsrv_done(pr->hcl);

		/* Release it */
		free(pr);
		return (false);
	}

	/* Still need to forward the body as there is length */

	/*
	 * Put this on the proxy_body list
	 * As we turned speaking on, this will cause
	 * the rest of the connection to be read
	 * and thus the body to be copied over
	 */
	list_addtail_l(&lst_proxy_body, &pr->node);

	/* Forward the body from hcl to pr */
	httpsrv_forward(hcl, pr->hcl);

	/* The Content-Length header is already included in all the headers */
	conn_add_contentlen(&pr->hcl->conn, false);

	logline(log_DEBUG_,
		"Forwarding body from " HCL_ID " to " HCL_ID,
		pr->hcl->id, hcl->id);

	/* No need to read from it further for the moment */
	return (true);
}

/* hcl == the client proxy request, pr->hcl = pull API request */
void
djb_bodyfwd_done(httpsrv_client_t *hcl, void UNUSED *user);
void
djb_bodyfwd_done(httpsrv_client_t *hcl, void UNUSED *user) {
	djb_req_t *pr;

	/* Find the request */
	pr = djb_find_hcl(&lst_proxy_body, hcl->bodyfwd);

	/* Should always be there */
	fassert(pr);

	logline(log_DEBUG_,
		"Done forwarding body from " HCL_ID " to " HCL_ID,
		hcl->id, pr->hcl->id);

	/* Send a content-length again if there is one */
	conn_add_contentlen(&pr->hcl->conn, true);

	/* Was this a push? Then we answer that it is okay */
	if (strcasecmp(hcl->headers.uri, "/push/") == 0) {
		/* Send back a 200 OK as we proxied it */
		logline(log_DEBUG_, "API push, done with it");

		/* HTTP okay */
		djb_httpanswer(hcl, 200, "OK");
		conn_addheaderf(&hcl->conn, "Content-Type: text/html");

		/* A message as a body (Content-Length is arranged by conn) */
		conn_printf(&hcl->conn, "Push Body Forward successful\r\n");

		/* The forwarded request is done */
		httpsrv_done(hcl);

		/* The calling request is done too */
		httpsrv_done(pr->hcl);

		/* All done */
		free(pr);
	} else {
		/* This was a proxy-POST, thus add it back to process queue */
		logline(log_DEBUG_, "proxy-POST, adding back to queue");

		/* Silence the proxy-POST for the time being */
		/* httpsrv_silence(hcl); */

		/* The forwarded request is done */
		httpsrv_done(pr->hcl);

		/* Done with this leg */
		free(pr);

		/* Served another one */
		thread_serve();
	}

	/* Done for this request is handled by caller: httpsrv_handle_http() */
	logline(log_DEBUG_, "end");
}

void
djb_accept(httpsrv_client_t *hcl, void UNUSED *user);
void
djb_accept(httpsrv_client_t *hcl, void UNUSED *user) {

	djb_headers_t *dh;

	logline(log_DEBUG_, HCL_ID, hcl->id);

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
		"%s.\n"
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
			"<td>" HCL_IDn "</td>"
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
			"No outstanding requests.");
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
		"Threads running inside JumpBox.\n"
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
		"<h2>JumpBox Information</h2>\n"
		"<p>\n"
		"Following details are available about this JumpBox (djb).\n"
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
	djb_httpanswer(hcl, 200, "OK");
	conn_addheaderf(&hcl->conn, "Content-Type: text/html");

	/* Body is just JumpBox (Content-Length is arranged by conn) */
	djb_html_top(hcl, NULL);

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

	djb_html_tail(hcl, NULL);

	/* This request is done */
	httpsrv_done(hcl);
}

bool
djb_handle_api(httpsrv_client_t *hcl, djb_headers_t *dh);
bool
djb_handle_api(httpsrv_client_t *hcl, djb_headers_t *dh) {

	/* A DJB API request */
	logline(log_DEBUG_,
		HCL_ID " DJB API request: %s",
		hcl->id, hcl->headers.uri);

	/* Our API URIs */
	if (strcasecmp(hcl->headers.uri, "/pull/") == 0) {
		return djb_pull(hcl);

	} else if (strcasecmp(hcl->headers.uri, "/push/") == 0) {
		return djb_push(hcl, dh);

	} else if (strcasecmp(hcl->headers.uri, "/shutdown/") == 0) {
		djb_error(hcl, 200, "Shutting down");
		thread_stop_running();
		return (true);

	} else if (strcasecmp(hcl->headers.uri, "/acs/") == 0) {
		djb_error(hcl, 500, "Not implemented yet");
		return (false);

	} else if (strncasecmp(hcl->headers.uri, "/rendezvous/", strlen("/rendezvous/")) == 0) {
		rendezvous(hcl);
		return (false);

	} else if (strcasecmp(hcl->headers.uri, "/") == 0) {
		djb_status(hcl);
		return (false);

	} else if (strcasecmp(hcl->headers.uri, "/djb.css") == 0) {
		djb_httpanswer(hcl, 200, "OK");
		conn_addheaderf(&hcl->conn, "Content-Type: text/css");

		djb_html_css(hcl);

		httpsrv_done(hcl);
		return (false);
	}

	/* Not a valid API request */
	djb_error(hcl, 404, "No such DJB API request");
	return (false);
}

bool
djb_handle_proxy(httpsrv_client_t *hcl);
bool
djb_handle_proxy(httpsrv_client_t *hcl) {
	djb_req_t	*pr;

	/* Proxy request - add it to the requester list */
	pr = mcalloc(sizeof *pr, "djb_req_t *");
	if (!pr) {
		djb_error(hcl, 500, "Out of memory");
		return (true);
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

	logline(log_DEBUG_,
		HCL_ID " Request added to proxy_new",
		hcl->id);

	/* No need to read more for now */
	return (true);
}

bool
djb_handle(httpsrv_client_t *hcl, void *user);
bool
djb_handle(httpsrv_client_t *hcl, void *user) {
	djb_headers_t	*dh = (djb_headers_t *)user;
	bool		done;

	logline(log_DEBUG_,
		HCL_ID " hostname: %s uri: %s",
		hcl->id, hcl->headers.hostname, hcl->headers.uri);

	/* Parse the request */
	if (!httpsrv_parse_request(hcl)) {
		/* function has added error already */
		return (true);
	}

	/* Is this a DJB API or a proxy request? */
	if (	strcmp(hcl->headers.hostname, "127.0.0.1:6543"	) == 0 ||
		strcmp(hcl->headers.hostname, "[::1]:6543"	) == 0 ||
		strcmp(hcl->headers.hostname, "localhost:6543"	) == 0) {

		/* API request */
		done = djb_handle_api(hcl, dh);
	} else {
		/* Proxied request */
		done = djb_handle_proxy(hcl);
	}

	logline(log_DEBUG_, HCL_ID " end", hcl->id);

	return (done);
}

void
djb_done(httpsrv_client_t *hcl, void *user);
void
djb_done(httpsrv_client_t *hcl, void *user) {
	djb_headers_t  *dh = (djb_headers_t *)user;

	logline(log_DEBUG_, HCL_ID, hcl->id);

	memzero(dh, sizeof *dh);
}

void
djb_close(httpsrv_client_t *hcl, void UNUSED *user);
void
djb_close(httpsrv_client_t *hcl, void UNUSED *user) {

	logline(log_DEBUG_, HCL_ID, hcl->id);
}

/*
 * pr = client request
 * ar = /pull/ API request
 */
static void
djb_handle_forward(djb_req_t *pr, djb_req_t *ar, const char *hostname);
static void
djb_handle_forward(djb_req_t *pr, djb_req_t *ar, const char *hostname) {
	djb_headers_t	*dh;

	fassert(pr->hcl);
	fassert(ar->hcl);

	logline(log_DEBUG_,
		"got request " HCL_ID ", got puller " HCL_ID,
		pr->hcl->id, ar->hcl->id);

	fassert(conn_is_valid(&pr->hcl->conn));
	fassert(conn_is_valid(&ar->hcl->conn));

	/* Resume reading from the sockets */
	httpsrv_speak(pr->hcl);
	httpsrv_speak(ar->hcl);

	/* pr's headers */
	dh = (djb_headers_t *)pr->hcl->user;

	/* HTTP okay */
	djb_httpanswer(ar->hcl, 200, "OK");

	/* DJB headers */
	conn_addheaderf(&ar->hcl->conn, "DJB-URI: http://%s%s%s%s",
			hostname ? hostname : pr->hcl->headers.hostname,
			pr->hcl->headers.uri,
			(strlen(pr->hcl->headers.args) > 0) ? "?" : "",
			pr->hcl->headers.args);

	conn_addheaderf(&ar->hcl->conn, "DJB-Method: %s",
			httpsrv_methodname(pr->hcl->method));

	conn_addheaderf(&ar->hcl->conn, "DJB-SeqNo: %09" PRIx64 "%09" PRIx64,
			pr->hcl->id, pr->hcl->reqid);

	/* Client to server */
	if (strlen(dh->cookie) > 0) {
		conn_addheaderf(&ar->hcl->conn, "DJB-Cookie: %s",
				dh->cookie);
	}

	if (pr->hcl->method != HTTP_M_POST) {
		logline(log_DEBUG_,
			"req " HCL_ID " with puller " HCL_ID " is non-POST",
			pr->hcl->id, ar->hcl->id);

		/* XHR requires a return, thus just give it a blank body */
		conn_addheaderf(&ar->hcl->conn, "Content-Type: text/html");

		/* Empty-ish body (Content-Length is arranged by conn) */
		conn_printf(&ar->hcl->conn, "Non-POST JumpBox response\r\n");

		/* This request is done */
		httpsrv_done(ar->hcl);

		/* Release it */
		free(ar);

		/* Put this on the proxy_out list now it is being handled */
		list_addtail_l(&lst_proxy_out, &pr->node);

		/* Served another one */
		thread_serve();
	} else {
		/* POST request */
		logline(log_DEBUG_,
			"req " HCL_ID " with puller " HCL_ID " is POST",
			pr->hcl->id, ar->hcl->id);

		/* Add the content type of the data to come */
		conn_addheaderf(&ar->hcl->conn, "Content-Type: %s",
				strlen(pr->hcl->headers.content_type) > 0 ?
					pr->hcl->headers.content_type :
					"text/html");

		/* Is there no body, then nothing further to do */
		if (pr->hcl->headers.content_length == 0) {
			/* This request is done (after flushing) */
			httpsrv_done(ar->hcl);

			/* Release it */
			free(ar);

			/* Served another one */
			thread_serve();
		} else {
			/* Resume reading/writing from the sockets */
			httpsrv_speak(pr->hcl);

			/* Put this on the proxy_body list */
			list_addtail_l(&lst_proxy_body, &ar->node);

			/* Put this on the proxy_out list */
			list_addtail_l(&lst_proxy_out, &pr->node);

			/* Forward the body from pr->hcl to ar->hcl */
			httpsrv_forward(pr->hcl, ar->hcl);

			logline(log_DEBUG_,
				"Forwarding POST body from "
				HCL_ID " to " HCL_ID,
				pr->hcl->id, ar->hcl->id);
		}
	}

	logline(log_DEBUG_, "end");
}

static void *
djb_worker_thread(void UNUSED *arg);
static void *
djb_worker_thread(void UNUSED *arg) {
	const char	*hostname = NULL;
	djb_req_t	*pr, *ar;

	logline(log_DEBUG_, "...");

	/* Forcing the hostname to something else than what the requestor wants? */
	hostname = getenv("DJB_FORCED_HOSTNAME");

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

		fassert(pr->hcl);

		logline(log_DEBUG_, "got request " HCL_ID ", getting poller",
			pr->hcl->id);

		/* We got a request, get a puller for it */
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

		logline(log_DEBUG_, "got request " HCL_ID " and poller "HCL_ID,
			pr->hcl->id, ar->hcl->id);

		djb_handle_forward(pr, ar, hostname);

		logline(log_DEBUG_, "end");
	}

	logline(log_DEBUG_, "exit");

	return (NULL);
}

static int
djb_run(void);
static int
djb_run(void) {
	httpsrv_t	*hs = NULL;
	int		ret = 0;
	unsigned int	i;

	/* Create out DGW structure */
	hs = (httpsrv_t *)mcalloc(sizeof *hs, "httpsrv_t");
	if (hs == NULL) {
		logline(log_CRIT_, "No memory for HTTP Server");
		return (-1);
	}

	while (true) {
		/* Launch a few worker threads */
		for (i = 0; i < DJB_WORKERS; i++) {
			if (!thread_add("DJBWorker", &djb_worker_thread, NULL)) {
				logline(log_CRIT_, "could not create thread");
				ret = -1;
				break;
			}
		}

		/* Initialize a HTTP Server */
		if (!httpsrv_init(hs, NULL,
				  djb_html_top,
				  djb_html_tail,
				  djb_accept,
				  djb_header,
				  djb_handle,
				  djb_bodyfwd_done,
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

	/* Just sleep over here */
	while (thread_keep_running()) {
		thread_sleep(5000);
	}

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
