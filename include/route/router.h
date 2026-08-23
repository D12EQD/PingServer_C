#pragma once

#include <stddef.h>
#include "net/connection.h" 

typedef void (*routerFunction)(connection_t*, void*);

routerFunction get_router_function(const char*, size_t, const char*, size_t);