#include "Server.h"


int main(void)
{
    Server* server;
        server = new Server();

        server->SaveUsers();
    

    delete server;
    return 0;
}

