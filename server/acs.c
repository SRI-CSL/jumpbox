#include "djb.h"

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
 *
 * The "when" field is optional and contains a ISO8601 Interval
 * for when the NET is valid.
 */

/* The HTTP Server */
static httpsrv_t	*l_hs = NULL;

/* Current status message */
static mutex_t		l_status_mutex;
static cond_t		l_status_cond;
static djb_status_t	l_status = DJB_ERR;
static char		l_message[DJB_MSGLEN];
static hlist_t		l_messages;

/* Are we dancing? */
static mutex_t		l_dancing_mutex;
static bool		l_dancing = false;
static json_t		*l_net = NULL;

typedef struct {
	hnode_t		node;
	uint64_t	status;
	char		message[DJB_MSGLEN];
} acsmsg_t;

static void
acs_msg_free(acsmsg_t *m);
static void
acs_msg_free(acsmsg_t *m) {
	mfree(m, sizeof *m, "acsmsg");
}

static void
acs_status(djb_status_t status, const char *format, ...) ATTR_FORMAT(printf, 2, 3);
static void
acs_status(djb_status_t status, const char *format, ...) {
	acsmsg_t	*m;
	va_list		ap;
	int		r;

	mutex_lock(l_status_mutex);

	l_status = status;

	va_start(ap, format);
	r = vsnprintf(l_message, sizeof l_message, format, ap);
	va_end(ap);

	if (!snprintfok(r, sizeof l_message)) {
		log_err("ACS message too long (%u>%u)",
			r, (int)sizeof l_message);

		snprintf(l_message, sizeof l_message, "Message too long");
		l_status = DJB_ERR;
	}

	/* Log it too, so it is easy to find as a single string */
	log_dbg("%s", l_message);

	m = (acsmsg_t *)mcalloc(sizeof *m, "acsmsg");
	if (m == NULL) {
		log_err("No memory for ACS Message");
		l_status = DJB_ERR;
	} else {
		/* Init the node */
		node_init(&m->node);

		/* Copy this status over */
		fassert(lengthof(m->message) == lengthof(l_message));
		m->status = l_status;
		memcpy(m->message, l_message, sizeof m->message);

		/* Keep a history of messages */
		list_addtail_l(&l_messages, &m->node);
	}

	/* Notify possible listeners */
	cond_trigger(l_status_cond);

	mutex_unlock(l_status_mutex);
}

static void
acs_sitdown(void);
static void
acs_sitdown(void) {
	log_dbg("..");
	mutex_lock(l_dancing_mutex);
	l_dancing = false;
	mutex_unlock(l_dancing_mutex);
}

static bool
acs_keep_running(void);
static bool
acs_keep_running(void) {
	if (thread_keep_running())
		return (true);

	acs_status(DJB_ERR, "ACS Dance slipped, aborting");
	acs_sitdown();
	return (false);
}

static bool
acs_check_net(json_t *net);
static bool
acs_check_net(json_t *net) {
	if (!json_is_object(net)) {
		acs_status(DJB_ERR, "Provided NET is not a JSON object");
		return (false);
	}

	/*
	 * XXX: Verify that all components (initial,redirect,etc)
	 *      are present and contain mostly valid details
	 *
	 *      Currently this is done during the dance and errors there
	 */

	return (true);
}

/*
 * Set the NET, might get called by:
 * - Rendezvous when it is finished
 * - ACS-part of the plugin (through acs_setup())
 *
 * Call with NULL to 'reset'/cleanup the status
 */
