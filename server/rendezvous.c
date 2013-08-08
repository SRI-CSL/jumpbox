#include "rendezvous.h"
#include "shared.h"

#include "defiantclient.h"
#include "defiantbf.h"
#include "defiant_params.h"
#include "defiantrequest.h"
#include "defianterrors.h"

#include "onion.h"
#include "outguess.h"

#include <jansson.h>

#include <openssl/sha.h>

#define KBYTE 1024

static char password[DEFIANT_REQ_REP_PASSWORD_LENGTH + 1];

static onion_t current_onion = NULL;
static size_t current_onion_size = 0;
//reset will unlink these 
static char* current_image_path = NULL;
static char* current_image_dir = NULL;
//captcha goes in the same directory
static char* captcha_image_path = NULL;

//pow thread
static int pow_thread_started = 0;
static int pow_thread_finished = 0;
static int pow_thread_quit = 0;
static pthread_t pow_thread;
static onion_t pow_inner_onion = NULL;
static volatile long pow_thread_progress = 0;



/* put this next to djb_freereadbody when the dust settles */
static int djb_allocreadbody(httpsrv_client_t *hcl);

static char* randomPath(void){
  char *retval = (char *)calloc(1024, sizeof(char));
  int r = rand();
  //look like flickr for today:
  snprintf(retval, 1024, "photos/26907150@N08/%d/lightbox", r);
  return retval;
}


static void respond(httpsrv_client_t *hcl, unsigned int errcode, const char *api, const char *msg) {
  conn_addheaderf(&hcl->conn, "HTTP/1.1 %u %s\r\n", errcode, api);
  conn_addheaderf(&hcl->conn, "Content-Type: application/json\r\n");

  conn_printf(&hcl->conn, "%s", msg);
  httpsrv_done(hcl);
}

static void onion_reset(void);
void onion_reset(void){
  logline(log_DEBUG_, "onion_reset");
  memset(password, 0, DEFIANT_REQ_REP_PASSWORD_LENGTH + 1);
  if(current_onion != NULL){
    free_onion(current_onion);
    current_onion = NULL;
    current_onion_size = 0;
  } 
}

static void pow_reset(void);
void pow_reset(void){
  logline(log_DEBUG_, "pow_reset");
  if(pow_thread_started){ 
    pow_thread_quit = 1;
    onion_t inner = pow_inner_onion;
    if(inner != NULL){
      pow_inner_onion = NULL;
      free_onion(inner);
    }
    pow_thread_started = 0;
    pow_thread_finished = 0;
    pow_thread_progress = 0;
  }
}

static void captcha_reset(void);
void captcha_reset(void){
  logline(log_DEBUG_, "captcha_reset");
  if(captcha_image_path != NULL){
    if(unlink(captcha_image_path) == -1){
      logline(log_DEBUG_, "unlink(%s) failed: %s", captcha_image_path, strerror(errno));
    };
    free(captcha_image_path);
    captcha_image_path = NULL;
  }
}

static void image_reset(void);
void image_reset(void){
  logline(log_DEBUG_, "image_reset");
  
  if(current_image_path != NULL){
    if(unlink(current_image_path) == -1){
      logline(log_DEBUG_, "unlink(%s) failed: %s", current_image_path, strerror(errno));
    };
    if(rmdir(current_image_dir) == -1){
      logline(log_DEBUG_, "rmdir(%s) failed: %s", current_image_dir, strerror(errno));
    };
    free(current_image_path);
    free(current_image_dir);
    current_image_path = NULL;
    current_image_dir = NULL;
  }
}

static void reset(httpsrv_client_t* hcl) {
  onion_reset();
  captcha_reset();
  image_reset();
  pow_reset(); 
  respond(hcl, 200, "reset", "Reset OK");
}

static void gen_request_aux(httpsrv_client_t* hcl, char* server, int secure){
  char *path = NULL, *request = NULL;
  bf_params_t *params =  NULL;
  int defcode = bf_char64_to_params(defiant_params_P, defiant_params_Ppub, &params);
  if(defcode == DEFIANT_OK){
    randomPasswordEx(password, DEFIANT_REQ_REP_PASSWORD_LENGTH + 1, 0);
    path = randomPath();
    if(secure){
      defcode = generate_defiant_ssl_request_url(params, password, server, path, &request);
    } else {
      defcode = generate_defiant_request_url(params, password, server, path, &request);
    }
  }
  if(defcode == DEFIANT_OK){
    respond(hcl, 200, "gen_request", request);
    logline(log_DEBUG_, "gen_request: secure=%s, password = %s, request = %s",
	    secure ? "yes" : "no",
	    password, request);
  } else {
    djb_error(hcl, 500, defiant_strerror(defcode));
  }
  bf_free_params(params);
  free(path);
  free(request);
}


