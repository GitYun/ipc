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

int main(int argc, char const *argv[]) {
	int recived = 0;

#if WRITER_PROCESS == 1
	Connection conn1("ipcdemo", CONN_TYPE_PID, 0);
	Connection conn2("ipcdemo2", CONN_TYPE_PID, 1);

	conn2.setCallback([&recived](IPC::Message msg) {
        printf("Recived data: %s\n", msg.getData());
	    recived++;
    });
	conn2.startAutoDispatch();

    DWORD pid = GetProcessIdByName(TEXT("cpp_multi_reader.exe"));

	{
		char* str = "This string was sent over IPC using named pipes!";
		Message msg(str, strlen(str)+1);
		msg.setPID(pid);
		conn1.send(msg);
	}

	{
		char* str = "This is the second message!";
		Message msg(str, strlen(str)+1);
		msg.setPID(pid);
		conn1.send(msg);
	}

	while (recived < 2) {
		usleep(100);
	}

	conn2.stopAutoDispatch();

	conn1.close();
	conn2.close();

#else
    Connection conn1("ipcdemo", CONN_TYPE_PID, 1);
    Connection* conn2;

	conn1.setCallback([&recived, &conn2](IPC::Message msg) {
        printf("Recived1 data: %s\n", msg.getData());
	    recived++;

		if (recived == 1) {
			conn2 = new Connection("ipcdemo2", CONN_TYPE_PID, 0);
		}
    });
	conn1.startAutoDispatch();

	while (recived < 2) {
		usleep(100);
	}

    DWORD pid = GetProcessIdByName(TEXT("cpp_multi_writer.exe"));

	{
		char* str = "This string was sent over IPC using named pipes!";
		Message msg(str, strlen(str)+1);
		msg.setPID(pid);
		conn2->send(msg);
	}

	usleep(100);

	{
		char* str = "This is the second message!";
		Message msg(str, strlen(str)+1);
		msg.setPID(pid);
		conn2->send(msg);
	}

	usleep(100);

	conn1.stopAutoDispatch();

	conn1.close();
	conn2->close();

#endif

	return 0;
}
