#include "djb.h"

#include "defiantclient.h"
#include "defiantbf.h"
#include "defiant_params.h"
#include "defiantrequest.h"
#include "defianterrors.h"

#include "onion.h"
#include "outguess.h"

#include <openssl/sha.h>

#define KBYTE 1024

static char l_password[DEFIANT_REQ_REP_PASSWORD_LENGTH + 1];

static onion_t	l_current_onion = NULL;
static uint64_t l_current_onion_size = 0;
/* reset will unlink these */
static char	*l_current_image_path = NULL;
static char	*l_current_image_dir = NULL;
/* captcha goes in the same directory */
static char	*l_captcha_image_path = NULL;

/* pow thread */
static int	l_pow_thread_started = 0;
static int	l_pow_thread_finished = 0;
static int	l_pow_thread_quit = 0;
static onion_t	l_pow_inner_onion = NULL;
static volatile long l_pow_thread_progress = 0;

static char *
rdv_randompath(void);
static char *
rdv_randompath(void) {
	char		*retval = (char *)calloc(1024, sizeof(char));
	uint64_t	r = generate_random_number();

	//look like flickr for today:
	snprintf(retval, 1024, "photos/26907150@N08/%" PRIu64 "/lightbox", r);
	return (retval);
}

static void
rdv_onion_reset(void);
static void
rdv_onion_reset(void) {
	logline(log_DEBUG_, "...");

	memset(l_password, 0, DEFIANT_REQ_REP_PASSWORD_LENGTH + 1);

	if (l_current_onion != NULL ){
		free_onion(l_current_onion);
		l_current_onion = NULL;
		l_current_onion_size = 0;
	}
}

static void
rdv_pow_reset(void);
static void
rdv_pow_reset(void) {
	logline(log_DEBUG_, "...");

	if (l_pow_thread_started) {
		onion_t inner;

		l_pow_thread_quit = 1;
		inner = l_pow_inner_onion;

		if (inner != NULL) {
			l_pow_inner_onion = NULL;
			free_onion(inner);
		}

		l_pow_thread_started = 0;
		l_pow_thread_finished = 0;
		l_pow_thread_progress = 0;
	}
}

static void
rdv_captcha_reset(void);
static void
rdv_captcha_reset(void) {
	logline(log_DEBUG_, "...");

	if (l_captcha_image_path != NULL) {
		if (unlink(l_captcha_image_path) == -1) {
			logline(log_DEBUG_,
				"unlink(%s) failed: %s",
				l_captcha_image_path, strerror(errno));
		}

		free(l_captcha_image_path);
		l_captcha_image_path = NULL;
	}
}

static void
rdv_image_reset(void);
static void
rdv_image_reset(void) {
	logline(log_DEBUG_, "...");

	if (l_current_image_path != NULL) {
		if (unlink(l_current_image_path) == -1){
			logline(log_WARNING_,
				"unlink(%s) failed: %s",
				l_current_image_path, strerror(errno));
		};

		if (rmdir(l_current_image_dir) == -1){
			logline(log_WARNING_,
				"rmdir(%s) failed: %s",
				l_current_image_dir, strerror(errno));
		};

		free(l_current_image_path);
		free(l_current_image_dir);
		l_current_image_path = NULL;
		l_current_image_dir = NULL;
	}
}

static void
rdv_reset(httpsrv_client_t* hcl);
static void
rdv_reset(httpsrv_client_t* hcl) {
	rdv_onion_reset();
	rdv_captcha_reset();
	rdv_image_reset();
	rdv_pow_reset();

	djb_result(hcl, "Reset OK");
}

static void
rdv_gen_request_aux(httpsrv_client_t* hcl, char* server, bool secure);
static void
rdv_gen_request_aux(httpsrv_client_t* hcl, char* server, bool secure) {
	char		*path = NULL, *request = NULL;
	bf_params_t	*params =  NULL;

	int defcode = bf_char64_to_params(defiant_params_P, defiant_params_Ppub, &params);
	if (defcode == DEFIANT_OK) {
		randomPasswordEx(l_password,
				 DEFIANT_REQ_REP_PASSWORD_LENGTH + 1, 0);
		path = rdv_randompath();

		if (secure) {
			defcode = generate_defiant_ssl_request_url(
				  params, l_password, server, path, &request);
		} else {
			defcode = generate_defiant_request_url(
				  params, l_password, server, path, &request);
		}
	}

	if (defcode == DEFIANT_OK) {
		djb_result(hcl, request);

		logline(log_DEBUG_,
			"secure=%s, password=%s, request=%s",
			yesno(secure),
			l_password, request);
	} else {
		djb_error(hcl, 500, defiant_strerror(defcode));
	}

	bf_free_params(params);

	if (path) {
		free(path);
	}

	if (request) {
		free(request);
	}
}

