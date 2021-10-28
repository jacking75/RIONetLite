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

//코드 확인: 현재(2021-10-28)는 send는 애플리케이션에서 호출하면 즉시 보내고 있다.
 
//TODO 리팩토링: modern c++ 20 
 
//TODO 기능 개선: 애플리케이션에서 send 요청 시 애플리케이션 스레드에 wait가 발생하지 않도록 lock-free와 io 사용 금지가 필요하다.
//TODO 기능 개선: send를 즉시 보내기와, 전용 스레드 사용 2가지 같이 구현 가능하나? 

//TODO 테스트: 더미 클라 테스트를 통해 최대 접속 수를 100 정도로 한 상태에서 반복적으로 ForcedClose를 발생시켜서 에러 발생이 없는지 확인한다
//TODO 샘플 프로젝트: 채팅 서버
//TODO 샘플 프로젝트: 오목 서버


//완료
//TODO 기능 개선: 세션은 동일한 스레드에서 recv 관련 처리를 하므로 recv와 disconnect에서는 lock을 걸지 않는다.