#include"headers.h"
//SERVER SIDE
MessageManager &messageManager = MessageManager::GetInstance();
int a = 3;
UDWORD Foo(LPARAM lp, WPARAM wp) {
	std::cout << "time: " << clock() << std::endl;
	return a++;
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