bool
acs_set_net(json_t *net) {
	char *j;

	log_dbg("...");

	/* Check if we are dancing already */
	mutex_lock(l_dancing_mutex);

	if (l_dancing) {
		/* Already dancing, thus can't change anything */
		mutex_unlock(l_dancing_mutex);
		acs_status(DJB_ERR, "Already dancing, can't replace NET");
		return (false);
	}

	/* Old NET? */
	if (l_net != NULL) {
		/* Dereference */
		json_decref(l_net);
		l_net = NULL;
	}

	/* The new NET */
	l_net = net;

	if (net != NULL) {
		if (acs_check_net(net)) {
			j = json_dumps(net, JSON_COMPACT | JSON_ENSURE_ASCII);
			if (j != NULL) {
				acs_status(DJB_OK, "NET: %s", j);
				free(j);
			}

			/* Reference it so it will not go away */
			json_incref(net);
			acs_status(DJB_OK, "Ready to Dance");
		} else {
			l_net = NULL;
		}
	} else {
		log_dbg("ACS NET reset");
	}

	/* It can start dancing now */
	mutex_unlock(l_dancing_mutex);

	return (true);
}

static const char *
acs_net_string(const char *var, const char *desc);
static const char *
acs_net_string(const char *var, const char *desc) {
	json_t		*str_j;
	const char 	*str;

	/* Get the string from the NET */
	str_j = json_object_get(l_net, var);

	if (!json_is_string(str_j)) {
		log_dbg("%s (%s): not found", desc, var);
		acs_status(DJB_ERR, "No %s in NET", desc);
		acs_sitdown();
		return (NULL);
	}

	str = json_string_value(str_j);
	fassert(str != NULL);

	log_dbg("%s (%s): %s", desc, var, str);

	return (str);
}

static bool
acs_net_number(const char *var, const char *desc, uint64_t *val);
static bool
acs_net_number(const char *var, const char *desc, uint64_t *val) {
	json_t *num_j;

	/* Get the number from the NET */
	num_j = json_object_get(l_net, var);

	if (!json_is_number(num_j)) {
		log_dbg("%s (%s): not found", desc, var);
		acs_status(DJB_ERR, "No %s in NET", desc);
		acs_sitdown();
		return (false);
	}

	*val = json_number_value(num_j);

	log_dbg("%s (%s): %" PRIu64, desc, var, *val);

	return (true);
}

static bool
acs_setup(httpsrv_client_t *hcl);
static bool
acs_setup(httpsrv_client_t *hcl) {
	json_error_t	jerr;
	json_t		*root;
	bool		ok;

	log_dbg("...");

	if (hcl->method != HTTP_M_POST) {
		djb_result(hcl, DJB_ERR,
			   "Setup requires a POST to provide the NET");
		return (true);
	}

	log_dbg("POST, checking for body");

	/* No BODY yet, then we have to start reading */
	if (hcl->readbody == NULL) {
		if (hcl->headers.content_length == 0) {
			djb_error(hcl, 400, "setup requires length");
			return (true);
		}

		log_dbg("POST allocating memory for body");

		if (httpsrv_readbody_alloc(hcl, 0, 0) < 0) {
			djb_result(hcl, DJB_ERR, "Out of Memory");
			return (true);
		}
		
		log_dbg("POST let it be read");
		/* Let httpsrv read it in */

		/* Nothing to do currently */
		return (false);
	}

	log_dbg("Got a POST body, set the NET");

	/* We should have a body, try to convert it into JSON */
	root = json_loads(hcl->readbody, 0, &jerr);
	if (root == NULL) {
		/* Failure */
		log_err("Could not parse NET (JSON load failed): "
			"line %u, column %u: %s",
			jerr.line, jerr.column, jerr.text);

		dumppacket(LOG_ERR,
			   (uint8_t *)hcl->readbody,
			   hcl->readbody_len);

		httpsrv_readbody_free(hcl);

		return (true);
	}

	/* Set the JSON root */
	ok = acs_set_net(root);

	/* We do not use root here anymore */
	json_decref(root);

	/* We are set, thus free it up */
	httpsrv_readbody_free(hcl);

	if (ok) {
		djb_result(hcl, DJB_OK, "ACS setup successful");
		return (true);
	}

	djb_result(hcl, DJB_ERR, "Could not setup NET");
	return (false);
}

