#include"headers.h"
//SERVER SIDE

static HANDLE gRiseEvent;
static ShareMemory* memshare;
enum class MsgType:int {
	RUN,
	STOP,
	EXIT,
	TASK1,
	TASK2,
};


class SIMPLEMSG{
public:
	MsgType   msgcode;//消息代码
	WPARAM wparam;//前参数
	LPARAM lparam;//后参数
	LPVOID lpReserved;//保留参数
};

void* servershareaddr = nullptr;
int MainThread();
int main() {
	gRiseEvent=CreateEventA(NULL, TRUE, FALSE, "ServerRiseEvent");//其实可转化为句柄值
	memshare = new ShareMemory("Process-sync");
	servershareaddr =memshare->OpenShareMem(NULL, 4096);
	auto mainthread= std::thread(&MainThread);
	//设置进程优先级到最高
	SetThreadPriority(mainthread.native_handle(), THREAD_PRIORITY_HIGHEST);
	mainthread.join();
	delete memshare;
	CloseHandle(mainthread.native_handle());
	CloseHandle(gRiseEvent);//释放掉申请的事件句柄
	
	return 0;
}

BOOL GetRemoteMessage(SIMPLEMSG* msg) {
	WaitForSingleObject(gRiseEvent, INFINITE);//等待接收到信号发出
	ResetEvent(gRiseEvent);
	memshare->ReadShareMem(servershareaddr, msg, sizeof(SIMPLEMSG));
	std::cout << "!Get Message" << std::endl;
	return (*msg).msgcode != MsgType::EXIT;
}




 
int MainThread() {

	SIMPLEMSG msg;
	while (GetRemoteMessage(&msg)){
		std::cout <<"MSG"<< (int)msg.msgcode << std::endl;
		std::cout <<"LPARAM"<< msg.lparam << std::endl;
	}
	return EXIT_SUCCESS;
}