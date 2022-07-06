#pragma once
#include<Windows.h>
#include"ShareMemory.h"
#include<unordered_map>
#include"SimpleThreadPool.h"
#define SHARED_MEMORY_SIZE 8192
#define MAX_WAITINGTIME 1000
#if defined _WIN64
using UDWORD = DWORD64;
#else
using UDWORD = DWORD32;
#endif
enum class ManagerType :int {
	Server,
	Client,
};
enum class MsgType :int {
	RUN,
	STOP,
	EXIT,
	PRINT,
};
class SIMPLEMSG {
public:
	MsgType   msgcode;//消息代码
	WPARAM	  wparam;//前参数
	LPARAM	  lparam;//后参数
	LPVOID	  lpReserved;//保留参数
	bool      isCallBack;
	std::string callbackeventname;
	//BYTE	  m_Returndata[4096];
	DWORD64	  m_Returndata;
	SIMPLEMSG() = default;
	SIMPLEMSG(MsgType msg) {
		msgcode = msg;
	}
};
class MessageManager {
private:
	MessageManager() = default;
	bool  m_IsServer = false;
	HANDLE m_event = INVALID_HANDLE_VALUE;
	HANDLE gRiseEvent;
	ShareMemory* memshare;
	void* ServerShareMemory;
	std::unordered_map<MsgType, HANDLE> m_ClientCallBackEvents;
	HANDLE m_ServerCallBackEvent;
	bool b_m_ServerCallBackEvents = false;
	HANDLE GetClientCallBackHandle(SIMPLEMSG msg);
	std::thread m_Thread;
	MessageManager(const MessageManager&) = delete;
	MessageManager(MessageManager&&) = delete;
	void operator= (const MessageManager&) = delete;
	std::string makename(int number) { return "MessageManager" + std::to_string(number); }
	std::threadpool* m_threadpool;
public:
	DWORD64 tempvalue;
	virtual ~MessageManager() noexcept;
	void Constrsuct(ManagerType type);
	void SetManagerCharater(ManagerType type);
	bool IsServer();
	void ListenThread();
	bool SendLocalMessage(SIMPLEMSG msg);
	bool PostLocalMessage(SIMPLEMSG MSG);
	bool GetRemoteMessage(SIMPLEMSG* msg);
	void BindMsg(SIMPLEMSG rawmsg, std::function<UDWORD(LPARAM, WPARAM)> _fun);
	void DispatchMsg(SIMPLEMSG& msg);
	std::unordered_map<MsgType, std::function<UDWORD(LPARAM, WPARAM)>> m_MsgMap;//消息映射
	static MessageManager& GetInstance();
	void SetServerEvent(SIMPLEMSG msg);
};
inline HANDLE MessageManager::GetClientCallBackHandle(SIMPLEMSG msg) {
	auto it = m_ClientCallBackEvents.find(msg.msgcode);
	if (it != m_ClientCallBackEvents.end()) {
		return it->second;
	}else {
		if (m_IsServer) {
			HANDLE clientevent = OpenEventA(EVENT_ALL_ACCESS, FALSE, makename((int)msg.msgcode).c_str());
			m_ClientCallBackEvents.insert(std::make_pair(msg.msgcode, clientevent));
			return clientevent;
		}else {
			HANDLE thisevent = CreateEventA(NULL, TRUE, FALSE, makename((int)msg.msgcode).c_str());
			m_ClientCallBackEvents.insert(std::make_pair(msg.msgcode, thisevent));
			return thisevent;
		}
	}
}
inline void MessageManager::Constrsuct(ManagerType type)
{
	memshare = new ShareMemory("Process-sync");
	ServerShareMemory = memshare->OpenShareMem(NULL, SHARED_MEMORY_SIZE);
	if (type == ManagerType::Server) {
		gRiseEvent = CreateEventA(NULL, TRUE, FALSE, "ServerRiseEvent");//其实可转化为句柄值
		m_threadpool = new std::threadpool(4);
		for (int i = 0; i < 4; i++) {
			m_threadpool->commit(std::mem_fn(&MessageManager::ListenThread), this);
		}
		m_IsServer = true;
	}else {
		gRiseEvent = OpenEventA(EVENT_ALL_ACCESS, false, "ServerRiseEvent");
		m_IsServer = false;
	}
}
inline MessageManager& MessageManager::GetInstance() {
	static MessageManager msgmg;
	return msgmg;
}

