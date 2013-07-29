#include "rendezvous.h"
#include "shared.h"

#define DECLIENT

#ifdef DECLIENT
#include "defiantclient.h"
static char password[DEFIANT_REQ_REP_PASSWORD_LENGTH + 1];
#endif


// fake everything, till the real thing comes along (and POSTs have accessible data).
// static char server[] = "vm06.csl.sri.com";
// static char secret[] = "U4Sv7k2PY0Gq7TFi";
static char freedom_request[] = "http://vm06.csl.sri.com/photos/26907150@N08/1457660969/lightbox?_utma=ACbLoAX643zHB8Bqb5MtfZWLUdfMZDUvH9hthuYoM96yRlIIBtPY1ns1kfEh72EFYUOr0sxHuBs2PiQ4WJf9RVNqCUaaDJabwIv8S5g8Ld1zNhoB4lc8QuqjqjVmk98P9qNJbOBpLQLHTU5Jo1f6koLiS1diUEEpTXRVsWzDKHsA&_utmz=Gu8jdzMURgtpNVnP6odSZVGKBEE=";


static void respond(httpsrv_client_t *hcl, unsigned int errcode, const char *api, const char *msg) {
	conn_addheaderf(&hcl->conn, "HTTP/1.1 %u %s\r\n", errcode, api);
	conn_printf(&hcl->conn, "%s", msg);
	httpsrv_done(hcl);
}

static void reset(httpsrv_client_t* hcl) {
#ifdef DECLIENT
  memset(password, 0, DEFIANT_REQ_REP_PASSWORD_LENGTH + 1);
#endif
  respond(hcl, 200, "reset", "Reset OK");
}

static void gen_request(httpsrv_client_t* hcl) {
  respond(hcl, 200, "gen_request", freedom_request);
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
