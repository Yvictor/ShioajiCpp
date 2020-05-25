#include "sol.h"

#include <iostream>

int hello() {
    solClient_initialize(SOLCLIENT_LOG_ERROR, NULL);
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
