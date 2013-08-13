#include "djb.h"

void preferences(httpsrv_client_t *hcl) {
  if (hcl->readbody == NULL) {
    if(httpsrv_readbody_alloc(hcl, 0, 0) < 0){
      logline(log_DEBUG_, "preferences: httpsrv_readbody_alloc( failed");
    }
    return;
  } else {
    logline(log_DEBUG_, "preferences: data = %s", hcl->readbody);
    //fprintf(stderr, "preferences: data = %s\n", hcl->readbody);
    djb_result(hcl, "Preferences OK");
  }
}
