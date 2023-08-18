tclient: tclient.c
	gcc -o tclient tclient.c -lncurses -lpthread

vclient vserver: vclient.c vserver.c
	gcc -o vclient vclient.c -lncurses -lpthread
	gcc -o vsercer vserver.c -lpthread

.PHONY : play start clean

play:
	./tclient

start:
	./vserver

clean: 
	rm -rf vclient vserver tclient