vclient vserver: vclient.c vserver.c
	gcc -o vclient vclient.c -lncurses -lpthread
	gcc -o vsercer vserver.c -lpthread

clean: 
	rm -rf vclient vserver