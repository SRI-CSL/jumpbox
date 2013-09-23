#include "djb.h"

/* The NET as provided by Rendezvous */
static json_t *l_net = NULL;

/*
 * An example NET:
 *
 * {
 *	"window"	: 7,
 *	"wait"		: 4,
 *	"redirect"	: "192.0.1.2",
 *	"initial"	: "192.0.1.25",
 *	"passphrase"	: "8b42c8971567e309c5fe7865"
 * }
 */

/*
 * Set the NET, might get called by:
 * - Rendezvous when it is finished
 * - ACS-part of the plugin (through acs_dance_initial())
 *
 * Call with NULL to 'reset'/cleanup the status
 */
void
acs_set_net(json_t *net) {
	logline(log_DEBUG_, "...");

	/* Old NET? */
	if (l_net != NULL) {
		/* Dereference */
		json_decref(l_net);
		l_net = NULL;
	}

	/* The new NET */
	l_net = net;

	if (net != NULL) {
		/* Reference it so it will not go away */
		json_incref(net);
	}
}

enum acs_status
{
	ACS_ERROR = 0,
	ACS_OK
};

static void
acs_result(httpsrv_client_t *hcl, enum acs_status status, const char *msg);
static void
acs_result(httpsrv_client_t *hcl, enum acs_status status, const char *msg) {
	char	t[1024];
	int	r;

	r = snprintf(t, sizeof t,
		"{"
		"\"status\": \"%s\", "
		"\"message\": \"%s\""
		"}",
		status == ACS_ERROR ? "error" : "ok",
		msg);

	if (!snprintfok(r, sizeof t)) {
		acs_result(hcl, ACS_ERROR, "Too Big");
	}

	djb_result(hcl, t);
}

static void
acs_result_e(httpsrv_client_t *hcl, const char *msg);
static void
acs_result_e(httpsrv_client_t *hcl, const char *msg) {
	acs_result(hcl, ACS_ERROR, msg);
}

static bool
acs_initial(httpsrv_client_t *hcl);
static bool
acs_initial(httpsrv_client_t *hcl) {
	json_error_t	jerr;
	json_t		*initial_j, *root;
	const char	*initial;

	logline(log_INFO_, "..");

	/* POST? Then read in the NET from the HTTP request */
	if (hcl->method == HTTP_M_POST) {
		logline(log_INFO_, "POST, checking for body");

		/* No BODY yet, then we have to start reading */
		if (hcl->readbody == NULL) {
			logline(log_INFO_, "POST allocating memory for body");

			if (httpsrv_readbody_alloc(hcl, 0, 0) < 0) {
				acs_result_e(hcl, "Out of Memory");
				return (true);
			} else {
				logline(log_INFO_, "POST let it be read");
				/* Let httpsrv read it in */
			}

			/* Nothing to do currently */
			return (false);
		}

		logline(log_INFO_, "Got a POST body, set the NET");

		/* We should have a body, try to convert it into JSON */
		root = json_loads(hcl->readbody, 0, &jerr);
		if (root == NULL) {
			/* Failure */
			logline(log_INFO_,
				"Could not parse NET (JSON load failed): "
				"line %u, column %u: %s",
				jerr.line, jerr.column, jerr.text);

			dumppacket(LOG_ERR,
				   (uint8_t *)hcl->readbody,
				   hcl->readbody_len);

			httpsrv_readbody_free(hcl);

			return (true);
		}

		/* Set the JSON root */
		acs_set_net(root);

		/* We do not use root here anymore */
		json_decref(root);

		/* We are set, thus free it up */
		httpsrv_readbody_free(hcl);

	} else if (hcl->method != HTTP_M_GET) {
		acs_result_e(hcl, "Invalid Method");
		return (true);
	}

	/* No NET yet? */
	if (l_net == NULL) {
		acs_result_e(hcl, "No NET was provided");
		return (true);
	}

	/* Get the Initial Gateway from the NET */
	initial_j = json_object_get(l_net, "initial");

	if (!json_is_string(initial_j)) {
		acs_result_e(hcl, "No initial gateway in NET");
		return (true);
	}

	initial = json_string_value(initial_j);

	logline(log_INFO_, "Initial Gateway: %s", initial);

	/*
	 * We ask the plugin to contact the Initial gateway
	 * we 're-use' the current request
	 */
	hcl->method = HTTP_M_GET;
	memzero(hcl->the_request, sizeof hcl->the_request);
	buf_emptyL(&hcl->the_headers);
	memzero(&hcl->headers, sizeof hcl->headers);
	strcpy(hcl->headers.hostname, initial);
	strcpy(hcl->headers.uri, "/");

	/* Block till we are done */
	djb_proxy_add(hcl);

	/* Nothing further to process for the moment */
	return (true);
}

static bool
acs_redirect(httpsrv_client_t *hcl);
static bool
acs_redirect(httpsrv_client_t *hcl) {
	djb_error(hcl, 500, "Not implemented");

	return (true);
}

bool
acs_handle(httpsrv_client_t *hcl) {
	const char *uri = &hcl->headers.uri[4];

	if (strcasecmp(uri, "/initial/") == 0) {
		return (acs_initial(hcl));

	} else if (strcasecmp(uri, "/redirect/") == 0) {
		return (acs_redirect(hcl));
	}

	/* Not a valid API request */
	djb_error(hcl, 404, "No such DJB API request (ACS)");

	return (true);
}

void
acs_exit(void) {
	/* Reset */
	acs_set_net(NULL);
}

