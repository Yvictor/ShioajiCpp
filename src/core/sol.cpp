#include "sol.h"


int hello() {
    solClient_initialize(SOLCLIENT_LOG_ERROR, NULL);
    SPDLOG_INFO("Hello ~");
    return 0;
}
