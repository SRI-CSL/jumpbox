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


static char password[DEFIANT_REQ_REP_PASSWORD_LENGTH + 1];

static onion_t current_onion = NULL;
static size_t current_onion_size = 0;
//reset will unlink these 
static char* current_image_path = NULL;
static char* current_image_dir = NULL;

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

static void reset(httpsrv_client_t* hcl) {
  memset(password, 0, DEFIANT_REQ_REP_PASSWORD_LENGTH + 1);
  if(current_onion != NULL){
    free_onion(current_onion);
    current_onion = NULL;
    current_onion_size = 0;
  } 
  if(current_image_path != NULL){
    unlink(current_image_path);
    unlink(current_image_dir);
    free(current_image_path);
    free(current_image_dir);
    current_image_path = NULL;
    current_image_dir = NULL;
  }

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
  if (hcl->headers.content_length < 10) {
    djb_error(hcl, 500, "POST body too puny");
    return 1;
  }
  
  if (hcl->headers.content_length >= (5*1024*1024)) {
    djb_error(hcl, 500, "POST body too big");
    return 2;
  }
  logline(log_DEBUG_, "djb_allocreadbody: asking for the body");
  /* Let the HTTP engine read the body in here */
  hcl->readbody = mcalloc(hcl->headers.content_length, "HTTPBODY");
  hcl->readbodylen = hcl->headers.content_length;
  hcl->readbodyoff = 0;
  logline(log_DEBUG_, "djb_allocreadbody: alloced %d bytes\n", (int)hcl->headers.content_length);
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

static char *make_image_reponse(char *path, int onion_type){
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
        response = make_image_reponse(image_path, ONION_TYPE(onion)); 
        if(response != NULL){
          logline(log_DEBUG_, "image: reponse %s", response);
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

static char *make_peel_reponse(const char* info, const char* additional, const char* status){
  int onion_type = ONION_TYPE(current_onion);
  char* retval = NULL;
  char* response = NULL;
  int chars = 0, response_size = 0;
  while (1) {
    chars = snprintf(response, response_size,
                     "{ \"info\": \"%s\",  \"additional\": \"%s\", \"status\": \"%s\", \"onion_type\": %d}",
                     info, additional, status, onion_type);
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
  response = make_peel_reponse("ok", "ok", "ok");
  return response;
}

static char *peel_pow(void) {
  char *response = NULL;
  response = make_peel_reponse("ok", "ok", "ok");
  return response;
}

static char *peel_captcha(void) {
  char *response = NULL;
  response = make_peel_reponse("ok", "ok", "ok");
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
      return make_peel_reponse("", "", "The server returned an onion whose signature we VERIFIED!");
    } else {
      return make_peel_reponse("", "Peeling it went wrong, very odd.", "");
    }
  } else {
    return make_peel_reponse("", "The server returned an onion whose signature we COULD NOT verify -- try again?", "");
  }
}


static void peel(httpsrv_client_t * hcl) {
  char *response = NULL;
  if((current_onion == NULL) || !ONION_IS_ONION(current_onion)) {
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
      response = peel_captcha();
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
      djb_error(hcl, 500, "make_peel_reponse failed");
    }
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


