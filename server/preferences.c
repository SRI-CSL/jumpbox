#include "djb.h"


static int prf_parse_preferences(void);
static void dump_preferences(void);

static char* stegotorus_server_address = NULL;
static int  stegotorus_server_port = -1;
static char* current_preferences = NULL;

enum { PRF_CC = 0, PRF_EXE, PRF_LL, PRF_SM, PRF_TP, PRF_SHS };

/* quick and dirty - sorry */
static int number_of_keys = 7;
/* keep these both the same length (number_of_keys) */
static char* keys[] =   { (char *)"stegotorus_circuit_count", 
                          (char *)"stegotorus_executable", 
                          (char *)"stegotorus_log_level", 
                          (char *)"stegotorus_steg_module", 
                          (char *)"stegotorus_trace_packets", 
                          (char *)"shared_secret", 
                          NULL};
static char* values[] = { NULL,  NULL,  NULL,  NULL,   NULL,  NULL,  NULL};
static char* defaults[] =   { (char *)"1", 
                              (char *)"stegotorus", 
                              (char *)"warn", 
                              (char *)"json", 
                              (char *)"false", 
                              (char *)NULL, 
                              NULL};


static char* getvalue(int i);

char* getvalue(int i){
  if((i < 0) || (i >= number_of_keys) || (keys[i] == NULL)){
    return NULL;
  } else {
    return (values[i] != NULL) ? values[i] : defaults[i]; 
  }
}



void 
prf_set_stegotorus_server(char *address, int port){
  if(stegotorus_server_address != NULL){
    free(stegotorus_server_address);
  }
  stegotorus_server_address = (address == NULL ? NULL : strdup(address));
  stegotorus_server_port = port;
}


void
prf_handle(httpsrv_client_t *hcl) {
  if (hcl->readbody == NULL) {
    if (httpsrv_readbody_alloc(hcl, 0, 0) < 0) {
      logline(log_WARNING_,
              "httpsrv_readbody_alloc() failed");
    }
    return;
  } else {
    int retcode, i;
    logline(log_INFO_, "prefs = %s", hcl->readbody);
    if(current_preferences != NULL){ free(current_preferences); } 
    current_preferences = (hcl->readbody == NULL ? NULL : strdup(hcl->readbody));
    retcode = prf_parse_preferences();
    if(retcode == 0){
      /* this block is just for testing */
      if(0){
        int argc = 0;
        char**argv = NULL;
        dump_preferences();
        prf_set_stegotorus_server((char*)"127.0.0.1", 1080);
        argc = prf_get_argv(&argv);
        logline(log_WARNING_, "argc = %d", argc);
        for(i = 0; i < argc; i++){
          fprintf(stderr, "argv[%d] = %s\n", i, argv[i]);
        }
      }
    } else {
      logline(log_WARNING_, "prf_parse_preferences() = %d", retcode);
    }
  }
  djb_result(hcl, "Preferences OK");
}



int prf_parse_preferences(){
  if(current_preferences == NULL){
    return 1;
  } else {
    json_error_t error;
    json_t *root = json_loads(current_preferences, 0, &error);
    if(!root){
      return 2;
    } 
    if(!json_is_object(root)){
      json_decref(root);
      return 3;
    } else {
      int i;
      for(i = 0; i < number_of_keys; i++){
        json_t *valueobj = NULL;
        char *key = keys[i];
        if(key == NULL){
          break; 
        }
        valueobj = json_object_get(root, key);
        if(!json_is_string(valueobj)){ 
          continue; 
        } else {
          char* oldvalue = values[i];
          if(oldvalue != NULL){ free(oldvalue); }
          values[i] = strdup(json_string_value(valueobj));
        }
      }
      json_decref(root);
    }
  }
  return 0;
}

void dump_preferences(void){
  int i;
  for(i = 0; i < number_of_keys; i++){
    if(keys[i] != NULL){
      fprintf(stderr, "%s => %s\n", keys[i], values[i]);
    }
  }
}

/*
${HOME}/Repositories/isc/stegotorus/stegotorus --log-min-severity=warn chop socks  --persist-mode --trace-packets --shared-secret=bingoBedbug 127.0.0.1:1080  127.0.0.1:6543 ${MODULE} ... NULL
1                                              2                       3    4      5               [opt]          [opt]                       6               [ n circuits * 2]            7 
*/

int prf_get_argv(char*** argvp){
  if(argvp != NULL){
    if((stegotorus_server_address == NULL) || (stegotorus_server_port == -1)){
      *argvp = NULL;
      return 0;
    } else {
      char** argv = NULL;
      char *shared_secret = getvalue(PRF_SHS);
      int trace_packets = (strcmp(getvalue(PRF_TP), "true") == 0);
      int circuits = atoi(getvalue(PRF_CC));
      int argc = 7 + (2 * circuits);
      int i;
      if(shared_secret != NULL){ argc++; }
      if(trace_packets){ argc++; } 
      argv = (char **)calloc(argc, sizeof(char *));
      if(argv != NULL){
        char scratch[156];
        int vslot = 0;
        argv[vslot++] = strdup(getvalue(PRF_EXE));
        memset(scratch, 0, 156);
        snprintf(scratch, 156, "--log-min-severity=%s", getvalue(PRF_LL));
        argv[vslot++] = strdup(scratch);
        argv[vslot++] = strdup("chop");
        argv[vslot++] = strdup("socks");
        argv[vslot++] = strdup("--persist-mode");
        if(trace_packets){ 
          argv[vslot++] = strdup("--trace-packets");
        }
        if(shared_secret != NULL && (strcmp(shared_secret, "") != 0)){
          memset(scratch, 0, 156);
          snprintf(scratch, 156, "--shared-secret=%s", getvalue(PRF_SHS));
          argv[vslot++] = strdup(scratch);
        }
        memset(scratch, 0, 156);
        snprintf(scratch, 156, "%s:%d", stegotorus_server_address, stegotorus_server_port);
        argv[vslot++] = strdup(scratch);
        for(i = 0; i < circuits; i++){
          argv[vslot++] = strdup("127.0.0.1:6543");  //should get this from somewhere?!?
          argv[vslot++] = strdup(getvalue(PRF_SM));
        }
        argv[vslot++] = NULL;
      }
      *argvp = argv;
      return argc;
    }
  } else {
    return -1;
  }
}

