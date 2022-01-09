#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

typedef struct conn_pool { 
        /* Largest file descriptor in this pool. */
        int maxfd;                      
        /* Number of ready descriptors returned by select. */
        int nready;                     
        /* Set of all active descriptors for reading. */
        fd_set read_set;                
        /* Subset of descriptors ready for reading. */
        fd_set ready_read_set;  
        /* Set of all active descriptors for writing. */
        fd_set write_set;               
        /* Subset of descriptors ready for writing.  */
        fd_set ready_write_set; 
        /* Doubly-linked list of active client connection objects. */
        struct conn *conn_head;
        /* Number of active client connections. */
        unsigned int nr_conns;
        
}conn_pool_t;

typedef struct msg {
        struct msg *prev;
        struct msg *next;
        char *message;
        int size;
}msg_t;

typedef struct conn {
        struct conn *prev;      
        struct conn *next;      
        int fd;                 
        /* 
         * Pointers for the doubly-linked list of messages that
         * have to be written out on this connection.
         */
        struct msg *write_msg_head;
	struct msg *write_msg_tail;
}conn_t;


/*
 * Init the conn_pool_t structure. 
 * @pool - allocated pool 
 * @ return value - 0 on success, -1 on failure 
 */
int init_pool(conn_pool_t* pool);



/*
 * Add connection when new client connects the server. 
 * @ sd - the socket descriptor returned from accept
 * @pool - the pool 
 * @ return value - 0 on success, -1 on failure 
 */
int add_conn(int sd, conn_pool_t* pool);

/*
 * Remove connection when a client closes connection, or clean memory if server stops. 
 * @ sd - the socket descriptor of the connection to remove
 * @pool - the pool 
 * @ return value - 0 on success, -1 on failure 
 */
int remove_conn(int sd, conn_pool_t* pool);

/*
 * Add msg to the queues of all connections (except of the origin). 
 * @ sd - the socket descriptor to add this msg to the queue in its conn object
 * @ buffer - the msg to add
 * @ len - length of msg
 * @pool - the pool 
 * @ return value - 0 on success, -1 on failure 
 */
int add_msg(int sd,char* buffer,int len,conn_pool_t* pool);


/*
 * Write msg to client. 
 * @ sd - the socket descriptor of the connection to write msg to
 * @pool - the pool 
 * @ return value - 0 on success, -1 on failure 
 */
int write_to_client(int sd,conn_pool_t* pool);

#endif
