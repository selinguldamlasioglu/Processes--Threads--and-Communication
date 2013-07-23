all: fileserver fileclient
fileserver: fileserver.c
	gcc -g -Wall -o fileserver fileserver.c -lpthread
fileclient: fileclient.c
	gcc -g -Wall -o fileclient fileclient.c -lpthread
clean:
	rm -fr *~ fileserver *.txt core* fileclient