static bool
acs_request(djb_push_f callback, const char *hostname, const char *uri);
static bool
acs_request(djb_push_f callback, const char *hostname, const char *uri) {
	httpsrv_client_t	*hcl;
	djb_headers_t		*dh;

	log_dbg("hostname: %s, uri: %s", hostname, uri);

	if (!acs_keep_running()) {
		return (true);
 	}

	assert(l_hs != NULL);
	hcl = httpsrv_newcl(l_hs);

	if (hcl == NULL) {
		acs_status(DJB_ERR, "Failed to create request");
		return (false);
	}

	dh = djb_create_userdata(hcl);
	if (dh == NULL) {
		acs_status(DJB_ERR, "Failed to create userdata");
		httpsrv_close(hcl);
		return (false);
	}

	/* Set our djb_push callback (internal proxy) */
	dh->push = callback;

	/* Fill in the request */
	hcl->method = HTTP_M_GET;
	strcpy(hcl->headers.hostname, hostname);
	strcpy(hcl->headers.rawuri, uri);

	/* Add it to the queue, call back will handle it further */
	return (djb_proxy_add(hcl));
}

static bool
acs_redirect_answer(httpsrv_client_t *shcl, httpsrv_client_t *hcl);
static bool
acs_redirect_answer(httpsrv_client_t *shcl, httpsrv_client_t *hcl) {
	djb_headers_t	*sdh;
	unsigned int	i, ans_len;
	char		*ans;
	bool		ok;

	log_dbg("..");

	sdh = httpsrv_get_userdata(shcl);

	/* Did the request go okay? */
	i = atoi(sdh->httpcode);
	if (i != 200) {
		acs_status(DJB_ERR,
			   "ACS Redirect failed: %u %s",
			   i, sdh->httptext);
		httpsrv_client_destroy(hcl);
		acs_sitdown();
		return (true);
	}

	/* No body yet? Then allocate some memory to get it */
	if (shcl->readbody == NULL) {
		if (shcl->headers.content_length == 0) {
			log_dbg("redirect_answer requires length");
			acs_status(DJB_ERR,
				   "ACS Redirect failed: no length");
			return (true);
		}

		if (httpsrv_readbody_alloc(shcl, 2, 0) < 0){
			log_dbg("httpsrv_readbody_alloc() failed");
			acs_status(DJB_ERR,
				   "ACS Redirect failed: no memory");
			return (true);
		}

		/* I want that body, thus not done yet */
		return (false);
	}

	/* Decode the result */
	if (!steg_decode(shcl->readbody, shcl->readbody_off,
			 shcl->headers.content_type,
			 &ans, &ans_len)) {
		acs_status(DJB_ERR,
			   "ACS Redirect failed: desteg failed");
		return (true);
	}

	acs_status(DJB_OK, "ACS Redirect success: HTTP %u %s",
		   i, sdh->httptext);

	log_dbg("Bridge Details: %s", ans);

	ok = prf_parse_bridge_details(ans);

	/* Free it up */
	steg_free(ans, ans_len, NULL, 0);

	/* Done with this request */
	httpsrv_client_destroy(hcl);

	if (ok) {
		/* Show okay, we are done */
		acs_status(DJB_OK, "ACS Dance complete");

		/* Show okay, we are done */
		acs_status(DJB_DONE,
			   "ACS completed successfully, you can "
			   "launch Tor over StegoTorus over "
			   "JumpBox/DGW");
	} else {
		acs_status(DJB_ERR,
			   "Unable to parse ACS received Bridge Details");
	}

	/* Done dancing */
	acs_sitdown();

	/* Done */
	return (true);
}

static void
acs_redirect(void);
static void
acs_redirect(void) {
	const char	*redirect;

	log_dbg("..");

	redirect = acs_net_string("redirect", "Redirect Gateway");
	if (redirect == NULL)
		return;

	/* Inject ACS Redirect into the proxy queue */
	if (!acs_request(acs_redirect_answer, redirect, "/")) {
		acs_sitdown();
	} else {
		acs_status(DJB_OK, "Dancing: Redirect Request sent");
	}
}

