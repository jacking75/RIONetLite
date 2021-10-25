#pragma once

#include <vector>
#include <thread>
#include <concurrent_queue.h>

#include "i_net_event_manager.h"
#include ".\impl\server_net_manager.h"
#include ".\impl\helper.h"
#include "define.h"



namespace c2rionet
{	
	class ServerNetManager
	{
	public:
		ServerNetManager() = default;
		~ServerNetManager() = default;
		
	
		NetErrorCode Initialize(NetConfig net_cfg, INetEventManager* p_net_event_mgr) {
			return impl_.Initialize(net_cfg, p_net_event_mgr);
		}

		void StartAndBlocking() { 
			impl_.StartAndBlocking();
		}
		
		void End() { 
			impl_.End(); 
		}
		
		NetErrorCode SendData(const INT32 session_index, const UINT64 session_uid, const UINT32 data_size, const char* p_data) {
			return impl_.SendData(session_index, session_uid, data_size, p_data);
		}

		void ForcedClose(const INT32 session_index, const UINT64 session_uid) {
			impl_.ForcedClose(session_index, session_uid);
		}
		

	private:
		ServerNetManagerImpl impl_;
	};

}

//TODO 리팩토링 및 기능 추가
/*
- 리팩토링
 - 코딩 스타일 지키기
 - modern c++ 20
 - 파일 분할
  - io_work 분할
 - c++의 정수 타입 사용하기  https://www.enseignement.polytechnique.fr/informatique/INF478/docs/Cpp/en/cpp/types/integer.html
- 기능 개선
 - 스케일업에 대응
- 샘플 프로젝트
 - 채팅 서버
 - 오목 서버
 - 메타버스
*/

/*
- 2021.10.24 std::option 이 VS 2019에서 성능 이슈가 있음. 사용하지 않은 것과 비교해서 10배 정도 느림. 그래서 여기서는 사용하지 않음.
*/