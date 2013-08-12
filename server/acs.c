#include "djb.h"

/* The NET as provided by Rendezvous */
json_t *net = NULL;

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
 * - ACS-part of the plugin (though acs_dance_initial())
 */
void
acs_set_net(json_t *net_) {
	logline(log_DEBUG_, "");

	/* Old NET? */
	if (net != NULL) {
		/* Dereference */
		json_decref(net);
		net = NULL;
	}

	/* The new NET */
	assert(net_);
	net = net_;
	json_incref(net);
}

void
acs_result(httpsrv_client_t *hcl, const char *status, const char *msg);
void
acs_result(httpsrv_client_t *hcl, const char *status, const char *msg) {
	char t[512];

	snprintf(t, sizeof t,
		"{"
		"\"status\": \"%s\", "
		"\"message\": \"%s\""
		"}",
		status, msg);

	djb_result(hcl, t);
}

void
acs_result_e(httpsrv_client_t *hcl, const char *msg);
void
acs_result_e(httpsrv_client_t *hcl, const char *msg) {
	acs_result(hcl, "error", msg);
}

void
acs_initial(httpsrv_client_t *hcl);
void
acs_initial(httpsrv_client_t *hcl) {
	json_error_t	jerr;
	json_t		*initial_j, *root;
	const char	*initial;

	logline(log_DEBUG_, "..");

	/* POST? Then read in the NET from the HTTP request */
	if (hcl->method == HTTP_M_POST) {
		logline(log_DEBUG_, "POST, checking for body");

		/* No BODY yet, then we have to start reading */
		if (hcl->readbody == NULL) {
			logline(log_DEBUG_, "POST allocating memory for body");

			if (httpsrv_readbody_alloc(hcl, 0, 0) < 0) {
				acs_result_e(hcl, "Out of Memory");
			} else {
				logline(log_DEBUG_, "POST let it be read");
				/* Let httpsrv read it in */
			}

			return;
		}

		logline(log_DEBUG_, "Got a POST body, set the NET");

		/* We should have a body, try to convert it into JSON */
		root = json_loads(hcl->readbody, 0, &jerr);
		if (root == NULL) {
			/* Failure */
			logline(log_DEBUG_,
				"Could not parse NET (JSON load failed): "
				"line %u, column %u: %s",
				jerr.line, jerr.column, jerr.text);
			dumppacket(LOG_ERR, (uint8_t *)hcl->readbody, hcl->readbodylen);

			httpsrv_readbody_free(hcl);
			return;
		}

		/* Set the JSON root */
		acs_set_net(root);

		/* We do not use root here anymore */
		json_decref(root);

		/* We are set, thus free it up */
		httpsrv_readbody_free(hcl);

	} else if (hcl->method != HTTP_M_GET) {
		acs_result_e(hcl, "Invalid Method");
		return;
	}

	/* No NET yet? */
	if (net == NULL) {
		acs_result_e(hcl, "No NET was provided");
		return;
	}

	/* Get the Initial Gateway from the NET */
	initial_j = json_object_get(net, "initial");

	if (!json_is_string(initial_j)) {
		acs_result_e(hcl, "No initial gateway in NET");
		return;
	}

	initial = json_string_value(initial_j);

	logline(log_DEBUG_, "Initial Gateway: %s", initial);

	/* Ask the plugin to contact the Initial gateway */

	/* httpsrv_newcl(hcl); */

	/* XXX */

	/* Block till we are done */
	djb_error(hcl, 500, "Not fully implemented");
}

void
acs_redirect(httpsrv_client_t *hcl);
void
acs_redirect(httpsrv_client_t *hcl) {
	djb_error(hcl, 500, "Not implemented");
}

void
acs(httpsrv_client_t *hcl) {
	const char *uri = &hcl->headers.uri[4];

	if (strcasecmp(uri, "/initial/") == 0) {
		acs_initial(hcl);
		return;

	} else if (strcasecmp(uri, "/redirect/") == 0) {
		acs_redirect(hcl);
		return;
	}

	/* Not a valid API request */
	djb_error(hcl, 404, "No such DJB API request (ACS)");
}

