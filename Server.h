#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>

#define PORT "4444"

class Server
{

private:
    fd_set _master;
    fd_set _read_fds;
    int _fdmax;

    int _listener;
    int _newfd;
    struct sockaddr_storage remoteaddr;
    socklen_t _addrlen;

    char buf[256];

    char remoteIP[INET6_ADDRSTRLEN];

    struct addrinfo _hints, *_ai;

private:
    void Initialization()
    {
        FD_ZERO(&_master);
        FD_ZERO(&_read_fds);

        memset(&_hints, 0, sizeof(_hints));
        _hints.ai_family = AF_UNSPEC;
        _hints.ai_socktype = SOCK_STREAM;
        _hints.ai_flags = AI_PASSIVE;

        int rv;
        if ((rv = getaddrinfo(NULL, PORT, &_hints, &_ai)) != 0)
        {
            std::cout << "selectserver: " << gai_strerror(rv) << std::endl;
            exit(1);
        }

        struct addrinfo *p;
        for (p = _ai; p != NULL; p = p->ai_next)
        {
            _listener - socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (_listener < 0)
            {
                continue;
            }

            int yes = 1;
            setsockopt(_listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
            if (bind(_listener, p->ai_addr, p->ai_addrlen) < 0)
            {
                close(_listener);
                continue;
            }

            break;
        }

        if (p == NULL)
        {
            std::cout << "selectserver: failed to bind\n";
            exit(2);
        }

        if (listen(_listener, 10) == -1)
        {
            perror("listen");
            exit(3);
        }
    }
    void AcceptConnection()
    {
        FD_SET(_listener, &_master);

        _fdmax = _listener;

        for (;;)
        {
            _read_fds = _master;
            if (select(_fdmax + 1, &_read_fds, NULL, NULL, NULL) == -1)
            {
                perror("select");
                exit(4);
            }

            for (int i = 0; i < _fdmax; ++i)
            {
                if (FD_ISSET(i, &_read_fds))
                {
                    if (i == _listener)
                    {
                        _addrlen = sizeof(remoteaddr);
                        _newfd = accept(_listener, (struct sockaddr *)&remoteaddr, &_addrlen);
                        if (_newfd == -1)
                        {
                            perror("accept");
                        }
                        else
                        {
                            FD_SET(_newfd, &_master);
                            if (_newfd > _fdmax)
                            {
                                _fdmax = _newfd;
                            }
                            std::cout << "selectserver: new connection\n";
                        }
                    }
                    else
                    {
                        int nbytes;
                        if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0)
                        {
                            if (nbytes == 0)
                            {
                                std::cout << "selectserver: socket " << i << " hung up\n";
                            }
                            else
                            {
                                perror("recv");
                            }
                            close(i);
                            FD_CLR(i, &_master);
                        }
                    }
                }
            }
        }
    }

public:
    Server(int port = 1235)
    {
        Initialization();
        AcceptConnection();
    }
    ~Server() {}
};

// получить sockaddr, IPv4 или IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}
