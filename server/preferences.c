#include "djb.h"

/* XXX: relies on strdup/calloc, verify if all is free()'d properly */

static mutex_t	l_mutex;
static char	*l_current_preferences = NULL;

enum prf_v {
	PRF_CC = 0,
	PRF_EXE,
	PRF_LL,
	PRF_SM,
	PRF_TP,
	PRF_SHS,
	PRF_PA,
	PRF_JA,
	PRF_MAX		/* Maximum argument */
};

/* keep these ALL the same length (number_of_keys) */
static const char* l_keys[] =  {
	"stegotorus_circuit_count", 
	"stegotorus_executable", 
	"stegotorus_log_level", 
	"stegotorus_steg_module", 
	"stegotorus_trace_packets", 
	"shared_secret", 
	"proxy_address", 
	"djb_address"
	};

static char* l_values[PRF_MAX];

static const char* l_defaults[] = {
	"1", 
	"stegotorus", 
	"warn", 
	"json", 
	"false", 
	NULL, 
	"127.0.0.1:1080",
	"127.0.0.1:6543"
	};

static const char *
prf_getvalue(enum prf_v i);
static const char *
prf_getvalue(enum prf_v i) {
	/* Just in case */
	fassert(i < PRF_MAX);

	/* Out of bound? */
	if (i >= PRF_MAX) {
		return (NULL);
	} else {
		/* Either return the current or the default */
		return ((l_values[i] != NULL) ? l_values[i] : l_defaults[i]);
	}
}

/*
 * Common options:
 * stegotorus --log-min-severity=warn chop socks --persist-mode --trace-packets --shared-secret=bingoBedbug 127.0.0.1:1080 127.0.0.1:6543 ${MODULE} ... NULL
 * 1          2                       3    4      5             [opt]           [opt]                       6               [ n circuits * 2]           7 
*/
/*
 * Get the Stegotorus argc & argv for a call to exevp from the preferences.
 *
 * Usage:
 *
 *  int argc;
 *  char** argv = NULL;
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
prf_get_argv(char*** argvp) {
	char		**argv = NULL;
	const char	*shared_secret = prf_getvalue(PRF_SHS);
	bool		trace_packets = (strcmp(prf_getvalue(PRF_TP), "true") == 0);
	unsigned int	circuits = atoi(prf_getvalue(PRF_CC));
	unsigned int	argc = 7 + (2 * circuits), i;
	int		r;
	char		scratch[256];
	unsigned int	vslot = 0;

	if (argvp == NULL) {
		log_dbg("No arguments provided");
		return (-1);
	}

	/* Optional arguments given? */
	if (shared_secret != NULL)
		argc++;

	if (trace_packets)
		argc++;
 
	argv = calloc(argc, sizeof argv);

	if (argv != NULL) {
		log_crt("Could not allocate memory for arguments");
		return (-1);
	}

	mutex_lock(l_mutex);

	/* Executable */
	argv[vslot++] = strdup(prf_getvalue(PRF_EXE));

	memzero(scratch, sizeof scratch);
	r = snprintf(scratch, sizeof scratch, "--log-min-severity=%s", prf_getvalue(PRF_LL));
	if (!snprintfok(r, sizeof scratch)) {
		mutex_unlock(l_mutex);
		log_crt("Could not store log severity");
		return (-1);
	}
	
	argv[vslot++] = strdup(scratch);
	argv[vslot++] = strdup("chop");
	argv[vslot++] = strdup("socks");
	argv[vslot++] = strdup("--persist-mode");

	if (trace_packets) { 
		argv[vslot++] = strdup("--trace-packets");
	}

	if (shared_secret != NULL && (strcmp(shared_secret, "") != 0)){
		memzero(scratch, sizeof scratch);
		snprintf(scratch, sizeof scratch, "--shared-secret=%s", prf_getvalue(PRF_SHS));
		argv[vslot++] = strdup(scratch);
	}

	argv[vslot++] = strdup(prf_getvalue(PRF_PA));

	for (i = 0; i < circuits; i++) {
		argv[vslot++] = strdup(prf_getvalue(PRF_JA));  
		argv[vslot++] = strdup(prf_getvalue(PRF_SM));
	}

	argv[vslot++] = NULL;

	*argvp = argv;

	mutex_unlock(l_mutex);

	return (argc);
}

void
prf_free_argv(unsigned int argc, char **argv) {
	unsigned int i;

	for (i = 0; i < argc; i++) {
		free(argv[i]);
	}

	free(argv);
}

/* XXX uses strdup */
static unsigned int
prf_parse_preferences(void);
static unsigned int
prf_parse_preferences(void) {
	json_error_t	jerr;
	json_t		*root;
	json_t		*valueobj;
	const char	*key;
	char		*oldval;
	unsigned int	i;

	log_dbg("...");

	if (l_current_preferences == NULL){
		log_err("No current preferences");
		return (1);
	}

	root = json_loads(l_current_preferences, 0, &jerr);
	if (root == NULL) {
		log_err("Could not JSON load preferences");
		return (2);
	} 

	if (!json_is_object(root)){
		json_decref(root);
		log_err("JSON Root is not a JSON Object");
		return (3);
	}

	for (i = 0; i < PRF_MAX; i++) {
		key = l_keys[i];

		valueobj = json_object_get(root, key);

		if (!json_is_string(valueobj)) { 
			continue;
		} else {
			oldval = l_values[i];

			if (oldval != NULL) {
				free(oldval);
			}

			l_values[i] = strdup(json_string_value(valueobj));
		}
	}

	json_decref(root);

	/* All okay */
	return (0);
}

void
prf_dump(void);
void
prf_dump(void) {
	unsigned int i;

	for (i = 0; i < PRF_MAX; i++) {
		if (l_keys[i] == NULL)
			continue;

		fprintf(stderr, "%s => %s\n", l_keys[i], l_values[i]);
	}
}

void
prf_handle(httpsrv_client_t *hcl) {
	if (hcl->readbody == NULL) {
		if (httpsrv_readbody_alloc(hcl, 0, 0) < 0) {
			log_wrn("httpsrv_readbody_alloc() failed");
		}

		/* Let httpsrv read */
		return;
	}

	log_inf("prefs = %s", hcl->readbody);

	mutex_lock(l_mutex);

	if (l_current_preferences != NULL)
		free(l_current_preferences); 

	l_current_preferences = (hcl->readbody == NULL ? NULL : strdup(hcl->readbody));
	if (prf_parse_preferences()) {
#if 0
		/* this block is just for testing */
		unsigned int	argc = 0, i;
		char		**argv = NULL;

		prf_dump();

		argc = prf_get_argv(&argv);
		log_wrn("argc = %u", argc);
		for (i = 0; i < argc; i++) {
			fprintf(stderr, "argv[%u] = %s\n", i, argv[i]);
		}
#endif
		djb_result(hcl, "Preferences OK");
	} else {
		djb_result(hcl, "Preferences broken");
	}

	mutex_unlock(l_mutex);
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

