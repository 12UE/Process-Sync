
#include<time.h>
#include<shared_mutex>
#include"../进程间同步-server/MessageManager.h"
//client
bool isrunning = true;
MessageManager& g_client = MessageManager::GetInstance();
DWORD64 FPS = 0X0;
std::shared_mutex mtx;
DWORD64 getfps() {
	DWORD _time = clock();
	static float lasttime = 0;
	static int fpscounter = 0;
	if (_time - lasttime > 1000) {
		std::lock_guard<std::shared_mutex> lock(mtx);
		FPS = fpscounter;
		fpscounter = 0;
		lasttime = _time;
	}
	else {
		fpscounter++;
	}
	return FPS;
}
void sendthread() {
	SIMPLEMSG msg(MsgType::PRINT);
	while (true) {
		g_client.SendLocalMessage(msg);
		getfps();
	}
	return;
}

int main() {
	g_client.Constrsuct(ManagerType::Client);
	for (int i = 0; i < std::thread::hardware_concurrency(); i++) {
		std::thread mainthread = std::thread(&sendthread);
		mainthread.detach();
		SetThreadPriority(mainthread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
	}
	std::cout << "按下F2 查看当前每秒处理次数"<< std::endl;
	while (!GetAsyncKeyState(VK_F1)) {
		if (GetAsyncKeyState(VK_F2)) {
			std::cout <<"处理次数" << FPS << std::endl;
		}
		Sleep(20);
	}
	return 0;
}