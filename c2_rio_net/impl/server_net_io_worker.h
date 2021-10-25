#pragma once

#include "tcp_session.h"
#include "session_manager.h"
#include "server_net_lazyed_dis_conn_manager.h"
#include "helper.h"
#include "../i_net_event_manager.h"
#include "../define.h"

#include <vector>
#include <functional>
#include <thread>
#include <cstdint>

namespace c2rionet
{
	class ServerNetIOWorker
	{
	public:
		ServerNetIOWorker() = default;
		~ServerNetIOWorker() = default;

		std::function<void(TcpSession*, const uint64_t, const DisconnectReason)> DisConnectFunc;

		void Init(ServerNetLazyedDisConnManager* p_lazyed_dis_conn_mgr, 
					SessionManager* p_session_mgr, 
					INetEventManager* p_net_event_mgr)
		{
			pref_lazyed_dis_conn_mgr_ = p_lazyed_dis_conn_mgr;
			pref_session_mgr_ = p_session_mgr;
			pref_net_event_mgr_ = p_net_event_mgr;
		}


		bool StartIoThreads(const std::uint32_t rio_thread_count,
			const std::uint32_t max_send_count_per_1sec,
			const std::uint32_t max_connection_count,
			const std::uint32_t io_work_thread_count, 
			std::vector<RIO_CQ>& rio_completion_queues)
		{
			auto session_count_per_rio_thread = max_connection_count / io_work_thread_count;

			const unsigned int maxCQSizePerRIOThread = (1 + max_send_count_per_1sec) * session_count_per_rio_thread;

			for (std::uint32_t i = 0; i < rio_thread_count; ++i)
			{
				auto rioQueue = RIO.RIOCreateCompletionQueue(maxCQSizePerRIOThread, 0);
				if (rioQueue == RIO_INVALID_CQ) {
					return false;
				}
				else {
					rio_completion_queues.push_back(rioQueue);
				}

				auto threadIndex = i;
				io_work_threads_.emplace_back([threadIndex, &rio_completion_queues, this]() {
						IoWorkerThread(threadIndex, rio_completion_queues);
					});

				// mIOWorkThreads 내부에서 초기화 작업이 있어서 잠시 대기한다
				std::this_thread::sleep_for(std::chrono::milliseconds(32));
			}

			return true;
		}

		void Stop()
		{
			is_run_ = true;

			for (auto& thread : io_work_threads_)
			{
				if (thread.joinable())
				{
					thread.join();
				}
			}
		}

		void IoWorkerThread(const int thread_index, std::vector<RIO_CQ>& rio_completion_queues)
		{
			auto threadIndex = thread_index;
			auto coreCount = Helper::GetNumberOfProcessors();
			auto results = new RIORESULT[coreCount];

			IoWorkerThread_startLog(threadIndex, coreCount);

			pref_lazyed_dis_conn_mgr_->AddSessionQueue();

			while (is_run_)
			{
				memset(results, 0, sizeof(results));
				ULONG numResults = RIO.RIODequeueCompletion(rio_completion_queues[threadIndex], results, coreCount);

				if (RIO_CORRUPT_CQ == numResults)
				{
					LogFuncPtr((int)LogLevel::kFATAL, "RIONetManager::IoWorkerThread() - RIO CORRUPT CQ");
					continue;
				}

				for (ULONG i = 0; i < numResults; ++i)
				{
					auto context = reinterpret_cast<RioIoContext*>(results[i].RequestContext);
					auto pSession = pref_session_mgr_->GetSessionObj(context->session_index_);
					auto transferred = results[i].BytesTransferred;

					IoWorkerThread_ProceRIORequest(transferred, context, pSession);
				}

				pref_lazyed_dis_conn_mgr_->ProcessLazyedDisConnectSession(threadIndex, pref_session_mgr_);

				if (0 == numResults) { Sleep(1); }///< for low cpu-usage
			}

			delete results;

			IoWorkerThread_endLog(threadIndex);
			return;
		}

		void IoWorkerThread_ProceRIORequest(const ULONG transferred, RioIoContext* p_context, TcpSession* p_session)
		{
			auto sessionUniqueIdByRioConext = p_context->session_unique_Id_;

			if (transferred == 0)
			{
				DisConnectFunc(p_session, sessionUniqueIdByRioConext, DisconnectReason::kZERO_COMPLETION);
			}
			else if (IOType::kRECV == p_context->io_type_)
			{
				RecvCompletion(p_session, transferred);

				if (auto result = p_session->PostRecv(); result != NetErrorCode::kNONE)
				{
					if (result == NetErrorCode::kRECV_RIO_FAIL)
					{
						DisConnectFunc(p_session, sessionUniqueIdByRioConext, DisconnectReason::kIO_REQUEST_RECV_ERROR);
					}
				}
			}
			else if (IOType::kSEND == p_context->io_type_)
			{
				auto postDataLen = p_context->Length;
				p_session->SendCompletion(p_context);

				if (postDataLen != transferred)
				{
					DisConnectFunc(p_session, sessionUniqueIdByRioConext, DisconnectReason::kPARITAL_SEND_COMPLETION);
				}
			}
			else
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "RIONetManager::IoWorkerThread() - Unknown I/O Type: %d", p_context->io_type_);
				LogFuncPtr((int)LogLevel::kERROR, logmsg);
			}
		}

		// 받기 버퍼의 읽은 위치도 여기서 이동 시키므로 링버퍼의 데이터가 겹치는 것을 막고 싶으면 애플리케이션에서 OnReceive의 데이터를 다 사용할 때까지 잘 보관해야 한다.
		void RecvCompletion(TcpSession* p_session, const UINT32 transferred)
		{
			p_session->RecvCompletion(transferred);

			auto [readSize, pReadData] = p_session->ReadRecvBuffer();
			auto usedSize = pref_net_event_mgr_->OnReceive(p_session->GetIndex(),
															p_session->GetUniqueId(),
															readSize,
															pReadData);
			p_session->IncremantReadPosRecvBuffer(usedSize);
		}

		// logs
		void IoWorkerThread_startLog(const int thread_index, const int core_count)
		{
			char logmsg[256] = { 0, };
			sprintf_s(logmsg, "RIONetManager::IoWorkerThread() - Start. threadIndex: %d, coreCount: %d", thread_index, core_count);
			LogFuncPtr((int)LogLevel::kINFO, logmsg);
		}

		void IoWorkerThread_endLog(const int threadIndex_)
		{
			char logmsg[256] = { 0, };
			sprintf_s(logmsg, "RIONetManager::IoWorkerThread() - End. threadIndex: %d", threadIndex_);
			LogFuncPtr((int)LogLevel::kINFO, logmsg);
		}

	private:
		bool is_run_ = false;

		ServerNetLazyedDisConnManager* pref_lazyed_dis_conn_mgr_ = nullptr;
		SessionManager* pref_session_mgr_ = nullptr;
		INetEventManager* pref_net_event_mgr_ = nullptr;

		std::vector<std::thread> io_work_threads_;
	};
}