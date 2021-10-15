#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h> // has usleep()
#endif

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>

#include "message.h"
#include "connection.h"

#define MAX_CB 50

/**
 * @brief Number of a pipe maximum connection
 * 
 * @details Default a pipe maximum connection 10 subject,
 *          if the default size is exceeded, the automatic expansion
 */
#define SUBS_LEN 10

#ifdef _WIN32
    #define getpid GetCurrentProcessId
    #define THREAD_RET_TYPE DWORD WINAPI
    #define PIPE_PREFIX "\\\\.\\pipe\\"
#else
    #define THREAD_RET_TYPE void *
    #define PIPE_PREFIX "/tmp/"
#endif

#define PIPE_PREFIX_LEN strlen(PIPE_PREFIX) + 1

#ifdef _WIN32
void usleep(__int64 usec) {
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}
#endif

//TODO: Implement hashmap
struct ConnCallbackElement {
    Connection *conn;
    ConnectionCallback cb;
    TID tid;
    struct dispatcherArgs *args;
};

struct dispatcherArgs {
    ConnectionCallback cb; /**< Copies of \ref ConnCallbackElement structure member `cb' */
    int type;
#ifdef _WIN32
    HANDLE hPipe; /**< Copies of \ref Connection structure member `hPipe' */
#else
    char *path;
#endif
    int *cont; /**< continue: Used to indicate whether the scheduler thread is running; 1: running, 0: stop */
    char **subs; /**< Copies of \ref Connection structure member `subscriptions' */
    int *numSubs; /**< Copies of  \ref Connection structure member `numSubs' */
};

struct cbCallerArgs {
    Message *msg;
    ConnectionCallback cb;
};

struct writerArgs {
    Message *msg;
    char *path;
#ifdef _WIN32
    HANDLE hPipe;
#endif
};

struct ConnCallbackElement *cbs[MAX_CB];
int numCallbacks = 0;

static int nextEmpty(char **strs, int len) {
    int i;
    for (i = 0; i < len; i++) {
        if (strs[i] == NULL) {
            return i;
        }
    }
    return -1;
}

static int findEqual(char **strs, char *other, int len) {
    int i;
    if (!other) return -1;

    for (i = 0; i < len; i++) {
        if (strs[i] && strcmp(strs[i], other) == 0) {
            return i;
        }
    }
    return -1;
}

