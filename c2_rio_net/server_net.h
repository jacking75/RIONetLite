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

//코드 확인: 애플리케이션에서 send 요청 시 즉시 보내는 방법에서는 
/* 1. 세션 단위로 lock을 걸고 있고, 동시 요청확률이 낮다. 
   2. RIO는 요청 시에 커널 컨텍스트 스위칭을 하지 않으므로 대기 발생이 거의 없다. 
   즉 즉시 보낼 때 애플리케이션 층에 대기를 발생할 확률이 낮은편이다.  
   1-send가 아니므로 완료 시에 요청할 때만큼 다 보내지 못한 경우 네트워크 버퍼가 찬 것으로 예상하여 연결을 끊도록 한다.
*/


//TODO 리팩토링: modern c++ 20  
//TODO 샘플 프로젝트: 채팅 서버
//TODO 샘플 프로젝트: 오목 서버
//TODO 더미 테스트: 더미 클라 테스트를 통해 최대 접속 수를 100 정도로 한 상태에서 반복적으로 ForcedClose를 발생시켜서 에러 발생이 없는지 확인한다


//완료
//기능 개선: 세션은 동일한 스레드에서 recv 관련 처리를 하므로 recv와 disconnect에서는 lock을 걸지 않는다.
//기능 개선: send를 즉시 보내기와, 전용 스레드 사용 2가지 같이 구현 가능하나?
//기능 테스트: 간단하게 클라 1,2개 접속 후 에코 테스트 완료