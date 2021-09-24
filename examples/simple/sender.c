#include <string.h>
#include "ipc.h"

int main (int argc, char* argv[])
{
    Connection* conn = connectionConnect("ExampleConnectionName", CONN_TYPE_SUB);
    char* data = "Example message data";
    Message* msg = messageCreate(data, strlen(data) +1);
    messageSetSubject(msg, "Example Subject");
    connectionSend(conn, msg);

    Sleep(1);
    messageDestroy(msg);
    connectionDestroy(conn);

    return 0;
}