static THREAD_RET_TYPE writer(void *args) {
    struct writerArgs *argz = args;

#ifdef _WIN32
    // In `connectionConnect()', pipe has been opened, then `hPipe' is invaild;
    // If server unconnect the pipe, then `hPipe' is vaild and `argz->hPipe' is invaild
    HANDLE hPipe = CreateFile(TEXT(argz->path),
                              GENERIC_READ | GENERIC_WRITE,
                              0,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
    
    // When `hPipe' is vaild and `argz->hPipe' is invaild,
    // then store `hPipe' to `argz->hPipe'
    if (hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(argz->hPipe);
        argz->hPipe = hPipe;
    }
#else
    int fd = open(argz->path, O_WRONLY);
#endif

    size_t *name_len = argz->msg->data + argz->msg->len;
    size_t len = sizeof(Message) + argz->msg->len + sizeof(size_t) + *name_len;
    char *data = malloc(len);

    memcpy(data, argz->msg, sizeof(Message));
    memcpy((data + sizeof(Message)), argz->msg->data, len - sizeof(Message));

    if (argz->msg->type == CONN_TYPE_SUB) {
        data = realloc(data, len + sizeof(size_t) + strlen(argz->msg->subject) + 1);
        size_t sub_len = strlen(argz->msg->subject) + 1;
        memcpy((data + len), &sub_len, sizeof(size_t));
        memcpy((data + len + sizeof(size_t)), argz->msg->subject, strlen(argz->msg->subject) + 1);
        len += sizeof(size_t) + strlen(argz->msg->subject) + 1;
    }

#ifdef _WIN32
    DWORD nbr_writed;
    while (len > 0) {
        if (0 == WriteFile(argz->hPipe, data, len, &nbr_writed, NULL)) {
            break;
        }
        len -= nbr_writed;
    }
    // WriteFile(hPipe, data, len, NULL, NULL);
    // CloseHandle(hPipe);
#else
    write(fd, data, len);
    close(fd);
#endif

    free(data);
    free(argz->path);
    free(argz->msg->data);
    messageDestroy(argz->msg);
    free(argz);

#ifdef _WIN32
    _endthread();
#endif

    return 0;
}

static THREAD_RET_TYPE cbCaller(void *args) {
    struct cbCallerArgs *argz = args;

    (argz->cb)(argz->msg);

    if (argz->msg->type == CONN_TYPE_SUB) {
        free(argz->msg->subject);
    }

    free(argz->msg->data);
    free(argz->msg);
    free(argz);

#ifdef _WIN32
    _endthread();
#endif
    return 0;
}

static TID dispatch(ConnectionCallback cb, Message *msg) {
    struct cbCallerArgs *cbargs = malloc(sizeof(struct cbCallerArgs));
    cbargs->cb = cb;
    cbargs->msg = msg;

    TID tid = 0;
#ifdef _WIN32
    tid = _beginthread(cbCaller, 0, cbargs);
#else
    pthread_create(&tid, NULL, cbCaller, cbargs);
#endif

    return tid;
}

static THREAD_RET_TYPE dispatcher(void *args) {
    struct dispatcherArgs *argz = args;
    // TID* cbCallerTids = calloc(100, sizeof(TID));

#ifdef _WIN32
    // Don't block, because 'CreateNamedPipe()' use `PIPE_NOWAIT' attribute
    ConnectNamedPipe(argz->hPipe, NULL);
#else
    int fd = open(argz->path, O_RDONLY | O_NONBLOCK);
#endif

    while (*(argz->cont)) {
        Message *msg = malloc(sizeof(Message));

#ifdef _WIN32        
        int code = ReadFile(argz->hPipe, msg, sizeof(Message), NULL, NULL);
#else
        int code = read(fd, msg, sizeof(Message));

        if (code == -1) {
            code = 0;
        }
#endif

        char *data;
        if (code && msg->len < MAX_MSG_SIZE) {
            data = malloc(msg->len + sizeof(size_t));
#ifdef _WIN32
            ReadFile(argz->hPipe, data, msg->len + sizeof(size_t), NULL, NULL);
#else
            read(fd, data, msg->len + sizeof(size_t));
#endif

            size_t *name_len = data + msg->len;
            data = realloc(data, msg->len + sizeof(size_t) + *name_len);
#ifdef _WIN32
            ReadFile(argz->hPipe, data + msg->len + sizeof(size_t), *name_len, NULL, NULL);
#else
            read(fd, data + msg->len + sizeof(size_t), *name_len);
#endif

            msg->data = data;
        }
        else {
            free(msg);
            usleep(10);
            continue;
        }

        if (msg->type == CONN_TYPE_SUB) {
            size_t sub_len = 0;
#ifdef _WIN32
            DWORD readed_sub_len = 0;
            DWORD read_len = sizeof(size_t);
            while (read_len > 0) {
                if (0 == ReadFile(argz->hPipe, &sub_len, sizeof(size_t), &readed_sub_len, NULL)) {
                    break;
                }
                read_len -= readed_sub_len;
            }
#else
            read(fd, &sub_len, sizeof(size_t));
#endif

            char *subject = malloc(sub_len);
            if (subject) {
#ifdef _WIN32
            ReadFile(argz->hPipe, subject, sub_len, NULL, NULL);
#else
            read(fd, subject, *sub_len);
#endif
            }

            msg->subject = subject;
        }

        switch (msg->type) {
        case CONN_TYPE_ALL:
            dispatch(argz->cb, msg);
            break;
        case CONN_TYPE_SUB:
            if (findEqual(argz->subs, msg->subject, *(argz->numSubs)) != -1) {
                dispatch(argz->cb, msg);
            }
            else {
                free(msg->subject);
                free(msg);
                free(data);
            }
            break;
        case CONN_TYPE_PID:
            if (msg->pid == getpid()) {
                dispatch(argz->cb, msg);
            }
            else {
                free(msg);
                free(data);
            }
            break;
        default:
            free(msg);
            free(data);

            usleep(100);
        }
    }

#ifdef _WIN32
    FlushFileBuffers(argz->hPipe);
    DisconnectNamedPipe(argz->hPipe);
    _endthread();
#else
    close(fd);
#endif

    return 0;
}

static int findFreeCBSlot() {
    if (numCallbacks < MAX_CB) {
        int i;
        for (i = 0; i < MAX_CB; i++) {
            if (cbs[i] == NULL) {
                return i;
            }
        }
    }
    return -1;
}

static int findInCBSlot(Connection *conn) {
    int i;
    for (i = 0; i < MAX_CB; i++) {
        if (cbs[i] && cbs[i]->conn == conn) {
            return i;
        }
    }
    return -1;
}

Connection *connectionCreate(char *name, int type) {
    Connection *ret = malloc(sizeof(Connection));
    ret->name = malloc(strlen(name) + 1);
    memcpy(ret->name, name, strlen(name) + 1);
    ret->type = type;

    char *path = malloc(strlen(name) + 1 + PIPE_PREFIX_LEN);
    memcpy(path, PIPE_PREFIX, PIPE_PREFIX_LEN);
    strcat(path, name);

#ifdef _WIN32
    // WaitNamedPipe(TEXT(path), NULL);

    // Don't block, because use `PIPE_NOWAIT' attribute,
    // and also `ReadFile()', `ConnectNamedPipe()' don't block
    ret->hPipe = CreateNamedPipe(TEXT(path),
                                 PIPE_ACCESS_DUPLEX,
                                 PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
                                 1,
                                 1024 * 16,
                                 1024 * 16,
                                 NMPWAIT_USE_DEFAULT_WAIT,
                                 NULL);
#else
    mkfifo(path, 0777);
#endif

    free(path);

    ret->subscriptions = malloc(sizeof(char *) * SUBS_LEN);

    int i;
    for (i = 0; i < SUBS_LEN; i++) {
        ret->subscriptions[i] = NULL;
    }

    ret->numSubs = SUBS_LEN;

    return ret;
}

Connection *connectionConnect(char *name, int type) {
    Connection *ret = malloc(sizeof(Connection));
    ret->name = malloc(strlen(name) + 1);
    memcpy(ret->name, name, strlen(name) + 1);
    ret->type = type;

#ifdef _WIN32
    char *path = malloc(strlen(name) + 1 + PIPE_PREFIX_LEN);
    memcpy(path, PIPE_PREFIX, PIPE_PREFIX_LEN);
    strcat(path, name);

    ret->hPipe = CreateFile(TEXT(path),
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);

    free(path);
#endif

    ret->subscriptions = malloc(sizeof(char *) * SUBS_LEN);

    int i;
    for (i = 0; i < SUBS_LEN; i++) {
        ret->subscriptions[i] = NULL;
    }

    ret->numSubs = SUBS_LEN;

    return ret;
}

void connectionStartAutoDispatch(Connection *conn) {
    int i = findInCBSlot(conn);

    if (i == -1) return;

    cbs[i]->args = malloc(sizeof(struct dispatcherArgs));

    cbs[i]->args->cont = malloc(sizeof(int));
    *(cbs[i]->args->cont) = 1;

#ifdef _WIN32
    cbs[i]->args->hPipe = conn->hPipe;
#else
    cbs[i]->args->path = malloc(strlen(conn->name) + 1 + PIPE_PREFIX_LEN);
    memcpy(cbs[i]->args->path, PIPE_PREFIX, PIPE_PREFIX_LEN);
    strcat(cbs[i]->args->path, conn->name);
#endif

    cbs[i]->args->cb = cbs[i]->cb;

    cbs[i]->args->subs = conn->subscriptions;
    cbs[i]->args->numSubs = &(conn->numSubs);

#ifdef _WIN32
    cbs[i]->tid = (TID)_beginthread(dispatcher, 0, cbs[i]->args);
#else
    pthread_create(&(cbs[i]->tid), NULL, dispatcher, cbs[i]->args);
#endif
}

void connectionStopAutoDispatch(Connection *conn)
{
    int i = findInCBSlot(conn);

    if (i == -1) return;

    *(cbs[i]->args->cont) = 0;

#ifdef _WIN32
    WaitForSingleObject(cbs[i]->tid, INFINITE);
#else
    pthread_join(cbs[i]->tid, NULL);
#endif

    free(cbs[i]->args->cont);

#ifndef _WIN32
    free(cbs[i]->args->path);
#endif
    free(cbs[i]->args);
}

void connectionSetCallback(Connection *conn, ConnectionCallback cb) {
    int i = findFreeCBSlot();

    if (i != -1 && !cbs[i]) {
        cbs[i] = malloc(sizeof(struct ConnCallbackElement));
        cbs[i]->conn = conn;
        cbs[i]->cb = cb;
        ++numCallbacks;
    }
}

ConnectionCallback connectionGetCallback(Connection *conn) {
    int i = findInCBSlot(conn);

    if (i != -1) {
        return cbs[i]->cb;
    }

    return 0;
}

/* Call after connectionStopAutoDispatch() and before connectionDestroy() */
void connectionRemoveCallback(Connection *conn) {
    int i = findInCBSlot(conn);

    if (i != -1) {
        // In `connectionStopAutoDispatch()', the `cbs[i]->args' memory has been released
        free(cbs[i]);
        cbs[i] = NULL;
        --numCallbacks;
    }
}

TID connectionSend(Connection *conn, Message *msg) {
    char *path = malloc(strlen(conn->name) + 1 + PIPE_PREFIX_LEN);
    memcpy(path, PIPE_PREFIX, PIPE_PREFIX_LEN);
    strcat(path, conn->name);

    struct writerArgs *args = malloc(sizeof(struct writerArgs));
    args->msg = malloc(sizeof(Message));
    memcpy(args->msg, msg, sizeof(Message));

    size_t name_len = strlen(conn->name) + 1;
    args->msg->data = malloc(msg->len + sizeof(size_t) + name_len);
    memcpy(args->msg->data, msg->data, msg->len);
    memcpy(args->msg->data + msg->len, &name_len, sizeof(size_t));
    memcpy(args->msg->data + msg->len + sizeof(size_t), conn->name, name_len);

    if (msg->type == CONN_TYPE_SUB) {
        args->msg->subject = malloc(strlen(msg->subject) + 1);
        memcpy(args->msg->subject, msg->subject, strlen(msg->subject) + 1);
    }

    args->path = path;

    TID tid;
#ifdef _WIN32
    args->hPipe = conn->hPipe;
    tid = _beginthread(writer, 0, args);
#else
    pthread_create(&tid, NULL, writer, args);
#endif

    return tid;
}

void connectionSubscribe(Connection *conn, char *subject) {
    int i = nextEmpty(conn->subscriptions, conn->numSubs);

    // If the default size is exceeded, the automatic expansion
    if (i == -1) {
        conn->subscriptions = realloc(conn->subscriptions, conn->numSubs + SUBS_LEN);
        int i;
        for (i = conn->numSubs; i < conn->numSubs + SUBS_LEN; i++) {
            conn->subscriptions[i] = NULL;
        }
        conn->numSubs += SUBS_LEN;
        connectionSubscribe(conn, subject);
        return;
    }

    conn->subscriptions[i] = malloc(strlen(subject) + 1);
    memcpy(conn->subscriptions[i], subject, strlen(subject) + 1);
}

void connectionRemoveSubscription(Connection *conn, char *subject) {
    int i = findEqual(conn->subscriptions, subject, conn->numSubs);
    if (i >= 0) {
        free(conn->subscriptions[i]);
        conn->subscriptions[i] = NULL;
    }
}

void connectionClose(Connection *conn) {
#ifdef _WIN32
    if (conn->hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(conn->hPipe);
    }
#else
    char *path = malloc(strlen(conn->name) + 1 + PIPE_PREFIX_LEN);
    memcpy(path, PIPE_PREFIX, PIPE_PREFIX_LEN);
    strcat(path, conn->name);

    unlink(path);

    free(path);
#endif
}

void connectionDestroy(Connection *conn) {
    int i;
    for (i = 0; i < conn->numSubs; i++) {
        if (conn->subscriptions[i] != NULL) {
            free(conn->subscriptions[i]);
        }
    }
    free(conn->subscriptions);

    free(conn->name);
    free(conn);
}
