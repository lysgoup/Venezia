#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <pthread.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <getopt.h>

#define BUFFER_SIZE 128
#define INFO_SIZE 10

static int MODE_INPUT = 0;
static int MODE_OUTPUT = 1;
static int MODE_LOGIN = 2;
int player, cur_player;

typedef struct user_info{
  char username[10];
  char password[10];
  int score;
}user_info ;

typedef struct thread_info{
	user_info * user;
	int player;
	int sock_fd;
	int file_fd;
}thread_info ;

user_info * header = NULL;

int
recv_bytes(int fd, void * ptr, int recv_size)
{
    char * local_ptr = (char *) ptr ;
    ssize_t acc_size = 0; 
    ssize_t size;
    while(acc_size < recv_size)
    {
        size = recv(fd, local_ptr + acc_size, recv_size - acc_size, 0);
        acc_size += size;
        
        if(size == -1 || size == 0)
        {
            return 0;
        }
    }
    return acc_size; 
}

int 
send_bytes(int fd, void * ptr, int send_size)
{
    char * local_ptr = (char *) ptr ;
    ssize_t size;
    ssize_t acc_size = 0;
    while(acc_size < send_size)
    {
        size = send(fd, local_ptr + acc_size, send_size - acc_size, MSG_NOSIGNAL);
        acc_size += size;
        if(size == -1 || size == 0)
        {
            return 0;
        }
    }
    return acc_size;
}

void *
input_thread(void * arg)
{
	int conn = *(int*)arg;
	printf("[INPUT]connected\n");
	close(conn);
}

void *
output_thread(void * arg)
{
	thread_info * info = (thread_info*)arg;
	printf("[OUTPUT]connected\n");
	while(cur_player<player);
	int s;
	char buf[BUFFER_SIZE];
	fgets(buf,sizeof(buf),(info->file_fd));
	buf[strlen(buf) - 1] = 0;
	if(!(s=send_bytes(info->sock_fd, &buf, 128))){
		fprintf(stderr, "[OUTPUT]cannot send sentence\n");
		return NULL;
	};
	printf("here\n");

	free(((thread_info*)arg)->user);
	free((thread_info*)arg);
	close(info->sock_fd);
}

int 
main(int argc, char *argv[])
{
  //get options
	if (argc < 9){
		printf("invalid command!\n\n Usage: %s --port [portn_num] --player [palyer_num] --turn [turn] --corpus [sentence_file]\n\n",argv[0]);
		exit(EXIT_FAILURE);
	}
	struct option options[] = {
		{"port", 1, 0, 0},
		{"player" , 1, 0, 0},
		{"turn" , 1, 0, 0},
		{"corpus", 1, 0, 0}
	};
	int opt, index = 0;
	int port, turn, game_source_fp;
	char * corpus;

	while(opt != -1){
		opt = getopt_long(argc, argv, "", options, &index);
		if(opt == -1) break;
		switch(index){
			case 0:
				port = atoi(optarg);
				break;
			case 1:
				player = atoi(optarg);
				break;
			case 2:
				turn = atoi(optarg);
				break;
			case 3:
				char * game_source;
				game_source = strdup(optarg);
				game_source_fp = fopen(game_source, "r");
				break;
			case '?':
				opt = -1;
				break;
		}
	}

	user_info user[player];

	//make server socket and bind 
	int listen_fd ; 
	struct sockaddr_in address; 
	int addrlen = sizeof(address); 
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == 0){ 
		perror("socket failed : "); 
		exit(EXIT_FAILURE); 
	}
	memset(&address, '0', sizeof(address)); 
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port); 
	if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed : "); 
		exit(EXIT_FAILURE); 
	}

	//Receive mode
	pthread_t tid;
	int mode, s = 0;
	while(1){
		if (listen(listen_fd, 16) < 0) { 
			perror("listen failed");
			exit(EXIT_FAILURE);
		} 
		int new_socket = accept(listen_fd, (struct sockaddr *) &address, (socklen_t*)&addrlen) ;
		if(!(s=recv_bytes(new_socket, (void *)&mode, sizeof(int)))){
			fprintf(stderr, "login error: mode");
			exit(EXIT_FAILURE);
		}

		//Login
		if(mode==MODE_LOGIN){
			printf("mode!!!: %d\n", mode);
			if(!(s=recv_bytes(new_socket, (void *)&user[cur_player].username, INFO_SIZE))){
				fprintf(stderr, "login error: username");
				exit(EXIT_FAILURE);
			}
			if(!(s=recv_bytes(new_socket, (void *)&user[cur_player].password, INFO_SIZE))){
				fprintf(stderr, "login error: password");
				exit(EXIT_FAILURE);
			}
			cur_player++;
			close(new_socket);
		}

		//Input client
		else if(mode==MODE_INPUT){
			thread_info * info;
			info = (thread_info*)malloc(sizeof(thread_info));
			info->player = player;
			info->sock_fd = new_socket;
			info->user = (user_info *)malloc(sizeof(user_info)*player);
			*(info->user) = *user;
			if (pthread_create(&tid, NULL, input_thread, info) != 0) {
				close(new_socket);
				free(info->user);
				free(info);
				perror("cannot create input thread") ;
				continue;
			}
		}

		//Output client
		else if(mode==MODE_OUTPUT){
			thread_info * info;
			info = (thread_info*)malloc(sizeof(thread_info));
			info->player = player;
			info->sock_fd = new_socket;
			info->user = (user_info *)malloc(sizeof(user_info)*player);
			*(info->user) = *user;
			if (pthread_create(&tid, NULL, output_thread, info) != 0) {
				free(info->user);
				free(info);
				close(new_socket);
				perror("cannot create output thread") ;
				continue;
			}
		}
	}
	printf("done");
	return 0;
}