static void
rdv_gen_request(httpsrv_client_t* hcl);
static void
rdv_gen_request(httpsrv_client_t* hcl) {
	json_error_t	error;
	json_t		*root;
	char		*server = NULL;
	bool		secure = false;

	/* No body yet? Then allocate some memory to get it */
	if (hcl->readbody == NULL) {
		if (httpsrv_readbody_alloc(hcl, 2, 0) < 0){
			logline(log_DEBUG_,
				"httpsrv_readbody_alloc() failed");
		}

		return;
	}

	logline(log_DEBUG_, "data: %s", hcl->readbody);

	root = json_loads(hcl->readbody, 0, &error);
	httpsrv_readbody_free(hcl);

	if (root != NULL && json_is_object(root)) {
		json_t *server_val, *secure_val;

		secure_val = json_object_get(root, "secure");

		if (secure_val != NULL && json_is_true(secure_val)) {
			secure = true;
		}

		server_val = json_object_get(root, "server");

		if (json_is_string(server_val)){
			server = (char *)json_string_value(server_val);
		}
	}

	if (server != NULL) {
		rdv_gen_request_aux(hcl, server, secure);
	} else {
		djb_error(hcl, 500, "POST data conundrum");

		if (root == NULL) {
			logline(log_DEBUG_,
				"data: %s, error: line: %u, msg: %s",
				hcl->readbody, error.line, error.text);
		}
	}

	json_decref(root);
}

static char *
rdv_make_image_response(char *path, int onion_type);
static char *
rdv_make_image_response(char *path, int onion_type) {
	char	*retval = NULL;
	char	*response = NULL;
	int	chars = 0, response_size = 0;

	while (1) {
		chars = snprintf(response, response_size,
				"{ \"image\": \"file://%s\", \"onion_type\": %u}",
				path, onion_type);

		if (response_size != 0 && chars > response_size) {
			break;
		} else if (response_size >= chars) {
			retval = response;
			break;
		} else if (response_size < chars) {
			response_size = chars + 1;
			response = (char *)calloc(response_size, sizeof(char));
		}
	}

	return (retval);
}


static void
rdv_image(httpsrv_client_t*  hcl);
static void
rdv_image(httpsrv_client_t*  hcl) {
	char	*response = NULL,
		*image_path = NULL,
		*image_dir = NULL,
		*encrypted_onion = NULL,
		*onion = NULL;
	size_t	encrypted_onion_sz = 0;
	int	retcode = DEFIANT_OK;

	logline(log_DEBUG_, "readbody: %s", yesno(hcl->readbody == NULL));

	/* No body yet? Then allocate some memory to get it */
	if (hcl->readbody == NULL) {
		if(httpsrv_readbody_alloc(hcl, 0, 0) < 0){
			logline(log_DEBUG_, "httpsrv_readbody_alloc() failed");
		}

		return;
	}

	retcode = extract_n_save(l_password, hcl->readbody, hcl->readbody_off,
				 &encrypted_onion, &encrypted_onion_sz,
				 &image_path, &image_dir);

	httpsrv_readbody_free(hcl);

	if (retcode != DEFIANT_OK){
		logline(log_DEBUG_,
			"extract_n_save() with password=%s returned %d -- %s",
			l_password, retcode, defiant_strerror(retcode));

		djb_error(hcl, 500,
			 "extract_n_save() failure, see log)");

	} else {
		int onion_sz = 0;
		onion = (onion_t)defiant_pwd_decrypt(l_password,
				(const uchar *)encrypted_onion,
				encrypted_onion_sz, &onion_sz);

		if (onion == NULL){
			logline(log_DEBUG_,
				"Decrypting onion failed: No onion");

		} else if (onion_sz < (int)sizeof(onion_header_t)) {
			logline(log_DEBUG_,
				"Decrypting onion failed: onion_sz less "
				"than onion header");

		} else if (!ONION_IS_ONION(onion)) {
			logline(log_DEBUG_,
				"Decrypting onion failed: Onion Magic Incorrect");

		} else if (onion_sz != (int)ONION_SIZE(onion)) {
			logline(log_DEBUG_,
				"Decrypting onion failed: onion_sz (%u) "
				"does nat match real onion sizeu(%d)",
				onion_sz, (int)ONION_SIZE(onion));
		} else {
			response = rdv_make_image_response(image_path,
				    ONION_TYPE(onion));

			if (response != NULL) {
				logline(log_DEBUG_, "response %s", response);

				l_current_onion = (onion_t)onion;
				l_current_onion_size = onion_sz;
				l_current_image_path = image_path;
				l_current_image_dir = image_dir;

				logline(log_DEBUG_,
					"onion_sz %" PRIu64 " -- "
					"onion_type: %u",
					l_current_onion_size,
					ONION_TYPE(l_current_onion));

				djb_result(hcl, response);
			}
		}
	}

	if (response == NULL) {
		/* these need to be reclaimed when things go wrong */
		if (image_path != NULL) {
			unlink(image_path);
			unlink(image_dir);
			free(image_path);
			free(image_dir);
		}

		free(onion);
		djb_error(hcl, 500, "server error");
	} else {
		free(response);
	}

	/* rain or shine these can get tossed */
	if (encrypted_onion != NULL) {
		free(encrypted_onion);
	}
}

