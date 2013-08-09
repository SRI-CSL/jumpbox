#include "shared.h"

void
djb_error(httpsrv_client_t *hcl, unsigned int errcode, const char *msg) {
	conn_addheaderf(&hcl->conn, "HTTP/1.1 %u %s", errcode, msg);
	conn_printf(&hcl->conn, "<h1>%s</h1>\n", msg);
	httpsrv_done(hcl);
}
