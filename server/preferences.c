#include "djb.h"

/* define to enable verbose debug output from prf_handle() function */
#undef DEBUGHANDLE

/* XXX: relies on strdup/calloc, verify if all is free()'d properly */

static mutex_t	l_mutex;
static char	*l_current_preferences = NULL;
static json_t	*l_bridge_access_list = NULL;

/* keep these ALL the same length (number_of_keys) */
static const char *l_keys[PRF_MAX] =  {
	"stegotorus_circuit_count", 
	"stegotorus_executable", 
	"stegotorus_log_level", 
	"stegotorus_steg_module", 
	"stegotorus_trace_packets", 
	"shared_secret", 
	"proxy_address", 
	"djb_address",
	"server_address"
	};

static char *l_values[PRF_MAX];

static const char *l_defaults[PRF_MAX] = {
	"1", 
	"/usr/sbin/stegotorus", 
	"warn", 
	"json", 
	"false", 
	NULL, 
	"127.0.0.1:1080",
	"127.0.0.1:6543",
	"127.0.0.1:8080",
	};

const char *
prf_get_value(enum prf_v i) {
	/* Just in case */
	fassert(i < PRF_MAX);

	/* Out of bound? */
	if (i >= PRF_MAX) {
		return (NULL);
	}

	/* Either return the current or the default */
	return ((l_values[i] != NULL) ? l_values[i] : l_defaults[i]);
}

static void
prf_set_value(enum prf_v i, const char *val);
static void
prf_set_value(enum prf_v i, const char *val) {
	/* Out of bound? */
	if (i >= PRF_MAX) {
		log_err("Value out of range");
		return;
	}

	if (l_values[i] != NULL)
		free(l_values[i]);

	log_dbg("Setting %s to %s", l_keys[i], val);
	l_values[i] = val == NULL ? NULL : strdup(val);

	if (l_values[i] == NULL) {
		log_err("Failed to duplicate string for preference value");
	}
}

/*
 * Common options:
 * stegotorus --log-min-severity=warn chop socks --persist-mode --trace-packets
 * 1          2                       3    4      5             [opt]
 *
 * --shared-secret=bingoBedbug 127.0.0.1:1080 127.0.0.1:6543 ${MODULE} ... NULL
 * [opt]                       6               [ n circuits * 2]           7 
 */

/*
 * Get the Stegotorus argc & argv for a call to exevp from the preferences.
 *
 * Usage:
 *
 *  int argc;
 *  char **argv = NULL;
 *
 *  argc = prf_get_argv(&argv);
 *
 *  if (argc > 0) {
 *  ...
 *  }
 *
 *  prf_free_argv(argv);
 */
int
prf_get_argv(char **argvp[]) {
	char		**argv = NULL;
	const char	*ss, *sm, *ja;
	bool		trace_packets;
	unsigned int	circuits, argc, i;
	int		r;
	char		scratch[256];
	unsigned int	vslot = 0;

	if (argvp == NULL) {
		log_dbg("No argument storage provided");
		return (-1);
	}

	/* Lock up to avoid other threads from modifying the prefs */
	mutex_lock(l_mutex);

	/* Do we want to trace packets? */
	trace_packets = (strcmp(prf_get_value(PRF_TP), "true") == 0);

	/* How many circuits? */
	circuits = atoi(prf_get_value(PRF_CC));

	/* Starting argument count */
	argc = 7 + (2 * circuits);

	/* Override the Shared Secret */
	ss = getenv("DJB_FORCED_SHAREDSECRET");
	if (ss == NULL) {
		ss = prf_get_value(PRF_SHS);
	} else {
		log_inf("Using Forced Shared Secret: %s", ss);
	}

	/* Override the JumpBox Address? */
	ja = getenv("DJB_FORCED_JUMPBOXADDRESS");
	if (ja == NULL) {
		ja = prf_get_value(PRF_JA);
	} else {
		log_inf("Using Forced JumpBox Address: %s", ja);
	}

	/* Override the Steg Method? */
	sm = getenv("DJB_FORCED_STEGMETHOD");
	if (sm == NULL) {
		sm = prf_get_value(PRF_SM);
	} else {
		log_inf("Using Forced Steg Method: %s", sm);
	}

	/* Optional arguments given? */
	if (ss != NULL && (strcmp(ss, "") != 0))
		argc++;

	if (trace_packets)
		argc++;
 
	argv = calloc(argc, sizeof argv);
	if (argv == NULL) {
		log_err("Could not allocate memory for arguments");
		mutex_unlock(l_mutex);
		return (-1);
	}

	/* Executable */
	argv[vslot++] = strdup(prf_get_value(PRF_EXE));

	memzero(scratch, sizeof scratch);
	r = snprintf(scratch, sizeof scratch, "--log-min-severity=%s", prf_get_value(PRF_LL));
	if (!snprintfok(r, sizeof scratch)) {
		log_err("Could not store log severity");
		mutex_unlock(l_mutex);
		return (-1);
	}
	
	argv[vslot++] = strdup(scratch);
	argv[vslot++] = strdup("chop");
	argv[vslot++] = strdup("socks");
	argv[vslot++] = strdup("--persist-mode");

	if (trace_packets) { 
		argv[vslot++] = strdup("--trace-packets");
	}

	if (ss != NULL && (strcmp(ss, "") != 0)) {
		memzero(scratch, sizeof scratch);
		snprintf(scratch, sizeof scratch, "--shared-secret=%s", ss);
		argv[vslot++] = strdup(scratch);
	}

	argv[vslot++] = strdup(prf_get_value(PRF_PA));

	for (i = 0; i < circuits; i++) {
		argv[vslot++] = strdup(ja);  
		argv[vslot++] = strdup(sm);
	}

	argv[vslot++] = NULL;

	*argvp = argv;

	mutex_unlock(l_mutex);

	if (vslot != argc) {
		log_err("Calculated argc = %u, but had vslot = %u", argc, vslot);
	}

	return (argc);
}

