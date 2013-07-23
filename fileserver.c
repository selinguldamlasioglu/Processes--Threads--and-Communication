#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>

#define BUF_SIZE 10240

char fileNames[1024 * 256];
int fileNamesLength;

void serveClient(int cfd);
int socketConnect(char* port);
void serveClient();
void ListDIR();

int main(int argc, char* argv[]) {
	if (argc != 2) {
		puts("Usage: fileserver <port>");
		return EXIT_FAILURE;
	}

	ListDIR();

	socketConnect(argv[1]);

	return EXIT_SUCCESS;
}

int cfd;

int socketConnect(char* port) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	s = getaddrinfo(NULL, port, &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;

		close(sfd);
	}

	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);

	listen(sfd, 10);

	socklen_t ca_len;
	struct sockaddr_in ca;

	while (1) {
		memset(&ca, 0, sizeof(ca));
		ca_len = sizeof(ca); // important to initialize
		cfd = accept(sfd, (struct sockaddr *) &ca, &ca_len);

		if (cfd == -1)
			continue;

		int f = fork();
		if (f < 0) {
			perror("main():");
			exit(1);
		}

		if (f == 0) {
			serveClient(cfd);
			exit(EXIT_SUCCESS);
		}
	}

	return 0;
}

char buf2[5][256];
int buf2len = 0;
int tcfds[5];

void *runner(void *param);

int threadConnect() {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int tfd, s;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	s = getaddrinfo(NULL, "0", &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		tfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (tfd == -1)
			continue;

		if (bind(tfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;

		close(tfd);
	}

	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);

	listen(tfd, 10);

	socklen_t sa_len;
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa_len = sizeof(sa); // important to initialize
	getsockname(tfd, (struct sockaddr *) &sa, &sa_len);
	int port = ntohs(sa.sin_port);

	char buf[100];

	sprintf(buf, "get %d %d", buf2len, port);
	send(cfd, buf, strlen(buf) + 1, 0);

	socklen_t ca_len;
	struct sockaddr_in ca;

	pthread_t tid[5];
	pthread_attr_t attr[5];
	int args[5];

	int n = 0;
	while (1) {
		memset(&ca, 0, sizeof(ca));
		ca_len = sizeof(ca);
		tcfds[n] = accept(tfd, (struct sockaddr *) &ca, &ca_len);

		if (tcfds[n] == -1)
			continue;

		pthread_attr_init(attr + n);
		args[n] = n;
		pthread_create(tid + n, attr + n, runner, &args[n]);

		n++;
		if (n >= buf2len) {
			break;
		}
	}

	int i;
	for (i = 0; i < n; i++) {
		pthread_join(tid[i], NULL);
	}

	return 0;
}

void *runner(void *param) {
	int n = *(int *) param;
	char bufr[1024];

	send(tcfds[n], buf2[n], strlen(buf2[n]) + 1, 0);
	FILE* file = fopen(buf2[n], "rb");
	if (file == NULL) {
		close(tcfds[n]);
		puts("Cannot open file");
		return NULL;
	}

	int k;
	while ((k = fread(bufr, 1, 1024, file)) > 0) {
		send(tcfds[n], bufr, k, 0);
	}

	fclose(file);
	close(tcfds[n]);
	return NULL;
}

void serveClient(int cfd) {
	char buf[BUF_SIZE];
	int k;
	int pos = 0;
	char *posPtr = NULL;
	while ((k = recv(cfd, buf + pos, BUF_SIZE - pos, 0)) > 0) {
		if ((posPtr = memchr(buf + pos, '\n', k)) != NULL) {
			if (memcmp(buf, "list", 4) == 0) {
				send(cfd, fileNames, fileNamesLength, 0);
			} else if (memcmp(buf, "get", 3) == 0) {
				*posPtr = 0;
				int f = sscanf(buf, "%*s %s %s %s %s %s", buf2[0], buf2[1],
						buf2[2], buf2[3], buf2[4]);
				int i;
				buf2len = 0;
				for (i = 0; i < f; i++) {
					if (access(buf2[i], F_OK) == 0) {
						if (i != buf2len) {
							strcpy(buf2[buf2len], buf2[i]);
						}
						buf2len++;
					}
				}
				if (buf2len)
					threadConnect();
				else
					send(cfd, "get 0 0\0", 8, 0);

			}
		} else {
			pos += k;
		}
	}
	close(cfd);
}

void ListDIR() {
	DIR *dp;
	struct dirent *ep;

	fileNames[0] = 0;
	fileNamesLength = 1;

	dp = opendir(".");
	if (dp != NULL) {
		while ((ep = readdir(dp)) != NULL) {
			if (ep->d_type & DT_REG) {
				strcpy(fileNames + fileNamesLength - 1, ep->d_name);
				fileNamesLength += strlen(ep->d_name);
				fileNames[fileNamesLength - 1] = '\n';
				fileNames[fileNamesLength] = '\0';
				fileNamesLength += 1;
			}
		}
		closedir(dp);
	}
}

