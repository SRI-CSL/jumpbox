#ifndef SHARED_H
#define SHARED_H 1

#if __llvm__
// Workaround DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <libfutil/httpsrv.h>

void
djb_error(httpsrv_client_t *hcl, unsigned int errcode, const char *msg);

int djb_allocreadbody(httpsrv_client_t *hcl, uint64_t min, uint64_t max);
void djb_freereadbody(httpsrv_client_t *hcl);

#endif /* SHARED_H */
