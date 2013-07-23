#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

int socketFD;

int socketConnect(char* server, char* port);

char buf[10240];
char stdinbuf[10240];

void *runner(void *param);

char cPort[10];

int main(int argc, char* argv[]) {
	if (argc != 3) {
		puts("Usage: fileclient <serverip or servername> <serverport>");
		return EXIT_FAILURE;
	}

	if ((socketFD = socketConnect(argv[1], argv[2])) == 0) {
		puts("Cannot connect");
		return EXIT_FAILURE;
	}

	while (1) {
		printf(">");
		gets(stdinbuf);
		if (strcmp(stdinbuf, "quit") == 0) {
			break;
		} else if (strcmp(stdinbuf, "list") == 0) {
			send(socketFD, "list\n", 5, 0);
			int k, i;
			while ((k = recv(socketFD, buf, 10240, 0)) > 0) {
				for (i = 0; i < k; i++) {
					if (buf[i] == '\0')
						break;
					printf("%c", buf[i]);
				}
				if (i != k)
					break;
			}
		} else if (strcmp(stdinbuf, "get") == 0) {
			puts("Get needs parameters.");
		} else if (strncmp(stdinbuf, "get", 3) == 0 && isspace(stdinbuf[3])) {
			int i;
			for (i = strlen(stdinbuf); i >= 4; i--) {
				if (isspace(stdinbuf[i])) {
					stdinbuf[i] = 0;
				} else {
					break;
				}
			}
			if (strlen(stdinbuf) > 4) {
				i = strlen(stdinbuf);
				stdinbuf[i] = '\n';
				send(socketFD, stdinbuf, i + 1, 0);
				recv(socketFD, buf, 10240, 0);
				int n = 0;
				sscanf(buf, "get %d %s", &n, cPort);
				pthread_t tid[5];
				pthread_attr_t attr[5];
				for (i = 0; i < n; i++) {
					pthread_attr_init(attr + i);
					pthread_create(tid + i, attr + i, runner, argv[1]);
				}

				for (i = 0; i < n; i++) {
					pthread_join(tid[i], NULL);
				}
			} else {
				puts("Get needs parameters.");
			}
		} else {
			puts("Inputs are list, get and quit.");
		}
	}

	close(socketFD);

	return EXIT_SUCCESS;
}

void *runner(void *param) {
	char buf[1024];
	char fName[256];
	int tfd = socketConnect((char *) param, cPort);
	int k = 0;
	int pos = 0;
	char *posPtr;
	while ((k = recv(tfd, buf + pos, 1024 - pos, 0)) > 0) {
		if ((posPtr = memchr(buf + pos, '\0', k))) {
			strcpy(fName, buf);
			pos += k;
			break;
		}
		pos += k;
	}

	FILE *file = fopen(fName, "wb");
	if (file == NULL) {
		close(tfd);
		puts("Cannot create file");
		return NULL;
	}
	int len = pos - (posPtr - buf) - 1;
	if (len > 0) {
		fwrite(posPtr + 1, 1, len, file);
	}
	while ((k = recv(tfd, buf, 1024, 0)) > 0) {
		fwrite(buf, 1, k, file);
	}
	fclose(file);
	close(tfd);
	printf("Downloaded: %s\n", fName);
	return NULL;
}

int socketConnect(char* server, char* port) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	s = getaddrinfo(server, port, &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (s == -1)
			continue;

		if (connect(s, rp->ai_addr, rp->ai_addrlen) != -1)
			break;

		close(s);
	}

	freeaddrinfo(result);

	if (rp == NULL) {
		return 0;
	}

	return s;
}
