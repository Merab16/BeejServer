#include "Server.h"
#include <exception>

int main(void)
{
    Server* server;
    try {
        server = new Server();
    }
    catch (std::exception& ex) {
        std::cout << ex.what();
        server->SaveUsers();
    }


    return 0;
}

