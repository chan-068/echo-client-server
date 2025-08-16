#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <thread>

void usage()
{
    printf("syntax : echo-client <ip> <port>\n");
    printf("sample : echo-client 192.168.10.2 1234\n");
}

void myerror(const char *msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }

struct Param
{
    char *ip{nullptr};
    char *port{nullptr};
    uint32_t srcIp{0};
    uint16_t srcPort{0};

    bool parse(int argc, char *argv[])
    {
        (*this).ip = argv[1];
        (*this).port = argv[2];
        return (ip != nullptr) && (port != nullptr);
    }
} param;

void recvThread(int sd) {
	printf("connected\n");
	fflush(stdout);
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) {
			fprintf(stderr, "recv return %zd", res);
			myerror(" ");
			break;
		}
		buf[res] = '\0';
		printf("%s", buf);
		fflush(stdout);
	}
	printf("disconnected\n");
	fflush(stdout);
	::close(sd);
	exit(0);
}

int main(int argc, char *argv[])
{
    if (!param.parse(argc, argv))
    {
        usage();
        return -1;
    }

    struct addrinfo aiInput, *aiOutput, *ai;
    memset(&aiInput, 0, sizeof(aiInput));
    aiInput.ai_family = AF_INET;
    aiInput.ai_socktype = SOCK_STREAM;
    aiInput.ai_flags = 0;
    aiInput.ai_protocol = 0;

    getaddrinfo(param.ip, param.port, &aiInput, &aiOutput);

    int sd;
    for (ai = aiOutput; ai != nullptr; ai = ai->ai_next)
    {
        sd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sd != -1)
            break;
    }
    if (ai == nullptr)
    {
        fprintf(stderr, "cann not find socket for %s\n", param.ip);
        return -1;
    }
    int optval = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, (const char *)&optval, sizeof(int)) < 0)
    {
        myerror("setsockopt(SO_KEEPALIVE)");
        return -1;
    }

    if (param.srcIp != 0 || param.srcPort != 0)
    {
        struct sockaddr_in addr;

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = param.srcIp;
        addr.sin_port = htons(param.srcPort);
        ssize_t res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
        if (res == -1)
        {
            myerror("bind");
            return -1;
        }
    }

        int resp = ::connect(sd, ai->ai_addr, ai->ai_addrlen);
    if (resp == -1)
    {
        myerror("connect");
        return -1;
    }
    
    std::thread t(recvThread, sd);
	t.detach();

    while (true) {
		std::string s;
		std::getline(std::cin, s);
        if (s == "q") break; 

		ssize_t res = ::send(sd, s.data(), s.size(), 0);
		if (res == 0 || res == -1) {
			fprintf(stderr, "send return %zd", res);
			myerror(" ");
			break;
		}
	}
	::close(sd); 
}