int djb_allocreadbody(httpsrv_client_t *hcl){
  /* Ian says:  {} is fine as a body for me. */
  if (hcl->headers.content_length < 2) {
    djb_error(hcl, 500, "POST body too puny");
    return 1;
  }
  
  if (hcl->headers.content_length >= (5*1024*1024)) {
    djb_error(hcl, 500, "POST body too big");
    return 2;
  }
  logline(log_DEBUG_, "djb_allocreadbody: asking for the body");
  /* Let the HTTP engine read the body in here */
  hcl->readbody = mcalloc(hcl->headers.content_length + 1, "HTTPBODY");  //keep some room for the NULL (keep VALGRIND happy)
  hcl->readbodylen = hcl->headers.content_length;
  hcl->readbodyoff = 0;
  hcl->readbody[hcl->headers.content_length] = '\0';
  logline(log_DEBUG_, "djb_allocreadbody: alloced %" PRIu64 " bytes\n", hcl->headers.content_length + 1);
  return 0;
}

static void gen_request(httpsrv_client_t* hcl) {
  /* No body yet? Then allocate some memory to get it */
  if (hcl->readbody == NULL) {
    if(djb_allocreadbody(hcl)){
      logline(log_DEBUG_, "gen_request: djb_allocreadbody crapped out");
    }
    return;
  } else {
    json_error_t error;
    json_t *root;
    char* server = NULL;
    int secure = 0;
    logline(log_DEBUG_, "gen_request: %s", hcl->readbody);
    root = json_loads(hcl->readbody, 0, &error);
    djb_freereadbody(hcl);
    if(root != NULL && json_is_object(root)){
      json_t *server_val, *secure_val;
      secure_val = json_object_get(root, "secure");
      if(secure_val != NULL && json_is_true(secure_val)){ secure = 1; }
      server_val = json_object_get(root, "server");
      if(json_is_string(server_val)){
        server = (char *)json_string_value(server_val);
      }
    }
    if(server != NULL){
      gen_request_aux(hcl, server, secure);
    } else {
      djb_error(hcl, 500, "POST data conundrum");
      if(root == NULL){
        logline(log_DEBUG_, "gen_request: data = %s error: line: %d msg: %s", hcl->readbody, error.line, error.text);
      }
    }
    json_decref(root);
  }
}

