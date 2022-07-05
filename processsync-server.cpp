#include"headers.h"
//SERVER SIDE
MessageManager &messageManager = MessageManager::GetInstance();
int a = 3;
UDWORD Foo(LPARAM lp, WPARAM wp) {
	DWORD _time = clock();
	static float lasttime = 0;
	static int fpscounter = 0;
	if (_time - lasttime > 1000) {
		a = fpscounter;
		fpscounter = 0;
		lasttime = _time;
	}else {
		fpscounter++;
	}
	return a;
}
int main() {
	SIMPLEMSG msg;
	msg.isCallBack = true;
	msg.msgcode = MsgType::PRINT;
	messageManager.BindMsg(msg, Foo);
	messageManager.Constrsuct(ManagerType::Server);
	while (!GetAsyncKeyState(VK_F1)) {
		
		Sleep(20);
	}
	return 0;
}






