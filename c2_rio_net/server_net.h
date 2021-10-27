#pragma once

#include "i_net_event.h"
#include "define.h"
#include ".\impl\server_net.h"
#include ".\impl\helper.h"

#include <vector>
#include <thread>
#include <cstdint>
#include <concurrent_queue.h>


namespace c2rionet
{	
	class ServerNetManager
	{
	public:
		ServerNetManager() = default;
		~ServerNetManager() = default;
		
	
		NetErrorCode Initialize(NetConfig net_cfg, INetEventManager* p_net_event_mgr) 
		{
			return impl_.Initialize(net_cfg, p_net_event_mgr);
		}

		void StartAndBlocking() 
		{
			impl_.StartAndBlocking();
		}
		
		void End() 
		{ 
			impl_.End(); 
		}
		
		NetErrorCode SendData(const std::int32_t session_index, const std::uint64_t session_uid, 
								const std::uint32_t data_size, const char* p_data) 
		{
			return impl_.SendData(session_index, session_uid, data_size, p_data);
		}

		void ForcedClose(const std::int32_t session_index, const std::uint64_t session_uid) 
		{
			impl_.ForcedClose(session_index, session_uid);
		}
		

	private:
		ServerNetManagerImpl impl_;
	};

}

//TODO 코드 확인: send는 애플리케이션에서 호출하면 즉시 보내나?
//TODO 리팩토링: modern c++ 20 
//TODO 기능 개선: 세션은 동일한 스레드에서 recv 요청과, recv, send의  완료 큐가 처리 되므로 lock을 걸리 않아도 되지 않나?
//TODO 기능 개선: send를 즉시 보내기와, 전용 스레드 사용 2가지 같이 구현 가능하나? 
//TODO 샘플 프로젝트: 채팅 서버
//TODO 샘플 프로젝트: 오목 서버
