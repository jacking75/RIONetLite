#pragma once

#include "tcp_session.h"
#include "session_manager.h"
#include "send_manager.h"
#include "server_net_lazyed_dis_conn_manager.h"
#include "helper.h"
#include "../i_net_event.h"
#include "../define.h"

#include <vector>
#include <functional>
#include <thread>
#include <format>
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
					INetEventManager* p_net_event_mgr,
				SendManager* p_send_mgr)
		{
			pref_lazyed_dis_conn_mgr_ = p_lazyed_dis_conn_mgr;
			pref_session_mgr_ = p_session_mgr;
			pref_net_event_mgr_ = p_net_event_mgr;
			pref_send_mgr_ = p_send_mgr;
		}


		bool StartIoThreads(const std::uint32_t rio_thread_count,
							const std::uint32_t max_send_count_per_1sec,
							const std::uint32_t max_connection_count,
							const std::uint32_t io_work_thread_count, 
							std::vector<RIO_CQ>& rio_completion_queues)
		{
			auto max_cq_count_per_thread = MaxCQCountPerThread(max_connection_count,
												io_work_thread_count,
												max_send_count_per_1sec);

			for (std::uint32_t i = 0; i < rio_thread_count; ++i) {				
				auto rioQueue = RIO.RIOCreateCompletionQueue(max_cq_count_per_thread, 0);
				if (rioQueue == RIO_INVALID_CQ) {
					return false;
				} else {
					rio_completion_queues.push_back(rioQueue);
				}

				auto threadIndex = i;
				io_work_threads_.emplace_back([threadIndex, &rioQueue, this]() {
						IoWorkerThread(threadIndex, rioQueue);
					});

				// mIOWorkThreads 내부에서 초기화 작업이 있어서 잠시 대기한다
				std::this_thread::sleep_for(std::chrono::milliseconds(32));
			}

			return true;
		}

		void Stop()
		{
			is_run_ = true;

			for (auto& thread : io_work_threads_) {
				if (thread.joinable()) {
					thread.join();
				}
			}
		}

		void IoWorkerThread(const int thread_index, RIO_CQ rio_completion_queues)
		{
			auto coreCount = Helper::GetNumberOfProcessors();
			auto results = new RIORESULT[coreCount];

			IoWorkerThread_startLog(thread_index, coreCount);

			pref_lazyed_dis_conn_mgr_->AddSessionQueue();

			while (is_run_) {
				memset(results, 0, sizeof(results));
				ULONG numResults = RIO.RIODequeueCompletion(rio_completion_queues, results, coreCount);

				if (RIO_CORRUPT_CQ == numResults) {
					LogFuncPtr((int)LogLevel::kFATAL, "RIONetManager::IoWorkerThread() - RIO CORRUPT CQ");
					continue;
				}

				for (ULONG i = 0; i < numResults; ++i) {
					auto context = reinterpret_cast<RioIoContext*>(results[i].RequestContext);
					auto pSession = pref_session_mgr_->GetSessionObj(context->session_index_);
					auto transferred = results[i].BytesTransferred;

					ProcessRIOCQ(transferred, context, pSession);
				}

				pref_lazyed_dis_conn_mgr_->ProcessLazyedDisConnectSession(thread_index, pref_session_mgr_);

				if (0 == numResults) { 
					Sleep(1); ///< for low cpu-usage
				}
			}

			delete results;

			IoWorkerThread_endLog(thread_index);
			return;
		}

		void ProcessRIOCQ(const ULONG transferred, RioIoContext* p_context, TcpSession* p_session)
		{
			auto cq_session_uid = p_context->session_unique_Id_;

			if (transferred == 0) {
				DisConnectFunc(p_session, cq_session_uid, DisconnectReason::kZERO_COMPLETION);

			} else if (IOType::kRECV == p_context->io_type_) {
				RecvCompletion(p_session, transferred);

				PostRecv(cq_session_uid, p_session);

			} else if (IOType::kSEND == p_context->io_type_) {
#ifndef __USE_SEND_THREAD__
				if (p_context->Length == transferred) {
					p_session->SendCompletion(p_context);
				} else {
					DisConnectFunc(p_session, cq_session_uid, DisconnectReason::kPARITAL_SEND_COMPLETION);
				}
#else
				pref_send_mgr_->SendCompletedSession(p_session->GetIndex(), transferred);
#endif			
			} else {
				LogFuncPtr((int)LogLevel::kERROR, 
					std::format("ServerNetIOWorker::ProcessRIOCQ() - Unknown I/O Type: {0}", (int)p_context->io_type_).c_str());
			}
		}

		// 받기 버퍼의 읽은 위치도 여기서 이동 시키므로 링버퍼의 데이터가 겹치는 것을 막고 싶으면 애플리케이션에서 OnReceive의 데이터를 다 사용할 때까지 잘 보관해야 한다.
		void RecvCompletion(TcpSession* p_session, const std::uint32_t transferred)
		{
			p_session->RecvCompletion(transferred);

			auto [readSize, pReadData] = p_session->ReadRecvBuffer();
			auto usedSize = pref_net_event_mgr_->OnReceive(p_session->GetIndex(),
															p_session->GetUID(),
															readSize,
															pReadData);
			p_session->IncremantReadPosRecvBuffer(usedSize);
		}

		bool PostRecv(uint64_t cq_session_uid, TcpSession* p_session)
		{
			if (auto result = p_session->PostRecv(); result != NetErrorCode::kNONE) {
				if (result == NetErrorCode::kRECV_RIO_FAIL) {
					DisConnectFunc(p_session, cq_session_uid, DisconnectReason::kIO_REQUEST_RECV_ERROR);
				}
				return false;
			}

			return true;
		}

		uint32_t MaxCQCountPerThread(const std::uint32_t max_connection_count,
									const std::uint32_t io_work_thread_count,
									const std::uint32_t max_send_count_per_1sec)
		{
			auto session_count_per_rio_thread = max_connection_count / io_work_thread_count;
			auto max_cq_count_per_rio_thread = (1 + max_send_count_per_1sec) * session_count_per_rio_thread;

			return max_cq_count_per_rio_thread;
		}



	private:
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


	
		bool is_run_ = false;

		ServerNetLazyedDisConnManager* pref_lazyed_dis_conn_mgr_ = nullptr;
		SessionManager* pref_session_mgr_ = nullptr;
		INetEventManager* pref_net_event_mgr_ = nullptr;
		SendManager* pref_send_mgr_ = nullptr;

		std::vector<std::thread> io_work_threads_;
	};
}