#include "djb.h"
#include "rendezvous.h"

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

void
acs_set_net(json_t *net_) {
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
acs_dance_initial(httpsrv_client_t *hcl);
void
acs_dance_initial(httpsrv_client_t *hcl) {
	json_t		*initial_j;
	const char	*initial;

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

	/* Ask the plugin to contact the Initial gateway */

	/* XXX */

	/* Block till we are done */
}

