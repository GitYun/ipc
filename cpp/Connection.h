#ifndef CONNECTION_CPP_H
#define CONNECTION_CPP_H

#include <functional>

#define CONN_TYPE_ALL 1
#define CONN_TYPE_PID 2
#define CONN_TYPE_SUB 3

#define MAX_MSG_SIZE 1048576

namespace IPC{
	// typedef void (*ConnectionCallback)(IPC::Message msg);
	using ConnectionCallback = std::function<void (IPC::Message&)>;

	class Connection{
	public:
		Connection(const Connection &obj): ptr(obj.ptr), destroy(0), hasCallback(0) { }
		Connection(void* ptr_): ptr(ptr_), destroy(0), hasCallback(0) { }
		Connection(char* name, int type, int create=1);
		~Connection();

		char* getName();
		void startAutoDispatch();
		void stopAutoDispatch();
		void setCallback(const ConnectionCallback& cb);
		ConnectionCallback& getCallback();
		void removeCallback();
		intptr_t send(Message msg);
		void subscribe(char* subject);
		void removeSubscription(char* subject);
		void close();

	private:
		void* ptr;
		int destroy;
		int hasCallback;
	};
}

#endif
