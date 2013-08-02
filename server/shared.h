#ifndef SHARED_H
#define SHARED_H 1

// Workaround DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER
// #pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <libfutil/httpsrv.h>

void
djb_error(httpsrv_client_t *hcl, unsigned int errcode, const char *msg);

void djb_freereadbody(httpsrv_client_t *hcl);

#endif /* SHARED_H */
