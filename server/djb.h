#ifndef SHARED_H
#define SHARED_H 1

#if __llvm__
// Workaround DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

/* LibFUtil - FUnctions and UTILities */
#include <libfutil/httpsrv.h>
#include <libfutil/list.h>

/* Jansson - the JSON parser */
#include <jansson.h>

/* DJB provided functions */
void djb_httpanswer(httpsrv_client_t *hcl, unsigned int code, const char *msg, const char *ctype);
void djb_error(httpsrv_client_t *hcl, unsigned int errcode, const char *msg);
void djb_result(httpsrv_client_t *hcl, const char *msg);

bool djb_proxy_add(httpsrv_client_t *hcl);

/* ACS API */
bool acs_handle(httpsrv_client_t *hcl);
void acs_set_net(json_t *net_);

/* Rendezvous API */
void rdv_handle(httpsrv_client_t *hcl);

/* Preferences API */
void prf_handle(httpsrv_client_t *hcl);

#endif /* SHARED_H */