void
prf_free_argv(unsigned int argc, char *argv[]) {
	unsigned int i;

	for (i = 0; i < argc; i++) {
		free(argv[i]);
	}

	free(argv);
}

/*
	The Bridge Details one gets back from DGW (without comments :) :

	// JSON Object
	{
		"BR_Access_List":
		[
			// First Bridge
			{
				"BR_Access":
				{
					"br_expiration":1000,
					"br_identity":"IDENTITY",
					"br_secret":"SECRET"
				},
				"Camouflage":
				{
					"method":"http",
					"scheme":"steg",
					"key":"XXX:ticket:46"
				},
				"Contact":
				{
					"IP_address":"10.42.20.221",
					"Encapsulations":
					[
						{
							"IP_subheader":6,
							"Discriminator":80
						}
					]
				}
			},

			// Second Bridge
			{
				"BR_Access":
				...
			}
		]
	}
*/

bool
prf_parse_bridge_details(json_t *bridge);
bool
prf_parse_bridge_details(json_t *bridge) {
	json_t		*camouflage, *method,
			*contact, *ip_address;
	bool		ok = false;

	while (true) {
		if (bridge == NULL || !json_is_object(bridge)) {
			log_err("JSON Root is not a JSON Object");
			ok = false;
			break;
		}

		camouflage = json_object_get(bridge, "Camouflage");
		if (camouflage == NULL || !json_is_object(camouflage)) {
			log_err("BR Camouflage not found");
			break;
		}

		method = json_object_get(camouflage, "method");
		if (method == NULL || !json_is_string(method)) {
			log_err("BR Camouflage method not found");
			break;
		}

		contact = json_object_get(bridge, "Contact");
		if (contact == NULL || !json_is_object(contact)) {
			log_err("BR Contact not found");
			break;
		}

		ip_address = json_object_get(contact, "IP_address");
		if (ip_address == NULL || !json_is_string(ip_address)) {
			log_err("BR Contact IP_address not found");
			break;
		}

		/* XXX: We ignore the protocol/port for now (TCP/80) */

		/* Set the new values */
		prf_set_value(PRF_SM, json_string_value(method));
		prf_set_value(PRF_SA, json_string_value(ip_address));

		/* All done here now */
		log_dbg("Completed succesfully");
		ok = true;
		break;
	}

	return (ok);
}

void
prf_br_list(httpsrv_client_t *hcl);
void
prf_br_list(httpsrv_client_t *hcl) {
	json_t		*obj, *arr,
			*list, *bridge,
			*camouflage, *method, *scheme;
	const char	*j;
	bool		fail = false;
	size_t		i;

	if (l_bridge_access_list == NULL) {
		djb_result(hcl, DJB_ERR,
			   "No Bridge List set yet, perform ACS first");
		return;
	}

	if (!json_is_object(l_bridge_access_list)) {
		djb_result(hcl, DJB_ERR,
			   "Bridge List is not a valid JSON Object");
		return;
	}

	list = json_object_get(l_bridge_access_list, "BR_Access_List");
	if (list == NULL || !json_is_array(list)) {
		djb_result(hcl, DJB_ERR,
			   "Bridge Access List array missing");
		return;
	}

	arr = json_array();
	if (arr == NULL) {
		djb_result(hcl, DJB_ERR,
			   "Could not create a JSON array");
		return;
	}

	for (i = 0; !fail && i < json_array_size(list); i++) {
		bridge = json_array_get(list, i);
		if (bridge == NULL || !json_is_object(bridge)) {
			djb_result(hcl, DJB_ERR,
				  "Bridge missing in list");
			fail = true;
			return;
		}

		camouflage = json_object_get(bridge, "Camouflage");
		if (camouflage == NULL || !json_is_object(camouflage)) {
			djb_result(hcl, DJB_ERR, "BR Camouflage not found");
			fail = true;
			break;
		}

		method = json_object_get(camouflage, "method");
		if (method == NULL || !json_is_string(method)) {
			djb_result(hcl, DJB_ERR,
				   "BR Camouflage method not found");
			fail = true;
			break;
		}

		scheme = json_object_get(camouflage, "scheme");
		if (scheme == NULL || !json_is_string(scheme)) {
			djb_result(hcl, DJB_ERR,
				   "BR Camouflage scheme not found");
			fail = true;
			break;
		}

		/* Add them to the array */
		json_array_append_new(arr, json_pack(
			"{s:s, s:s}",
			"method", method,
			"scheme", scheme));

		/* continue */
	}

	if (fail) {
		/* Error reported already, just clean up and get out */
		json_decref(arr);
		return;
	}

	/* arr becomes part of this hence no decref */
	obj = json_pack("{s:o}", "bridges", arr);
	if (obj == NULL) {
		djb_result(hcl, DJB_ERR, "Could not pack JSON");
		json_decref(arr);
		return;
	}

	j = json_dumps(obj, JSON_COMPACT | JSON_ENSURE_ASCII);
	if (j == NULL) {
		djb_result(hcl, DJB_ERR, "Could not dump JSON");
	} else {
		/* Return the object */
		djb_result(hcl, DJB_OK, j);
	}

	/* Done with it */
	json_decref(obj);

	return;
}