static void
acs_wait(void);
static void
acs_wait(void) {
	uint64_t d_window, d_wait, w;

	if (!acs_net_number("window", "Delay window", &d_window))
		return;

	if (!acs_net_number("wait", "Delay wait", &d_wait))
		return;

	/* Calculate a random wait + window */
	w = generate_random_number();
	w = d_wait + (w % d_window);

	acs_status(DJB_OK, "Moonwalking for %" PRIu64 " seconds...", w);

	if (!thread_sleep(w * 1000)) {
		acs_status(DJB_ERR, "Moonwalking aborted");
		return;
	}

	acs_status(DJB_OK, "Moonwalk done");

	if (!acs_keep_running())
		return;

	/* Go to the redirect phase */
	acs_redirect();
}

static bool
acs_initial_answer(httpsrv_client_t *shcl, httpsrv_client_t *hcl);
static bool
acs_initial_answer(httpsrv_client_t *shcl, httpsrv_client_t *hcl) {
	djb_headers_t	*sdh;
	unsigned int	i;

	log_dbg("..");

	sdh = httpsrv_get_userdata(shcl);

	/* Did the request go okay? */
	i = atoi(sdh->httpcode);
	if (i != 200) {
		acs_status(DJB_ERR,
			   "ACS Initial failed: %u %s",
			   i, sdh->httptext);
		httpsrv_client_destroy(hcl);
		acs_sitdown();

		/* Done */
		return (true);
	}

	acs_status(DJB_OK, "ACS Initial success: HTTP %u %s", i, sdh->httptext);

	/* Done with this request */
	httpsrv_client_destroy(hcl);

	if (!acs_keep_running())
		return (true);

	/* Perform Wait stage */
	acs_wait();

	/* Done */
	return (true);
}

static void
acs_initial(void);
static void
acs_initial(void) {
	json_t			*initial_j;
	const char		*initial;

	log_dbg("..");

	/* Get the Initial Gateway from the NET */
	initial_j = json_object_get(l_net, "initial");

	if (initial_j == NULL || !json_is_string(initial_j)) {
		acs_status(DJB_ERR, "No ACS Initial Gateway in NET");
		acs_sitdown();
		return;
	}

	initial = json_string_value(initial_j);
	fassert(initial);

	log_dbg("Initial Gateway: %s", initial);

	/* Inject ACS Initial into the proxy queue */
	if (!acs_request(acs_initial_answer, initial, "/")) {
		acs_sitdown();
	} else {
		acs_status(DJB_OK, "Dancing: Initial Request sent");
	}
}

static bool
acs_when(void);
static bool
acs_when(void) {
	json_t		*when_j;
	const char	*when;
	uint64_t	now, start, end;

	log_dbg("..");

	/* Get the "when" from the NET */
	when_j = json_object_get(l_net, "when");

	if (when_j == NULL) {
		acs_status(DJB_OK, "No NET 'when' thus NET is always valid");
		return (true);
	}

	if (!json_is_string(when_j)) {
		acs_status(DJB_ERR, "NET 'when' is not a string, thus invalid");
		acs_sitdown();
		return (false);
	}

	when = json_string_value(when_j);
	fassert(when);

	log_dbg("NET when: %s", when);

	/* Parse the interval */
	if (!parse_iso8601_interval(when, &start, &end)) {
		acs_status(DJB_ERR, "NET when (%s) format is invalid", when);
		acs_sitdown();
		return (false);
	}

	/* Current time */
	now = time(NULL);

	if (now < start) {
		acs_status(DJB_ERR,
			   "NET when (%s) makes Discovery Provisioning "
			   "not valid yet",
			   when);
		acs_sitdown();
		return (false);
	}

	if (now >= end) {
		acs_status(DJB_ERR,
			   "NET when (%s) notes that "
			   "Discovery Provisioning expired",
			   when);
		acs_sitdown();
		return (false);
	}

	acs_status(DJB_OK,
		   "NET 'when' (%s) indicates it is currently valid",
		   when);

	return (true);
}

