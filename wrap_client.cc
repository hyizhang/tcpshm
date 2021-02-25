#include <stdio.h>
#include <iostream>
#include <signal.h>
#include <cstdlib>
#include "echo_client.h"

#ifdef __cplusplus
extern "C" {
#endif


void* client;
void handler(int) {
    exit(0);
}

void c_wrap_get_instance()
{
    client=get_instance("client","client");

}


void c_wrap_run()
{ 
    //catch ctrl+C signal
    signal(SIGINT, &handler);
    client = Run(client,true,"127.0.0.1",12345);
}

//send message
void c_wrap_send()
{
    client = send(client);
}

//receive message
char* c_wrap_receive()
{
    string result="";
    client = receive(client,result);
    char* p = (char*)result.c_str();
    return p;
}



#ifdef __cplusplus
}
#endif