bool
prf_set_bridge_access_list(const char *br) {
	json_error_t	jerr;

	if (l_bridge_access_list != NULL) {
		/* Remove the last reference that keeps it open */
		json_decref(l_bridge_access_list);
	}

	/* Load the new one */
	l_bridge_access_list = json_loads(br, 0, &jerr);
	if (l_bridge_access_list == NULL) {
		log_err("Could not JSON load Bridge Access List"
			"line %u, column %u: %s",
			jerr.line, jerr.column, jerr.text);

		return (false);
	}

	return (true);
}

static bool
prf_parse_preferences(void);
static bool
prf_parse_preferences(void) {
	json_error_t	jerr;
	json_t		*root;
	json_t		*valueobj;
	const char	*key;
	unsigned int	i;

	log_dbg("...");

	if (l_current_preferences == NULL) {
		log_err("No current preferences");
		return (false);
	}

	root = json_loads(l_current_preferences, 0, &jerr);
	if (root == NULL) {
		log_err("Could not JSON load preferences");
		return (false);
	} 

	if (!json_is_object(root)) {
		json_decref(root);
		log_err("JSON Root is not a JSON Object");
		return (false);
	}

	for (i = 0; i < PRF_MAX; i++) {
		key = l_keys[i];

		valueobj = json_object_get(root, key);

		if (!json_is_string(valueobj)) { 
			continue;
		} else {
			prf_set_value(i, json_string_value(valueobj));
		}
	}

	json_decref(root);

	/* All okay */
	return (true);
}

static void
prf_set(httpsrv_client_t *hcl);
static void
prf_set(httpsrv_client_t *hcl) {

	if (hcl->readbody == NULL) {
		if (hcl->headers.content_length == 0) {
			djb_error(hcl, 400,
				  "Setting preferences requires a length");
			return;
		}

		if (httpsrv_readbody_alloc(hcl, 0, 0) < 0) {
			log_wrn("httpsrv_readbody_alloc() failed");
		}

		/* Let httpsrv read */
		return;
	}

	log_dbg("prefs = %s", hcl->readbody);

	mutex_lock(l_mutex);

	if (l_current_preferences != NULL)
		free(l_current_preferences); 

	l_current_preferences = (hcl->readbody == NULL ? NULL : strdup(hcl->readbody));
	if (prf_parse_preferences()) {
#if DEBUGHANDLE
		/* this block is just for testing */
		unsigned int	argc = 0, i;
		char		**argv = NULL;

		for (i = 0; i < PRF_MAX; i++) {
			if (l_keys[i] == NULL)
				continue;

			if (l_values[i] == NULL)
				continue;

			log_dbg("%s => %s\n", l_keys[i], l_values[i]);
		}

		argc = prf_get_argv(&argv);
		log_wrn("argc = %u", argc);
		for (i = 0; i < argc; i++) {
			log_dbg("argv[%u] = %s\n", i, argv[i]);
		}
#endif
		djb_result(hcl, DJB_OK, "Preferences OK");
	} else {
		djb_result(hcl, DJB_OK, "Preferences broken");
	}

	mutex_unlock(l_mutex);
}

void
prf_handle(httpsrv_client_t *hcl) {
	/* Skip the /preferences/ portion */
	const char *uri = &hcl->headers.uri[12];

	log_dbg("URI: %s", uri);

	if (strcasecmp(uri, "/set/") == 0) {
		prf_set(hcl);
		return;

	} else if (strcasecmp(uri, "/bridge/list/") == 0) {
		prf_br_list(hcl);
		return;
	}

	/* Not a valid API request */
	djb_error(hcl, 404, "No such DJB API request (Preferences)");
}

void
prf_init(void) {
	/* Just in case */
	fassert(lengthof(l_defaults) == lengthof(l_keys));
	fassert(lengthof(l_defaults) == lengthof(l_values));

	/* Init the mutex */
	mutex_init(l_mutex);

	/* No values set yet */
	memzero(l_values, sizeof l_values);
}

void
prf_exit(void) {
	log_dbg("...");

	if (l_current_preferences != NULL) {
		free(l_current_preferences);
		l_current_preferences = NULL;
	}

	/* Destroy it */
	mutex_destroy(l_mutex);
}