static char *
rdv_make_peel_response(const char* info, const char* status);
static char *
rdv_make_peel_response(const char* info, const char* status) {
	int	onion_type = ONION_TYPE(l_current_onion);
	char	*retval = NULL;
	char	*response = NULL;
	int	chars = 0, response_size = 0;

	while (1) {
		chars = snprintf(response, response_size,
				"{ \"info\": \"%s\",  \"status\": \"%s\", \"onion_type\": %d}",
				info, status, onion_type);

		if (response_size != 0 && chars > response_size) {
			break;
		} else if (response_size >= chars) {
			retval = response;
			break;
		} else if (response_size < chars) {
			response_size = chars + 1;
			response = (char *)calloc(response_size, sizeof(char));
		}
	}

	return (retval);
}

static char *
rdv_make_pow_response(unsigned int percent, const char *status);
static char *
rdv_make_pow_response(unsigned int percent, const char *status) {
	int	onion_type = ONION_TYPE(l_current_onion);
	char	*retval = NULL;
	char	*response = NULL;
	int	chars = 0, response_size = 0;

	while (1) {
		chars = snprintf(response, response_size,
				"{ \"info\": %u,  \"status\": \"%s\", \"onion_type\": %d}",
				percent, status, onion_type);

		if (response_size != 0 && chars > response_size) {
			break;
		} else if (response_size >= chars) {
			retval = response;
			break;
		} else if (response_size < chars) {
			response_size = chars + 1;
			response = (char *)calloc(response_size, sizeof(char));
		}
	}

	return (retval);
}

static char *
rdv_peel_base(void);
static char *
rdv_peel_base(void) {
	char		*response = NULL;
	const char	*nep;
	json_error_t	error;
	json_t		*root, *resp;

	nep = (char*)ONION_DATA(l_current_onion);

	logline(log_DEBUG_, "nep = %s", nep);

	root = json_loads(nep, 0, &error);
	if (root != NULL) {
		resp = json_pack("{s:i, s:s, s:o}",
				 "onion_type", ONION_TYPE(l_current_onion),
				 "status", "Here is your NET!",
				 "info", root);

		response = json_dumps(resp, 0);

		/* Pass the NET to ACS so that it can Dance */
		acs_set_net(root);
	} else {
		logline(log_DEBUG_,
			"data = %s error: line: %d msg: %s",
			 nep, error.line, error.text);
		response = rdv_make_peel_response(
			"Sorry your nep did not parse as JSON", "");
	}

	return (response);
}

/* Show progress */
#undef DRIVEL

/* we are currently forcing passwords to start with "aaa" */
static const long maxAttempts = 1 * 1 * 1 * 26 * 26 * 26 * 26 * 26;

