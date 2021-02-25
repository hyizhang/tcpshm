#include <stdio.h>
#include "echo_server.h"

#ifdef __cplusplus
extern "C" {
#endif


void* server;

void handler(int) {
    exit(0);
}


void c_wrap_get_instance()

{
    server=get_instance("server","server");
}

void c_wrap_run()

{
    signal(SIGINT, &handler);
    server = Run(server,"0.0.0.0", 12345);

}

#ifdef __cplusplus
}
#endif



