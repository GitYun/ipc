#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "ipc.h"

Connection* conn;

int recieved = 0;
void callback(Message* msg){
    printf(msg->data);
    ++recieved;

    DWORD writed_size = 0;
    char* data = "Example Response Message\n";
    WriteFile(conn->hPipe, data, strlen(data) + 1, &writed_size, NULL);
}

int main (int argc, char* argv[])
{
    conn = connectionCreate("ExampleConnectionName", CONN_TYPE_SUB);
    connectionSubscribe(conn, "Example Subject");
    connectionSetCallback(conn, callback);
    connectionStartAutoDispatch(conn);

    while(recieved < 10){
        usleep(100);
    }

    connectionStopAutoDispatch(conn);
    connectionClose(conn);
    // connectionRemoveCallback(conn);
    connectionDestroy(conn);

    return 0;
}