static void *
rdv_pow_worker(void UNUSED *arg);
static void *
rdv_pow_worker(void UNUSED *arg) {
	size_t	puzzle_size = ONION_PUZZLE_SIZE(l_current_onion);
	char	*hash = ONION_PUZZLE(l_current_onion);
	int	hash_len = SHA_DIGEST_LENGTH;
	char	*secret = hash + SHA_DIGEST_LENGTH;
	int	secret_len = puzzle_size - SHA_DIGEST_LENGTH;
	char	*data = ONION_DATA(l_current_onion);
	size_t	data_len = ONION_DATA_SIZE(l_current_onion);

	l_pow_inner_onion = defiant_pow_aux((uchar*)hash, hash_len,
					   (uchar*)secret, secret_len,
					   (uchar*)data, data_len,
					   &l_pow_thread_progress);

	logline(log_DEBUG_,
		"pow_inner_onion = %p : %s %s",
		l_pow_inner_onion, (char *)l_pow_inner_onion,
		(char *)ONION_DATA(l_pow_inner_onion));

	l_pow_thread_progress = maxAttempts;
	l_pow_thread_finished = 1;

	return (NULL);
}


static unsigned int
rdv_attempts2percent(void);
static unsigned int
rdv_attempts2percent(void) {
	unsigned int	retval = 0;
	unsigned long	current = l_pow_thread_progress;

	retval = ((current * 100) / maxAttempts);

#ifdef DRIVEL
	logline(log_DEBUG_, "%lu %u%%\n", current, retval);
#endif
	return (retval);
}

static char *
rdv_peel_pow(void);
static char *
rdv_peel_pow(void) {
	char		*response = NULL;
	unsigned int	pc = rdv_attempts2percent();

	if (l_pow_thread_started == 0) {
		/* Start the POW thread */
		if (thread_add("RendezvousPOW", rdv_pow_worker, NULL)) {
			l_pow_thread_started = 1;
			response = rdv_make_pow_response(pc,
				"OK the Proof-Of-Work has commenced");
		} else {
			response = rdv_make_peel_response("",
				"Creating the Proof-Of-Work failed :-(");
		}

	} else {
		/*
		 * Monitor the progress of the thread;
		 * or do the current <--> inner switch
		 */
		if (l_pow_thread_finished == 0) {
			response = rdv_make_pow_response(pc,
				   "Working away...");
		} else {
			if (l_pow_inner_onion == NULL){
				response = rdv_make_peel_response("",
					"Proof of work FAILED?!?");
			} else {
				onion_t old_onion = l_current_onion;

				l_current_onion = l_pow_inner_onion;
				free_onion(old_onion);

				response = rdv_make_pow_response(100,
					"Your Proof-Of-Work has "
					"finished successfully!");

				l_pow_inner_onion = NULL;
				rdv_pow_reset();
			}
		}
	}

	return (response);
}

static char *
rdv_peel_captcha_no_image_path(void);
static char *
rdv_peel_captcha_no_image_path(void) {
	/* need to make it */
	int retcode = DEFIANT_DATA;
	char path[KBYTE];

	fassert(l_current_image_dir != NULL);

	snprintf(path, sizeof path,
		 "%s" PATH_SEPARATOR "captcha.png",
		l_current_image_dir);

	retcode = bytes2file(path,
			     ONION_PUZZLE_SIZE(l_current_onion),
			     ONION_PUZZLE(l_current_onion));

	if (retcode != DEFIANT_OK) {
		logline(log_DEBUG_,
			"Could not store captcha to %s: %s",
			path, defiant_strerror(retcode));
		return (NULL);
	}

	l_captcha_image_path = strdup(path);

	logline(log_DEBUG_,
		"Captcha image = %s",
		l_captcha_image_path);

	snprintf(path, sizeof path,
		"file://%s",
		l_captcha_image_path);

	return (rdv_make_peel_response(path, "Here is your captcha image!"));
}

