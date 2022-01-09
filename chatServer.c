#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include "chatServer.h"

#define MAX_SIZE 4096

static int end_server = 0;

void intHandler(int SIG_INT) {
    end_server = 1;
	/* use a flag to end_server to break the main loop */
}

int main (int argc, char *argv[])
{

    if(argc != 2) {
        printf("Usage: chatServer <port>\n");
        exit(1);
    }
	
	signal(SIGINT, intHandler);
   
	conn_pool_t* pool = malloc(sizeof(conn_pool_t));
	init_pool(pool);
   
    int main_socket;
    if((main_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    int port = atoi(argv[1]);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    int on = 1;
    ioctl(main_socket, (int)FIONBIO, (char *)&on);

	int is_bind;
    int is_listen;
    is_bind = bind(main_socket, (struct sockaddr *) &server, sizeof(server));

    if (is_bind != 0) {
        perror("error: bind\n");
        exit(1);
    }

    is_listen = listen(main_socket, 5);
    if (is_listen != 0) {
        perror("error: listen\n");
        exit(1);
    }

    FD_SET(main_socket, &pool->read_set);
    pool->maxfd = main_socket;

	do
	{
		pool->ready_read_set = pool->read_set;
        pool->ready_write_set = pool->write_set;
        printf("Waiting on select()...\nMaxFd %d\n", pool->maxfd);
		pool->nready = select(pool->maxfd+1, &pool->ready_read_set, &pool->ready_write_set, NULL, NULL);
        if(pool->nready < 0) {
            continue;
        }
		
		for (int i = 0; i<pool->maxfd+1; i++)
		{
            int sd;
			if (FD_ISSET(i, &pool->ready_read_set))
			{
				if(i == main_socket) {
                    struct sockaddr_in client;
                    socklen_t peeradr_size = sizeof(client);
                    sd = accept(main_socket, (struct sockaddr *) &client, &peeradr_size);
                    if(sd<0){
                        perror("error: accept\n");
                        exit(1);
                    }
                    printf("New incoming connection on sd %d\n", sd);
                    add_conn(sd, pool);
                }
				else {
                    ssize_t temp;
                    char buffer[MAX_SIZE];
                    memset(buffer, 0, MAX_SIZE);
                    printf("Descriptor %d is readable\n", i);
                    temp = read(i, buffer, MAX_SIZE);
                    if(temp != 0) {
                        printf("%d bytes received from sd %d\n", (int) temp, i);
                    }
                    if(temp == 0) {
                        printf("Connection closed for sd %d\n",i);
                        remove_conn(i, pool);
                        close(i);
                        FD_CLR(i, &pool->read_set);
                    } else {
                        add_msg(i, buffer, (int)temp, pool);
                    }
                }
			} 
			if (FD_ISSET(i, &pool->ready_write_set)) {
				/* try to write all msgs in queue to sd */
                write_to_client(i, pool);
		 	}
		 if(pool->nready == 0) {
             break;
         }
		 
      } 

   } while (end_server == 0);
        conn_t* temp = pool->conn_head;
        conn_t* prev=temp;
        while (temp!=NULL){
            msg_t* temp2=temp->write_msg_head;
            msg_t* prev2=temp2;
            while (temp2!=NULL) {
                prev2=temp2;
                temp2=temp2->next;
                free(prev2->message);
                free(prev2);
            }
            prev=temp;
            temp=temp->next;
        }

        temp=pool->conn_head;
        prev=temp;
        while (temp!=NULL) {
            printf("removing connection with sd %d \n", temp->fd);
            prev=temp;
            temp=temp->next;
            free(prev);
        }
        free(pool);
        return 0;

}


int init_pool(conn_pool_t* pool) {
    pool->maxfd = 0;
    pool->nready = 0;
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->ready_read_set);
    FD_ZERO(&pool->write_set);
    FD_ZERO(&pool->ready_write_set);
    pool->conn_head=NULL;
    pool->nr_conns = 0;
	return 0;
}

int add_conn(int sd, conn_pool_t* pool) {
    if(pool->conn_head == NULL) {
        pool->conn_head = (conn_t*) malloc(sizeof(conn_t));
        pool->conn_head->write_msg_head=NULL;
        pool->conn_head->write_msg_tail=NULL;
        pool->conn_head->prev=NULL;
        pool->conn_head->next=NULL;
        pool->conn_head->fd = sd;
    }
    else {
        conn_t *curr, *before;
        curr = pool->conn_head;
        while(curr != NULL) {
            before = curr;
            curr = curr->next;
        }
        curr = (conn_t*) malloc((sizeof(conn_t)));
        curr->write_msg_head=NULL;
        curr->write_msg_tail=NULL;
        curr->next=NULL;
        curr->fd = sd;
        before->next=curr;
    }
    pool->nr_conns += 1;
    FD_SET(sd, &pool->read_set);
    if(sd>pool->maxfd) {
        pool->maxfd=sd;
    }

	return 0;
}

int remove_conn(int sd, conn_pool_t* pool) {
    conn_t *curr = pool->conn_head;
    if (curr != NULL && curr->fd == sd)
    {
        pool->conn_head = curr->next;
        FD_CLR(sd, &pool->read_set);
        FD_CLR(sd, &pool->write_set);
        free(curr);
        if (pool->maxfd==sd)
        {
            pool->maxfd--;
        }
        return 0;
    }
    conn_t* prev=NULL;
    while (curr != NULL && curr->fd != sd) {
        prev=curr;
        curr = curr->next;
    }
    if (curr == NULL)
        return -1;

    if (prev)
    {
        prev->next=curr->next;
    }

    // Unlink the node from linked list
    FD_CLR(sd, &pool->read_set);
    FD_CLR(sd, &pool->write_set);
    if (pool->maxfd==sd)
    {
        pool->maxfd--;
    }
    pool->nr_conns--;
    close(sd);
    free(curr);
    return 0;
}

int add_msg(int sd,char* buffer,int len,conn_pool_t* pool) {
    len = (int)strlen(buffer);
    conn_t *curr = pool->conn_head;
    msg_t *temp;
    while (curr != NULL) {
        if (curr->fd != sd) {
            temp = curr->write_msg_head;
            if (temp == NULL) {
                curr->write_msg_head = malloc(sizeof(msg_t));
                curr->write_msg_head->message = malloc(len + 1);
                curr->write_msg_head->prev=NULL;
                curr->write_msg_head->next=NULL;
                strncpy(curr->write_msg_head->message, buffer, len);
                curr->write_msg_head->size = len;
                FD_SET(curr->fd, &pool->write_set);
            } else {
                msg_t *before = NULL;
                while (temp != NULL) {
                    before = temp;
                    temp = temp->next;
                }
                temp = malloc(sizeof(msg_t));
                temp->prev=NULL;
                temp->next=NULL;
                temp->message = malloc(len + 1);
                strncpy(temp->message, buffer, len);
                temp->size = len;
                before->next = temp;
                FD_SET(curr->fd, &pool->write_set);
            }
        }
        curr = curr->next;
    }

	return 0;
}

int write_to_client(int sd,conn_pool_t* pool) {
    conn_t *curr = pool->conn_head;
    msg_t *temp;
    while (curr != NULL) {
        if(curr->fd == sd) {
            temp = curr->write_msg_head;
            if (temp == NULL) {
                curr=curr->next;
                continue;
            }
            msg_t* prev=temp;
            while (temp != NULL) {
                write(sd, temp->message, temp->size);
                prev=temp;
                temp = temp->next;
                free(prev->message);
                free(prev);
            }

            curr->write_msg_head = NULL;
            curr->write_msg_tail = NULL;
        }
        curr = curr->next;
    }
    FD_CLR(sd, &pool->write_set);
	return 0;
}

