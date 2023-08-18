#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <pthread.h> 
#include <sys/socket.h> 
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <getopt.h>

#define THEME BLUE
#define BUFSIZE 128
#define INFO_SIZE 10
static int MODE_INPUT = 0;
static int MODE_OUTPUT = 1;
static int MODE_LOGIN = 2;
static char username[INFO_SIZE];
static char password[INFO_SIZE];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t c = PTHREAD_COND_INITIALIZER;

typedef enum{
	BLACK,
	BLUE,
	GREEN,
	RED,
	NOP
} color;

typedef struct windows{
	WINDOW * gameboard;
	WINDOW * scoreboard;
}windows;

int isError = 0;
int done = 0;
char errorMsg[BUFSIZE];
char* ip;
int port = 0;
int game_start = 0;

int send_bytes(int fd, void * buf, size_t len){
    char * p = buf ;
    size_t acc = 0 ;

    while (acc < len) {
        size_t sent ;
        sent = send(fd, p, len - acc, MSG_NOSIGNAL) ;
        if (sent == -1 || sent == 0)
                return 0 ;
        p += sent ;
        acc += sent ;
    }
    return acc ;
}

int recv_bytes(int fd, void * buf, size_t len){
    char * p = buf ;
    size_t acc = 0 ;

    while (acc < len) {
        size_t sent ;
        sent = recv(fd, p, len - acc, 0) ;
        if (sent == -1 || sent == 0)
                return 0 ;
        p += sent ;
        acc += sent ;
    }
    return acc ;
}

//make new sock and connection with ip,port and return the sock_fd(0 on fail)
int makeConnection(){

	int conn;
	struct sockaddr_in serv_addr; 
	conn = socket(AF_INET, SOCK_STREAM, 0) ;
	
	if (conn <= 0) {
		strcpy(errorMsg,  " socket failed\n");
		return 0;
	} 

	memset(&serv_addr, '\0', sizeof(serv_addr)); 
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(port); 
	if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
		strcpy(errorMsg, " inet_pton failed\n");
		return 0;
	}
	if (connect(conn, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		strcpy(errorMsg, " connect failed\n");
		return 0;
	}

	return conn;

}

//init basic about main window
void init(){

	initscr();
    start_color(); //for using color option
		init_pair(0, COLOR_WHITE, COLOR_BLACK); //set color themes
    init_pair(1, COLOR_BLUE, COLOR_WHITE);
    init_pair(2, COLOR_GREEN, COLOR_WHITE);
    init_pair(3, COLOR_RED, COLOR_WHITE);
	bkgd(COLOR_PAIR(THEME));//select theme
    curs_set(2); //set corsor's visibility (0: invisible, 1: normal, 2: very visible)
    keypad(stdscr, TRUE); //enable to reading a function keys 
	cbreak(); //accept the special characters... 
	echo();
    
}

//print basic information to main window
void init_main(){
	mvprintw(2, 1, "2023-summer capston1-study");
	mvprintw(27, COLS-10, "@leeejjju");
	move(30, 0);
	
	refresh();

}

//init inputbox window
void init_inputBox(WINDOW* inputwin){

	WINDOW* inBox = newwin(3, COLS, 24, 0);
	wbkgd(inBox, COLOR_PAIR(THEME));
	box(inBox, ACS_VLINE, ACS_HLINE);
	wrefresh(inBox);

	wbkgd(inputwin, COLOR_PAIR(THEME));
	idlok(inputwin, TRUE);
	scrollok(inputwin, TRUE);
	wrefresh(inputwin);
	refresh();
}

