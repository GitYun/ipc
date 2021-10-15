#ifndef CONNECTION_H
#define CONNECTION_H

#ifdef _WIN32
#include <windows.h>
#define TID HANDLE
void usleep(__int64 usec);
#else
#define TID pthread_t
#endif

typedef enum {
	CONN_TYPE_ALL = 1, /**< */ 
	CONN_TYPE_PID = 2, /**< use PID link to named pipe */
	CONN_TYPE_SUB = 3, /**< use subject's name link to named pipe */
} ConnctionType;

#define MAX_MSG_SIZE 1048576

/**
 * @brief connection information
 * 
 * @details connection name and type, subject scriptions
 */
typedef struct {
	char* name; /**< Connection name */
	int type; /**< Connection type \ref ConnectionType */
	char** subscriptions; /**< Subscriber name string array, the maximum number is specified by \ref numSubs member */
	int numSubs; /**< Number of connection subject */
#ifdef _WIN32
	HANDLE hPipe; /**< pipe handle on the windows OS */
#endif
} Connection;

typedef struct sMessage Message;
typedef void (*ConnectionCallback)(Message* msg);

/**
 * @brief Create IPC connection information structure instace for communictation recvier
 * @author vemagic (admin@vamgic.com)
 * @param name IPC connection name string
 * @param type IPC connection type \ref ConnectionType
 * @return Connection* Pointer to a connection instance
 */
Connection* connectionCreate(char* name, int type);

/**
 * @brief Create IPC connection information structure instace for communictation sender
 * @author vemagic (admin@vamgic.com)
 * @param name IPC connection name string
 * @param type IPC connection type \ref ConnectionType
 * @return Connection* Pointer to a connection instance
 */
Connection* connectionConnect(char* name, int type);

/**
 * @brief Create and run internal dispatcher thread
 * @details Copy `conn' to dispatcher data structure object,
 * 		    then create a thread to run dispatcher
 * @author vemagic (admin@vamgic.com)
 * @param conn Pointer to a connection instance
 */
void connectionStartAutoDispatch(Connection* conn);

/**
 * @brief Stop and destory internal dispatcher thread
 * @author vemagic (admin@vamgic.com)
 * @param conn Pointer to a connection instance
 */
void connectionStopAutoDispatch(Connection* conn);

/**
 * @brief Set connection callback handler
 * @author vemagic (admin@vamgic.com)
 * @param conn Pointer to a connection instance
 * @param cb Callback hanler
 */
void connectionSetCallback(Connection* conn, ConnectionCallback cb);

ConnectionCallback connectionGetCallback(Connection* conn);

/**
 * @brief Remove subscriber callback from a connection
 * @author vemagic (admin@vamgic.com)
 * @param conn Pointer to a connection instance
 * @param subject Subscriber name string
 * @note Call it must after connectionStopAutoDispatch() and before connectionDestroy() 
 */
void connectionRemoveCallback(Connection* conn);

/**
 * @brief Send message to subscriber
 * @author vemagic (admin@vamgic.com)
 * @param conn Pointer to a connection instance
 * @param msg Subscriber name string
 * @return A thread id
 */
TID connectionSend(Connection* conn, Message* msg);

/**
 * @brief Set a subscriber name
 * @author vemagic (admin@vamgic.com)
 * @param conn Pointer to a connection instance
 * @param subject Subscriber name string
 */
void connectionSubscribe(Connection* conn, char* subject);

/**
 * @brief Remove subscriber from a connection
 * @author vemagic (admin@vamgic.com)
 * @param conn Pointer to a connection instance
 * @param subject Subscriber name string
 */
void connectionRemoveSubscription(Connection* conn, char* subject);

/**
 * @brief Destroy all resource of a \ref Connection instance
 * @author vemagic (admin@vamgic.com)
 * @param conn Pointer to a connection instance
 */
void connectionDestroy(Connection* conn);

/**
 * @brief Close the pipe handle(on the Windows OS) or file(on the *nix OS)
 * @details Only used in *nix OS (use unlink(filename) to delete pipe file)
 * @author vemagic (admin@vamgic.com)
 * @param conn Pointer to a connection instance
 * @note Call it must before connectionDestroy()
 */
void connectionClose(Connection* conn);

#endif