static char *
rdv_peel_captcha_with_image_path(json_t *root);
static char *
rdv_peel_captcha_with_image_path(json_t *root) {
	json_t		*answer_val;
	const char	*r;

	/* Do they have an answer? */
	answer_val = json_object_get(root, "action");

	if (json_is_string(answer_val)) {
		char	*answer;
		onion_t	inner_onion = NULL;
		int	defcode;

		answer = (char *)json_string_value(answer_val);

		defcode = peel_captcha_onion(answer, l_current_onion,
					     &inner_onion);

		if (defcode == DEFIANT_OK){
			free(l_current_onion);

			l_current_onion = inner_onion;

			r = "Excellent, you solved the captcha";
		} else {
			r = "Nope, try again?";
		}
	} else {
		r = "Answer wasn't of the right type";
	}

	return (rdv_make_peel_response("", r));
}

static char *
rdv_peel_captcha(json_t *root);
static char *
rdv_peel_captcha(json_t *root) {
	return (l_captcha_image_path == NULL ?
		rdv_peel_captcha_no_image_path() :
		rdv_peel_captcha_with_image_path(root));
}

static char *
rdv_peel_signed(void);
static char *
rdv_peel_signed(void) {
	int 		errcode;
	const char	*r;

	errcode = verify_onion(l_current_onion);

	if (errcode == DEFIANT_OK){
		onion_t inner_onion = NULL;
		errcode = peel_signed_onion(l_current_onion, &inner_onion);

		if (errcode == DEFIANT_OK){
			free_onion(l_current_onion);
			l_current_onion = inner_onion;
			l_current_onion_size = ONION_SIZE(inner_onion);
			r = "The server returned an onion whose "
			    "signature we VERIFIED!";
		} else {
			r = "Peeling it went wrong, very odd.";
		}
	} else {
		r = "The server returned an onion whose signature "
		    "we COULD NOT verify -- try again?";
	}

	return (rdv_make_peel_response("", r));
}

static void
rdv_peel(httpsrv_client_t *hcl);
static void
rdv_peel(httpsrv_client_t *hcl) {
	char		*response = NULL;
	json_error_t	error;
	json_t		*root;
	int		otype;

	/* No body yet? Then allocate some memory to get it */
	if (hcl->readbody == NULL) {
		if (httpsrv_readbody_alloc(hcl, 0, 0) < 0){
			logline(log_DEBUG_, "httpsrv_readbody_alloc() failed");
		}
		return;
	}

	logline(log_DEBUG_, "readbody: %s", hcl->readbody);
	root = json_loads(hcl->readbody, 0, &error);
	httpsrv_readbody_free(hcl);

	if (root == NULL) {
		logline(log_DEBUG_,
			"libjansson error: line: %u msg: %s",
			error.line, error.text);
		djb_error(hcl, 500, "Bad JSON");

	} else if ((l_current_onion == NULL) || !ONION_IS_ONION(l_current_onion)) {
		djb_error(hcl, 500, "Bad onion");

	} else {
		otype = ONION_TYPE(l_current_onion);

		switch(otype){
		case BASE:
			response = rdv_peel_base();
			break;

		case POW:
			response = rdv_peel_pow();
			break;

		case CAPTCHA:
			response = rdv_peel_captcha(root);
			break;

		case SIGNED:
			response = rdv_peel_signed();
			break;

		case COLLECTION:
		default:
			djb_error(hcl, 500,
				 "Onion method not implemented yet");
			break;
		}

		if (response != NULL){
			djb_result(hcl, response);
			free(response);
			response = NULL;

		} else {
			djb_error(hcl, 500, "Peeling failed");
		}
	}

	json_decref(root);
}

static void
rdv_dance(httpsrv_client_t* hcl);
static void
rdv_dance(httpsrv_client_t* hcl) {
	djb_error(hcl, 500, "Dancing not implemented yet");
}

/* Called from djb */
void
rdv_handle(httpsrv_client_t *hcl) {
	const char	*query;

	/* Skip '/rendezvous/' (12) */
	query = &(hcl->headers.uri[12]);

	logline(log_DEBUG_, "query = %s", query);

	if (strcasecmp(query, "reset") == 0) {
		rdv_reset(hcl);

	} else if (strcasecmp(query, "gen_request") == 0) {
		rdv_gen_request(hcl);

	} else if (strcasecmp(query, "image") == 0) {
		rdv_image(hcl);

	} else if (strcasecmp(query, "peel") == 0) {
		rdv_peel(hcl);

	} else if (strcasecmp(query, "dance") == 0) {
		rdv_dance(hcl);

	} else {
		djb_error(hcl, 500, "No such DJB API request (Rendezvous)");
	}
}

