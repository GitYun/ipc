#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "ipc.h"

int recieved = 0;
void callback(Message* msg){
    printf(msg->data);
    recieved = 1;
}

int main (int argc, char* argv[])
{
    Connection* conn = connectionCreate("ExampleConnectionName", CONN_TYPE_SUB);
    connectionSubscribe(conn, "Example Subject");
    connectionSetCallback(conn, callback);
    connectionStartAutoDispatch(conn);

    while(!recieved){
        // usleep(100);
        Sleep(1);
    }

    connectionStopAutoDispatch(conn);
    connectionClose(conn);
    connectionRemoveCallback(conn);
    connectionDestroy(conn);

    return 0;
}