static char *make_image_response(char *path, int onion_type){
  char* retval = NULL;
  char* response = NULL;
  int chars = 0, response_size = 0;
  while (1) {
    chars = snprintf(response, response_size,
                     "{ \"image\": \"file://%s\", \"onion_type\": %d}",
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
  return retval;
}


static void image(httpsrv_client_t*  hcl) {
  char *response = NULL, *image_path = NULL, *image_dir = NULL, *encrypted_onion = NULL,  *onion = NULL;
  logline(log_DEBUG_, "image %d", hcl->readbody == NULL);
  /* No body yet? Then allocate some memory to get it */
  if (hcl->readbody == NULL) {
    if(djb_allocreadbody(hcl)){
      logline(log_DEBUG_, "image: djb_allocreadbody crapped out");
    }
    return;
  } else {
    size_t encrypted_onion_sz = 0;
    int retcode = DEFIANT_OK;
    retcode = extract_n_save(password, hcl->readbody, hcl->readbodyoff,  &encrypted_onion, &encrypted_onion_sz, &image_path, &image_dir);
    djb_freereadbody(hcl);
    if(retcode != DEFIANT_OK){
      logline(log_DEBUG_, "image: extract_n_save with password %s returned %d -- %s", password, retcode, defiant_strerror(retcode));
      djb_error(hcl, 500, "Not implemented yet");
    } else {
      int onion_sz = 0;
      onion = (onion_t)defiant_pwd_decrypt(password, (const uchar*)encrypted_onion, (int)encrypted_onion_sz, &onion_sz); 
      if(onion == NULL){
        logline(log_DEBUG_, "image: Decrypting onion failed: No onion");
      } else if (onion_sz < (int)sizeof(onion_header_t)) {
        logline(log_DEBUG_, "image: Decrypting onion failed: onion_sz less than onion header");
      } else if (!ONION_IS_ONION(onion)) {
        logline(log_DEBUG_, "image: Decrypting onion failed: Onion Magic Incorrect");
      } else if (onion_sz != (int)ONION_SIZE(onion)) {
        logline(log_DEBUG_, "image: Decrypting onion failed: onion_sz (%d) does nat match real onion size (%d)", onion_sz, (int)ONION_SIZE(onion));
      } else {
        response = make_image_response(image_path, ONION_TYPE(onion)); 
        if(response != NULL){
          logline(log_DEBUG_, "image: response %s", response);
          current_onion = (onion_t)onion;
          current_onion_size = onion_sz;
          current_image_path = image_path;
          current_image_dir = image_dir;
          logline(log_DEBUG_, "image: onion_sz %d -- onion_type: %d", (int)current_onion_size, ONION_TYPE(current_onion));
          respond(hcl, 200, "image", response);
        }
      }
    }
  }
  if(response == NULL){
    //these need to be reclaimed when things go wrong
    if(image_path != NULL){
      unlink(image_path);
      unlink(image_dir);
      free(image_path);
      free(image_dir);
    }
    free(onion);
    djb_error(hcl, 500, "server error");
  } 
  //rain or shine these can get tossed
  free(response);
  free(encrypted_onion);
  
}

static char *make_peel_response(const char* info, const char* status){
  int onion_type = ONION_TYPE(current_onion);
  char* retval = NULL;
  char* response = NULL;
  int chars = 0, response_size = 0;
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
  return retval;
}

static char *make_pow_response(int percent, const char* status){
  int onion_type = ONION_TYPE(current_onion);
  char* retval = NULL;
  char* response = NULL;
  int chars = 0, response_size = 0;
  while (1) {
    chars = snprintf(response, response_size,
                     "{ \"info\": %d,  \"status\": \"%s\", \"onion_type\": %d}",
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
  return retval;
}

static char *peel_base(void) {
  char *response = NULL;
  char *nep = (char*)ONION_DATA(current_onion);
  json_error_t error;
  json_t *root;
  logline(log_DEBUG_, "peel_base: %s", nep);
  root = json_loads(nep, 0, &error);
  if(root != NULL){
    json_t *resp = json_pack("{s:i, s:s, s:o}", "onion_type", ONION_TYPE(current_onion), "status", "Here is your NET!", "info", root);
    response = json_dumps(resp, 0);
  } else {
    logline(log_DEBUG_, "peel_base: data = %s error: line: %d msg: %s", nep, error.line, error.text);
    response = make_peel_response("Sorry your nep did not parse as JSON", "");
  }
  return response;
}

int drivel = 0;
/* we are currently forcing passwords to start with "aaa" */
long maxAttempts = 1 * 1 * 1 * 26 * 26 * 26 * 26 * 26;

void *pow_worker(void *arg);
void *pow_worker(void *arg){
  size_t puzzle_size = ONION_PUZZLE_SIZE(current_onion);
  char* hash = ONION_PUZZLE(current_onion);
  int hash_len = SHA_DIGEST_LENGTH;
  char* secret = hash + SHA_DIGEST_LENGTH;
  int secret_len = puzzle_size - SHA_DIGEST_LENGTH;
  char* data = ONION_DATA(current_onion);
  size_t data_len = ONION_DATA_SIZE(current_onion);
  pow_inner_onion = defiant_pow_aux((uchar*)hash, hash_len, (uchar*)secret, secret_len, (uchar*)data, data_len, &pow_thread_progress);
  logline(log_DEBUG_, "pow_inner_onion = %p : %s %s", pow_inner_onion, (char *)pow_inner_onion, (char *)ONION_DATA(pow_inner_onion));
  pow_thread_progress = maxAttempts;
  pow_thread_finished = 1;
  return arg;
}


int attempts2percent(void);
int attempts2percent(void){
  int retval = 0;
  long current = pow_thread_progress;
  retval = ((current * 100)/maxAttempts);
  if(drivel){
    fprintf(stderr, "%ld %d%c\n", current, retval, '%');
  }
  return retval;
}


static char *peel_pow(void) {
  char *response = NULL;
  int pc = attempts2percent();
  if(pow_thread_started == 0){
    /* need to start the pow thread */
    int errcode = pthread_create(&pow_thread, NULL, pow_worker, NULL);
    if(errcode != 0){
      response = make_peel_response("", "Creating the Proof-Of-Work failed :-(");
    } else {
      pow_thread_started = 1;
      response = make_pow_response(pc, "OK the Proof-Of-Work has commenced");
    }
  } else {
    /* monitor the progress of the thread; or do the current <--> inner switch */
    if(pow_thread_finished == 0){
      response = make_pow_response(pc, "Working away...");
    } else {
      if(pow_inner_onion == NULL){
        response = make_peel_response("", "Proof of work FAILED?!?");
      } else {
        onion_t old_onion = current_onion;
        current_onion = pow_inner_onion;
        free_onion(old_onion);
        response = make_pow_response(100, "Your Proof-Of-Work has finished successfully!");
        pow_inner_onion = NULL;
        pow_reset();
      }
    }
  }
  return response;
}

static char *peel_captcha(json_t *root) {
  char *response = NULL;
  if(captcha_image_path == NULL){
    if(current_image_dir == NULL){
      /* shouldn't get here */
      
    } else {
      /* need to make it */
      int retcode = DEFIANT_DATA;
      char path[KBYTE];
      snprintf(path, sizeof path, "%s" PATH_SEPARATOR "captcha.png", current_image_dir);
      retcode = bytes2file(path, ONION_PUZZLE_SIZE(current_onion), ONION_PUZZLE(current_onion)); 
      if(retcode != DEFIANT_OK){ 
        logline(log_DEBUG_, "image: captcha writing failed %s", defiant_strerror(retcode));
      } else {
        captcha_image_path = strdup(path);
        logline(log_DEBUG_, "image: captcha image = %s", captcha_image_path);
        snprintf(path, sizeof path, "file://%s", captcha_image_path);
        response = make_peel_response(path, "Here is your captcha image!");
      }
    }
  } else {
    /* do they have an answer? */
    json_t *answer_val = json_object_get(root, "action");
    if(json_is_string(answer_val)){
      char *answer = (char *)json_string_value(answer_val);
      onion_t inner_onion = NULL;
      int defcode = peel_captcha_onion(answer, current_onion, &inner_onion);
      if(defcode == DEFIANT_OK){
        free(current_onion);
        current_onion = inner_onion;
        response = make_peel_response("", "Excellent, you solved the captcha");
      } else {
        response = make_peel_response("", "Nope, try again?");
      }
    } else {
      response = make_peel_response("", "Answer wasn't of the right type");
    }
  }

  return response;
}

static char *peel_signed(void) {
  int  errcode = verify_onion(current_onion);
  if(errcode == DEFIANT_OK){
    onion_t inner_onion = NULL;
    errcode = peel_signed_onion(current_onion, &inner_onion);
    if(errcode == DEFIANT_OK){
      free_onion(current_onion);
      current_onion = inner_onion;
      current_onion_size = ONION_SIZE(inner_onion);
      return make_peel_response("", "The server returned an onion whose signature we VERIFIED!");
    } else {
      return make_peel_response("", "Peeling it went wrong, very odd.");
    }
  } else {
    return make_peel_response("", "The server returned an onion whose signature we COULD NOT verify -- try again?");
  }
}


static void peel(httpsrv_client_t * hcl) {
  /* No body yet? Then allocate some memory to get it */
  if (hcl->readbody == NULL) {
    if(djb_allocreadbody(hcl)){
      logline(log_DEBUG_, "peel: djb_allocreadbody crapped out");
    }
    return;
  } else {
    char *response = NULL;
    json_error_t error;
    json_t *root;
    logline(log_DEBUG_, "peel: %s", hcl->readbody);
    root = json_loads(hcl->readbody, 0, &error);
    djb_freereadbody(hcl);
    if(root == NULL){
      logline(log_DEBUG_, "peel: libjansson error: line: %d msg: %s", error.line, error.text);
    } else if((current_onion == NULL) || !ONION_IS_ONION(current_onion)) {
      djb_error(hcl, 500, "Bad onion");
    } else {
      int otype = ONION_TYPE(current_onion);
      switch(otype){
      case BASE: { 
        response =  peel_base();
        break;
      }
      case POW: {
        response =  peel_pow();
        break;
      }
      case CAPTCHA: {
        response = peel_captcha(root);
        break;
      }
      case SIGNED: {
        response = peel_signed();
        break;
      }
      case COLLECTION:
      default:
      djb_error(hcl, 500, "Onion method not implemented yet");
      }
      if(response != NULL){
        respond(hcl, 200, "peel", response);
      } else {
        djb_error(hcl, 500, "make_peel_response failed");
      }
    }
    if(response != NULL){ free(response); }
    json_decref(root);
  }
}
   


static void dance(httpsrv_client_t* hcl) {
  djb_error(hcl, 500, "Not implemented yet");
}


void rendezvous(httpsrv_client_t *hcl) {
  size_t prefix = strlen("/rendezvous/");
  char* query = &(hcl->headers.uri[prefix]);
  
  logline(log_DEBUG_, "rendezvous: query = %s", query);
  
  if (strcasecmp(query, "reset") == 0) {

    reset(hcl);

  } else if (strcasecmp(query, "gen_request") == 0) {

    gen_request(hcl);

  } else if (strcasecmp(query, "image") == 0) {

    image(hcl);

  } else if (strcasecmp(query, "peel") == 0) {

    peel(hcl);

  } else if (strcasecmp(query, "dance") == 0) {
    
    dance(hcl);

  } else {

    djb_error(hcl, 500, "Not a DJB API request");
    return;

  }

}