inline MessageManager::~MessageManager() {
	delete memshare;
	if (m_IsServer) {
		CloseHandle(m_Thread.native_handle());
		CloseHandle(gRiseEvent);
		delete m_threadpool;
	}
	for (auto& it : m_ClientCallBackEvents) {//释放回调映射
		CloseHandle(it.second);
	}
	
}

inline void MessageManager::SetManagerCharater(ManagerType type) {
	m_IsServer = (type == ManagerType::Server);
}

inline bool MessageManager::IsServer() {
	return m_IsServer;
}
inline void MessageManager::ListenThread()
{
	SIMPLEMSG msg;
	while (GetRemoteMessage(&msg)) {
		DispatchMsg(msg);
	}
}
inline bool MessageManager::SendLocalMessage(SIMPLEMSG msg) {
	if (m_IsServer) {
		memshare->WriteShareMem(ServerShareMemory, &msg, sizeof(msg));
	}else {
		msg.isCallBack = true;
		HANDLE callbackevt = GetClientCallBackHandle(msg);
		msg.callbackeventname = makename((int)msg.msgcode).c_str();
		memshare->WriteShareMem(ServerShareMemory, &msg, sizeof(msg));
		SetServerEvent(msg);
		WaitForSingleObject(callbackevt, INFINITE);
		ResetEvent(callbackevt);
		memshare->ReadShareMem(ServerShareMemory, &msg, sizeof(SIMPLEMSG));
		tempvalue = msg.m_Returndata;
	}
	return true;
}
inline bool MessageManager::PostLocalMessage(SIMPLEMSG msg) {
	msg.isCallBack = false;
	memshare->WriteShareMem(ServerShareMemory, &msg, sizeof(msg));
	SetServerEvent(msg);
	return true;
}
inline bool MessageManager::GetRemoteMessage(SIMPLEMSG* msg) {
	if (m_IsServer) {
		WaitForSingleObject(gRiseEvent, INFINITE);//等待接收到信号发出
		ResetEvent(gRiseEvent);
		memshare->ReadShareMem(ServerShareMemory, msg, sizeof(SIMPLEMSG));
		return (*msg).msgcode != MsgType::EXIT;
	}
	return true;
}
inline void MessageManager::BindMsg(SIMPLEMSG rawmsg, std::function<UDWORD(LPARAM, WPARAM)> _fun) {
	m_MsgMap.insert(std::make_pair(rawmsg.msgcode, _fun));
}
inline void MessageManager::DispatchMsg(SIMPLEMSG& msg) {//处理收到的消息
	auto result = m_MsgMap.find(msg.msgcode);
	if (result != m_MsgMap.end()) {
		if (msg.isCallBack) {
			auto ret = (*result).second(msg.lparam, msg.wparam);
			msg.m_Returndata = ret;
			memshare->WriteShareMem(ServerShareMemory, &msg, sizeof(SIMPLEMSG));
			SetEvent(GetClientCallBackHandle(msg));
		}else {
			(*result).second(msg.lparam, msg.wparam);
		}
	}
}
inline void MessageManager::SetServerEvent(SIMPLEMSG msg) {
	if (b_m_ServerCallBackEvents) {
		SetEvent(m_ServerCallBackEvent);//已经被打开过的时间
	}else {
		b_m_ServerCallBackEvents = true;
		m_ServerCallBackEvent = OpenEventA(EVENT_ALL_ACCESS, FALSE, "ServerRiseEvent");
		if (m_ServerCallBackEvent != INVALID_HANDLE_VALUE) {
			SetEvent(m_ServerCallBackEvent);
		}
	}
}