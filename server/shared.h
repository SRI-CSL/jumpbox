#ifndef SHARED_H
#define SHARED_H 1

#include <libfutil/httpsrv.h>

void
djb_error(httpsrv_client_t *hcl, unsigned int errcode, const char *msg);

#endif /* SHARED_H */