void* inputClient(void* w){

	WINDOW* inputwin = (WINDOW*)w;
	char buf[BUFSIZE];
	int s;
	int conn;

	while(game_start == 0){
		//waiting
	}
	
	while(1){

		wgetstr(inputwin, buf);

		if(strlen(buf) > BUFSIZE || strlen(buf) == 0){
			werase(inputwin);
			mvwprintw(inputwin, 0, 0, " cannot send :(");
			getch();
			werase(inputwin);
			wrefresh(inputwin);
			continue;
		}

		if(strlen(buf) <= 0){
			werase(inputwin);
			wrefresh(inputwin);
			continue;
		}

		if(!(conn = makeConnection())){
			isError = 1;
			return NULL;
		}
		
		//send mode
		if(!(s = send_bytes(conn, (void*)&MODE_INPUT, sizeof(int)))){
			fprintf(stderr, " [INPUT]cannot send header");
			return NULL;
		}
		//send username 
		if(!(s = send_bytes(conn, (void*)username, INFO_SIZE))){
			fprintf(stderr, " [INPUT]cannot send username");
			isError = 1;
			return NULL;
		}
		//send password 
		if(!(s = send_bytes(conn, (void*)password, INFO_SIZE))){
			fprintf(stderr, " [INPUT]cannot send password");
			isError = 1;
			return NULL;
		}
		//send buf
		if(!(s = send_bytes(conn, (void*)buf, INFO_SIZE))){
			fprintf(stderr, " [INPUT]cannot send sentence");
			isError = 1;
			return NULL;
		}

		werase(inputwin);
		wrefresh(inputwin);
		close(conn);
	}

	return NULL;
}

//init outputbox window
void init_outputBox(WINDOW* gameboard){

	WINDOW* outBox = newwin(21, COLS/3*2, 3, 0);
	wbkgd(outBox, COLOR_PAIR(THEME));
	box(outBox, ACS_VLINE, ACS_HLINE);
	wrefresh(outBox);
	refresh();

	wbkgd(gameboard, COLOR_PAIR(THEME));
	idlok(gameboard, TRUE);
	scrollok(gameboard, TRUE);
	wprintw(gameboard, " enter \"quit\" to exit!\n");
	wrefresh(gameboard);
	refresh();
}

void* outputClient(void* w){
	
	windows * output_wins = (windows *)w;
	char sentence[BUFSIZE];
	char name[INFO_SIZE];
	int score;
	int conn;
	int s, i = 0;

	werase(output_wins->gameboard);
	mvwprintw(output_wins->gameboard, 0, 0, "Waiting other players...\n");
	wrefresh(output_wins->gameboard);
	refresh();

	while(1){
		if(!(conn = makeConnection())){
			isError = 1;
			return NULL;
		}

		//send mode
		if(!(s = send_bytes(conn, (void*)&MODE_OUTPUT, sizeof(int)))){
			fprintf(stderr, "[OUTPUT]cannot send mode\n");
			return NULL;
		}

		shutdown(conn, SHUT_WR);

		//receive sentence
		if(!(s = recv_bytes(conn, (void*)sentence, BUFSIZE))){
			fprintf(stderr, "[OUTPUT]cannot receive sentence\n");
			return NULL;
		}
		werase(output_wins->gameboard);
		mvwprintw(output_wins->gameboard, 0, 0, "%s\n",sentence);
		wrefresh(output_wins->gameboard);
		refresh();

		game_start = 1;

		//recieve name and score
		werase(output_wins->scoreboard);
		mvwprintw(output_wins->scoreboard, 0, 0, "");

		while((s = recv_bytes(conn, (void*)name, INFO_SIZE))){
			if(!(s = recv_bytes(conn, (void*)&score, sizeof(int)))){
				fprintf(stderr,"recv score error");
				close(conn);
				return NULL;
			}
			wprintw(output_wins->scoreboard, "%s : %d\n", name, score);
			wrefresh(output_wins->scoreboard);
		}
		refresh();
		close(conn);
	}
	return NULL;
}

//init scorebox window
void init_scoreBox(WINDOW* score){
	WINDOW* sBox = newwin(21, COLS/3, 3, COLS/3*2);
	wbkgd(sBox, COLOR_PAIR(THEME));
	box(sBox, ACS_VLINE, ACS_HLINE);
	wrefresh(sBox);
	refresh();

	wbkgd(score, COLOR_PAIR(THEME));
	idlok(score, TRUE);
	scrollok(score, TRUE);
	wprintw(score, " ***SCORE BOARD***\n");
	wrefresh(score);
	refresh();
}

