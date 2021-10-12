#include <stdio.h>
#include <string.h>
#include "ipc.h"

int main (int argc, char* argv[])
{
    Connection* conn = connectionConnect("ExampleConnectionName", CONN_TYPE_SUB);

    char data[] = "Example message0 data\n";
    Message* msg = messageCreate(data, strlen(data) + 1);
    messageSetSubject(msg, "Example Subject");

    for (int i = 0; i < 10; ++i) {
        msg->data[15] = '0' + i;
        printf(msg->data);
        connectionSend(conn, msg);
        usleep(100);
    }

    messageDestroy(msg);
    connectionDestroy(conn);

    return 0;
}
