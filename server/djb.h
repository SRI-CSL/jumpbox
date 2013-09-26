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
void prf_init(void);
void prf_exit(void);

/* Get the stegotorus argc & argv for a call to exevp from the preferences.

   Usage:
   
   int argc;
   char** argv = NULL;

   argc = prf_get_argv(&argv);

   if(argc > 0){
   ...
   }

   for(i = 0; i < argc; i++){
     free(argv[i]);
   }
   free(argv);


*/
int prf_get_argv(char*** argvp);


#endif /* SHARED_H */
