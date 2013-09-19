#include "djb.h"

void
prf_handle(httpsrv_client_t *hcl) {
	if (hcl->readbody == NULL) {
		if (httpsrv_readbody_alloc(hcl, 0, 0) < 0) {
			logline(log_DEBUG_,
				"httpsrv_readbody_alloc() failed");
		}
		return;
	} else {
		logline(log_DEBUG_, "prefs = %s", hcl->readbody);
		djb_result(hcl, "Preferences OK");
	}
}

