#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fstream>
#include <map>
#include <string>
#include <iostream>

#define PORT "9034"   // port we're listening on

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

class Server {

private:

    // DB
    std::map<std::string, std::string> _users_from_db;
    std::string _lg, _pw;

    // server 
    fd_set _master;
    fd_set _read_fds;
    int _fdmax;

    int _listener;
    int _newfd;
    struct sockaddr_storage _remoteaddr; // client address
    socklen_t _addrlen;

    char _buf[256];    // buffer for client data
    int _nbytes;

	char _remoteIP[INET6_ADDRSTRLEN];
    struct addrinfo _hints, *_ai;

private:
    void Initialization() {
        FD_ZERO(&_master);    // clear the _master and temp sets
        FD_ZERO(&_read_fds);

        // get us a socket and bind it
        memset(&_hints, 0, sizeof _hints);
        _hints.ai_family = AF_UNSPEC;
        _hints.ai_socktype = SOCK_STREAM;
        _hints.ai_flags = AI_PASSIVE;
        int rv;
        if ((rv = getaddrinfo(NULL, PORT, &_hints, &_ai)) != 0) {
            fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
            exit(1);
        }
        
        struct addrinfo* p;
        for(p = _ai; p != NULL; p = p->ai_next) {
            _listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (_listener < 0) { 
                continue;
            }
            
            // lose the pesky "address already in use" error message
            int yes = 1;
            setsockopt(_listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

            if (bind(_listener, p->ai_addr, p->ai_addrlen) < 0) {
                close(_listener);
                continue;
            }

            break;
        }

        // if we got here, it means we didn't get bound
        if (p == NULL) {
            fprintf(stderr, "selectserver: failed to bind\n");
            exit(2);
        }

        freeaddrinfo(_ai); // all done with this

        // listen
        if (listen(_listener, 10) == -1) {
            perror("listen");
            exit(3);
        }

        // add the listener to the _master set
        FD_SET(_listener, &_master);

        // keep track of the biggest file descriptor
        _fdmax = _listener; // so far, it's this one
    }

    void AcceptConnection() {
        for(;;) {
        _read_fds = _master; // copy it
        if (select(_fdmax+1, &_read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        int i;
        for(i = 0; i <= _fdmax; i++) {
            if (FD_ISSET(i, &_read_fds)) { // we got one!!
                if (i == _listener) {
                    // handle new connections
                    _addrlen = sizeof _remoteaddr;
					_newfd = accept(_listener,
						(struct sockaddr *)&_remoteaddr,
						&_addrlen);

					if (_newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(_newfd, &_master); // add to master set
                        if (_newfd > _fdmax) {    // keep track of the max
                            _fdmax = _newfd;
                        }
                        printf("selectserver: new connection from %s on "
                            "socket %d\n",
							inet_ntop(_remoteaddr.ss_family,
								get_in_addr((struct sockaddr*)&_remoteaddr),
								_remoteIP, INET6_ADDRSTRLEN),
							_newfd);
                    }
                } else {
                    // handle data from a client
                    int nbytes;
                    if ((nbytes = recv(i, _buf, sizeof _buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &_master); // remove from master set
                    } else {
                        //std::cout << _buf << std::endl;
                        bool isLogin = true;
                            for (const auto& ch: _buf) {
                                if (ch == ' ') {
                                    isLogin = false;
                                    continue;
                                }
                                if (isLogin) {
                                    _lg += ch;
                                }
                                else {
                                    _pw += ch;
                                }

                            }
                            std::cout << "Parsed: " << _lg << ' ' << _pw << std::endl;

                            char success[] = "-y";
                            char non[] = "-n";
                            if (CheckUser(_lg, _pw)) {
                                send(i, success, sizeof success, NULL);
                            }
                            else {
                                send(i, non, sizeof non, NULL);
                            }
                        
                        _lg.clear();
                        _pw.clear();
                        



                        // int j;
                        // // we got some data from a client
                        // for(j = 0; j <= _fdmax; j++) {
                        //     // send to everyone!
                        //     if (FD_ISSET(j, &_master)) {
                        //         // except the listener and ourselves
                        //         if (j != _listener && j != i) {
                        //             if (send(j, _buf, nbytes, 0) == -1) {
                        //                 perror("send");
                        //             }
                        //         }
                        //     }
                        // }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    }

    void LoadUsers() {
        std::ifstream fin("users.txt");
        while (!fin.eof()) {
            std::string lg, pw;
            fin >> lg >> pw;
            _users_from_db.emplace(lg, pw);
        }


        for (const auto& [lg, pw]: _users_from_db) {
            std::cout << lg << ' ' << pw << std::endl;
        }


        fin.close();

    }

    bool CheckUser(const std::string& lg, const std::string& pw) {
        if (_users_from_db.count(lg)) {
            std::string db_pw = _users_from_db[lg];
            std::cout << db_pw.size() << ' ' << pw.size() << std::endl;
            std::cout << db_pw << ' ' << pw << std::endl;
            return (db_pw == pw);
            //std::cout << "true\n";
            //return true;
        }
        return false;
    }

public:
    Server() {
        LoadUsers();
        Initialization();
        AcceptConnection();
    }

    ~Server() {

    }



};