static bool
acs_progress(httpsrv_client_t *hcl);
static bool
acs_progress(httpsrv_client_t *hcl) {
	acsmsg_t *m;

	/* Check if we are dancing already */
	mutex_lock(l_dancing_mutex);

	mutex_lock(l_status_mutex);

	if (!l_dancing && l_status == DJB_OK) {
		if (l_net == NULL) {
			mutex_unlock(l_status_mutex);
			mutex_unlock(l_dancing_mutex);

			acs_status(DJB_ERR, "No NET setup thus can't dance");
		} else {
			/* Start the dance */
			l_dancing = true;

			mutex_unlock(l_status_mutex);
			mutex_unlock(l_dancing_mutex);

			/* Check time validity */
			if (acs_when()) {
				/* Time valid thus go on */
				acs_status(DJB_OK, "Starting to dance...");

				/* Queue ACS Initial */
				acs_initial();
			}
		}

		/* Lock it up again */
		mutex_lock(l_status_mutex);
	} else {
		/* Nothing to report */
		mutex_unlock(l_dancing_mutex);
	}

	/* Already a message on the queue? */
	m = (acsmsg_t *)list_pop(&l_messages);
	if (m != NULL) {
		/* Return it directly */
		djb_result(hcl, m->status, m->message);
		acs_msg_free(m);
	} else {
		/* Wait for 5 seconds till we get signaled or time out */
		cond_wait(l_status_cond, l_status_mutex, (5*1000));

		/* Try to pop it from the list */
		m = (acsmsg_t *)list_pop(&l_messages);
		if (m != NULL) {
			/* Return the one from the queue */
			djb_result(hcl, m->status, m->message);
			acs_msg_free(m);
		} else {
			/* Return the current status + message */
			djb_result(hcl, l_status, l_message);
		}
	}

	mutex_unlock(l_status_mutex);

	return (true);
}

bool
acs_handle(httpsrv_client_t *hcl) {
	/* Skip the /acs/ portion */
	const char *uri = &hcl->headers.uri[4];

	log_dbg("ACS: %s", uri);

	if (strcasecmp(uri, "/setup/") == 0) {
		return (acs_setup(hcl));

	} else if (strcasecmp(uri, "/progress/") == 0) {
		return (acs_progress(hcl));
	}

	/* Not a valid API request */
	djb_error(hcl, 404, "No such DJB API request (ACS)");

	return (true);
}

void
acs_init(httpsrv_t *hs) {
	/* Init the mutex */
	mutex_init(l_status_mutex);

	/* Init the condition */
	cond_init(l_status_cond);

	/* Init the mutex */
	mutex_init(l_dancing_mutex);

	/* Message history */
	list_init(&l_messages);

	/* The HTTPserver */
	assert(hs != NULL);
	l_hs = hs;

	/* Empty it */
	memzero(l_message, sizeof l_message);

	acs_status(DJB_OK, "ACS Dancer Initialized");
}

void
acs_exit(void) {
	acsmsg_t *m;

	/* Stopped dancing */
	acs_sitdown();

	/* Reset */
	acs_set_net(NULL);

	/* Notify possible listeners */
	mutex_lock(l_status_mutex);
	cond_trigger(l_status_cond);
	mutex_unlock(l_status_mutex);

	l_hs = NULL;

	while ((m = (acsmsg_t *)list_pop(&l_messages))) {
		acs_msg_free(m);
	}

	/* Cleanup */
	list_destroy(&l_messages);
	mutex_destroy(l_dancing_mutex);
	cond_destroy(l_status_cond);
	mutex_destroy(l_status_mutex);
}

