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
void djb_error(httpsrv_client_t *hcl, unsigned int errcode, const char *msg);
void djb_result(httpsrv_client_t *hcl, const char *msg);

bool djb_proxy_add(httpsrv_client_t *hcl);

/* ACS API */
bool acs_handle(httpsrv_client_t *hcl);
void acs_set_net(json_t *net_);
void acs_exit(void);

/* Rendezvous API */
void rdv_handle(httpsrv_client_t *hcl);

/* Preferences API */
void prf_init(void);
void prf_exit(void);
void prf_handle(httpsrv_client_t *hcl);
int prf_get_argv(char*** argvp);
void prf_free_argv(unsigned int argc, char **argv);

#endif /* SHARED_H */
