all: dbclient dbserver

dbclient: dbclient.c
	gcc dbclient.c -o dbclient -Wall -Werror -std=gnu99 -pthread
dbserver: dbserver.c
	gcc dbserver.c -o dbserver -Wall -Werror -std=gnu99 -pthread
clean:
	rm -rf *o dbserver dbclient
