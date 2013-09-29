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

typedef bool (*djb_push_f)(httpsrv_client_t *shcl, httpsrv_client_t *hcl);

typedef struct {
	/* Push Callback */
	djb_push_f	push;

	char		httpcode[64];
	char		httptext[(256-64-32)];
	char		seqno[32];
	char		setcookie[8192];
	char		cookie[8192];
} djb_headers_t;

/* DJB provided functions */
void djb_error(httpsrv_client_t *hcl, unsigned int errcode, const char *msg);
void djb_result(httpsrv_client_t *hcl, const char *msg);

bool djb_proxy_add(httpsrv_client_t *hcl);
djb_headers_t *djb_create_userdata(httpsrv_client_t *hcl);

/* ACS API */
void acs_init(httpsrv_t *hs);
void acs_exit(void);
bool acs_handle(httpsrv_client_t *hcl);
bool acs_set_net(json_t *net_);

/* Rendezvous API */
void rdv_handle(httpsrv_client_t *hcl);

/* Preferences API */
void prf_init(void);
void prf_exit(void);
void prf_handle(httpsrv_client_t *hcl);
bool prf_parse_bridge_details(const char *br);
int prf_get_argv(char*** argvp);
void prf_free_argv(unsigned int argc, char **argv);

#endif /* SHARED_H */
