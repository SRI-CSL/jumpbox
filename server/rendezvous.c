#include "rendezvous.h"
#include "shared.h"

#include "defiantclient.h"
#include "defiantbf.h"
#include "defiant_params.h"
#include "defiantrequest.h"
#include "defianterrors.h"

#include <jansson.h>


static char password[DEFIANT_REQ_REP_PASSWORD_LENGTH + 1];


static char* randomPath(void){
  char *retval = (char *)calloc(1024, sizeof(char));
  int r = rand();
  //look like flickr for today:
  snprintf(retval, 1024, "photos/26907150@N08/%d/lightbox", r);
  return retval;
}


static void respond(httpsrv_client_t *hcl, unsigned int errcode, const char *api, const char *msg) {
  conn_addheaderf(&hcl->conn, "HTTP/1.1 %u %s\r\n", errcode, api);
  conn_printf(&hcl->conn, "%s", msg);
  httpsrv_done(hcl);
}

static void reset(httpsrv_client_t* hcl) {
  memset(password, 0, DEFIANT_REQ_REP_PASSWORD_LENGTH + 1);
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
    logline(log_DEBUG_, "gen_request: password = %s\nrequest = %s", password, request);
  } else {
    djb_error(hcl, 500, defiant_strerror(defcode));
  }
  bf_free_params(params);
  free(path);
  free(request);
}

static void gen_request(httpsrv_client_t* hcl) {
  /* fake it till we can get the POST data from hcl */
  char text[] = "{ \"server\": \"vm06.csl.sri.com\", \"secure\": false }";
  json_error_t error;
  json_t *root = json_loads(text, 0, &error);
  char* server = NULL;
  int secure = 0;  
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
  }
  json_decref(root);
}

static void image(httpsrv_client_t*  hcl) {
  djb_error(hcl, 500, "Not implemented yet");
}

static void peel(httpsrv_client_t * hcl) {
  djb_error(hcl, 500, "Not implemented yet");
}

static void dance(httpsrv_client_t* hcl) {
  djb_error(hcl, 500, "Not implemented yet");
}




void rendezvous(httpsrv_client_t *hcl) {
  size_t prefix = strlen("/rendezvous/");
  char* query = &(hcl->headers.uri[prefix]);

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

