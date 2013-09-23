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
void prf_handle(httpsrv_client_t *hcl);

/* set the address from the dance; address is copied */
void prf_set_stegotorus_server(char *address, int port);

/* get the stegotorus argc & argv for a call to exevp from the preferences.
   Linda points out the we also need to provide the location of the traces
   directory.

   Usage:
   
   int argc;
   char** argv = NULL;

   argc = prf_get_argv(&argv);

   if(argc > 0){
   ...
   }

*/
int prf_get_argv(char*** argvp);
char* prf_get_traces_dir(void);


#endif /* SHARED_H */
