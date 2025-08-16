#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>

std::vector<int> g_client_sds;
std::mutex g_client_sds_mutex;

void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }

void usage() {
	printf("syntax : echo-server <port> [-e[-b]]\n");
    printf("sample : echo-server 1234 -e -b\n");
}


struct Param {
	bool echo{false};
    bool broadcast{false};
	uint16_t port{0};
	uint32_t srcIp{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc;) {
			if (strcmp(argv[i], "-e") == 0) {
				echo = true;
				i++;
				continue;
			}
            if(strcmp(argv[i], "-b")==0){
                broadcast=true;
                i++;
                continue;
            }
            if (i < argc) port = atoi(argv[i++]);
        }

		return port != 0;
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
		printf("%s\n", buf);
		fflush(stdout);
		if (param.echo) {
			buf[res] = '\n'; 

			ssize_t sent_len = ::send(sd, buf, res + 1, 0); 
			if (sent_len == 0 || sent_len == -1) {
				fprintf(stderr, "send return %zd", sent_len);
				myerror(" ");
				break;
			}
		}
        if (param.broadcast) {
            std::lock_guard<std::mutex> lock(g_client_sds_mutex);
            for (int client_sd : g_client_sds) {
                if (client_sd == sd) continue;
                buf[res] = '\n';
                ssize_t sent_len = ::send(client_sd, buf, res+1, 0);
                if (sent_len <= 0) {
                    fprintf(stderr, "broadcast send to %d failed\n", client_sd);
                }
            }
        }
	}
	printf("disconnected\n");
	fflush(stdout);
	::close(sd);
}

int main(int argc, char* argv[]){
    if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}


    int sd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		myerror("socket");
		return -1;
	}
    
    struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = param.srcIp;
	addr.sin_port = htons(param.port);

   
	ssize_t res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
	if (res == -1) {
		myerror("bind");
		return -1;
	}

    int resp = listen(sd, 5);
		if (resp == -1) {
			myerror("listen");
			return -1;
	}

    while (true) {
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		int newsd = ::accept(sd, (struct sockaddr *)&addr, &len);
		if (newsd == -1) {
			myerror("accept");
			break;
		}
        int optval = 1;
		if (setsockopt(newsd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&optval, sizeof(int)) < 0) {
			myerror("setsockopt(SO_KEEPALIVE)");
			return -1;
		}

        {
        std::lock_guard<std::mutex> lock(g_client_sds_mutex);
        g_client_sds.push_back(newsd);
        printf("client connected. current clients: %zu\n", g_client_sds.size());
        }

        std::thread* t = new std::thread(recvThread, newsd);
		t->detach();
		
	}

	::close(sd);
}