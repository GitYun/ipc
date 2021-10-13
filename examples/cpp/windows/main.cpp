#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ipc_cpp.h"

using namespace IPC;

#include <Tlhelp32.h>
DWORD GetProcessIdByName(LPCTSTR lpszProcessName)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    if (Process32First(hSnapshot, &pe)) {
        do {
            if (lstrcmpi(lpszProcessName, pe.szExeFile) == 0) {
                CloseHandle(hSnapshot);
                return pe.th32ProcessID;
            }
        } while(Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return 0;
}

#define getppid() GetProcessIdByName(TEXT("cpp_reader.exe"))

int recived = 0;

void recive(Message msg){
	printf("Recived data: %s\n", msg.getData());

	recived++;
}

void reader(Connection& conn){
	conn.setCallback(recive);
	conn.startAutoDispatch();

	while (recived < 2) {
		usleep(100);
	}
	conn.stopAutoDispatch();
	printf("Not Listening\n");
	conn.startAutoDispatch();
	while (recived < 3) {
		usleep(100);
	}
	conn.stopAutoDispatch();

	conn.startAutoDispatch();
	conn.subscribe("DEMO-SUBJECT");
	while (recived < 4) {
		usleep(100);
	}

	conn.removeSubscription("DEMO-SUBJECT");
	printf("Next message wont be recieved in 5 seconds\n");
	sleep(5);
	conn.stopAutoDispatch();
	conn.close();
}

void writer(Connection& conn){
	{
		char* str = "This string was sent over IPC using named pipes!";
		Message msg(str, strlen(str)+1);
		msg.setPID(getppid());
		conn.send(msg);
	}

	{
		char* str = "This is the second message!";
		Message msg(str, strlen(str)+1);
		msg.setPID(getppid());
		conn.send(msg);
	}

	//Wait for dispatcher to actually stop
	for (int i = 0; i < 50; i++) {
		usleep(100);
	}

	{
		char* str = "Listening again!";
		Message msg(str, strlen(str)+1);
		msg.setPID(getppid());
		conn.send(msg);
	}

	//Wait for dispatcher to actually stop
	for (int i = 0; i < 50; i++) {
		usleep(100);
	}

	{
		char* str = "Subject Message 1";
		Message msg(str, strlen(str)+1);
		msg.setSubject("DEMO-SUBJECT");
		conn.send(msg);
	}

	//Wait for unsubscription
	for (int i = 0; i < 50; i++) {
		usleep(100);
	}

	{
		char* str = "Subject Message 2";
		Message msg(str, strlen(str)+1);
		msg.setSubject("DEMO-SUBJECT");
		conn.send(msg);
	}

	for (int i = 0; i < 50; i++) {
		usleep(100);
	}
}


int main(int argc, char const *argv[]) {
#if WRITER_PROCESS == 1
	Connection conn("ipcdemo", CONN_TYPE_PID, 0);

    writer(conn);
#else
    Connection conn("ipcdemo", CONN_TYPE_PID, 1);

    reader(conn);
#endif

	return 0;
}