int main(int argc, char *argv[]) {

	//init windows
	init();
	init_main();
	WINDOW* gameboard = newwin(19, COLS/3*2-2, 4, 1);
	init_outputBox(gameboard);
	WINDOW* scoreboard = newwin(19, COLS/3-3, 4, COLS/3*2+1);
	init_scoreBox(scoreboard);
	WINDOW* inputwin = newwin(1, COLS-2, 25, 1);
	init_inputBox(inputwin);
	refresh();

	windows output_wins;
	output_wins.gameboard = gameboard;
	output_wins.scoreboard = scoreboard;

	//get options
	if (argc < 9){
		mvwprintw(gameboard, 0, 0, " invalid command!\n\n Usage: %s --ip=[ip] --port=[port] --username=username(MAX_LENGTH=20) --password=password(MAX_LENGTH=20)\n\n",argv[0]);
		wrefresh(gameboard);
		goto EXIT;
	}
	struct option options[] = {
		{"ip", 1, 0, 0},
		{"port" , 1, 0, 0},
		{"username" , 1, 0, 0},
		{"password", 1, 0, 0}
	};
	int opt, index = 0;

	while((opt = getopt_long(argc, argv, "", options, &index)) != -1){
		
		switch(index){
			case 0:
				ip = (char *)malloc(strlen(optarg)+1);
				strcpy(ip,optarg);
				ip[strlen(optarg)] = 0;
				break;
			case 1:
				port = atoi(optarg);
				break;
			case 2:
				if(strlen(optarg)>10){
					mvwprintw(gameboard, 0, 0, "too long username\n");
					wrefresh(gameboard);
					goto EXIT;
				}
				strcpy(username,optarg);
				break;
			case 3:
				if(strlen(optarg)>10){
					mvwprintw(gameboard, 0, 0, "too long password\n");
					wrefresh(gameboard);
					goto EXIT;
				}
				strcpy(password,optarg);
				break;
			case '?':
				opt = -1;
				break;
		}
	}

	//Login
	int sock_fd, s;
	if(!(sock_fd = makeConnection())){
		goto EXIT;
	}
	if(!(s=(send_bytes(sock_fd, (void*)&MODE_LOGIN, sizeof(int))))){
		fprintf(stderr, " [cannot send mode]\n");
		goto EXIT;
	if(!(s = send_bytes(sock_fd, (void*)&username, INFO_SIZE))){
	}
		fprintf(stderr, " [cannot send username]\n");
		goto EXIT;
	}
	if(!(s = send_bytes(sock_fd, (void*)&password, INFO_SIZE))){
		fprintf(stderr, " [cannot send password]\n");
		goto EXIT;
	}
	close(sock_fd);

	//make threads
	pthread_t input_pid;
	if(pthread_create(&input_pid, NULL, (void*)inputClient, (void*)inputwin)){
		perror("make new thread");
		mvwprintw(gameboard, 0, 0, " make thread failed\n");
		wrefresh(gameboard);
		goto EXIT;
	}
	pthread_t output_pid;
	if(pthread_create(&output_pid, NULL, (void*)outputClient, (void*)&output_wins)){
		perror("make new thread");
		mvwprintw(gameboard, 0, 0, " make thread failed\n");
		wrefresh(gameboard);
		goto EXIT;
	}
	refresh();

	//exit
	pthread_join(input_pid, NULL);
	noecho();

	if(isError){
		mvwprintw(gameboard, 0, 0, "%s", errorMsg);
		wrefresh(gameboard);
		goto EXIT;
	}
	mvwprintw(inputwin, 0, 0, " Good bye :)");
	
	EXIT:
	wprintw(gameboard, " press any key to exit... ");
	wrefresh(inputwin);
	wrefresh(gameboard);

	getch();

	free(ip);
	delwin(inputwin);
	delwin(gameboard);
	delwin(scoreboard);
  endwin();
	return 0;
}
