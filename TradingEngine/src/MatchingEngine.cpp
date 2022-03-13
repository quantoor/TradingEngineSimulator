#include "MatchingEngine.h"
#include "MarketDataStreamer.h"
#include "OrderUpdate.h"
#include <thread>
#include <sstream>


static OrderId orderIdCount = 1000;


MatchingEngine::MatchingEngine()
{
	auto streamer = MarketDataStreamer();
	std::vector<Order> orders;
	streamer.GetData(orders);

	auto callback = std::bind(&MatchingEngine::NotifyOrderUpdate, this, std::placeholders::_1);
	m_orderBook.SetOrderUpdateCallback(callback);

	ReceiveStreamMarketData(orders);

	m_isProcessing = true;
	m_transactionsQueue.reset(new std::deque<Transaction>());
	m_processingThread.reset(new std::thread(&MatchingEngine::ProcessingQueue, this));
}

// PUBLIC METHODS
void MatchingEngine::Stop()
{
	m_isProcessing = false;
	m_processingThread->join();
}

void MatchingEngine::InsertOrder(const ClientId& clientId, const Order& i_order)
{
	// assuming the client has enough balance to place the order
	auto order = i_order;
	order.id = orderIdCount++;
	order.clientId = clientId;
	std::ostringstream ss;
	ss << order;
	NotifyAck(clientId, Ack{ true, "Insert " + ss.str() });

	AddTransactionToProcessingQueue(Transaction{ order.id, order, TransactionType::Insert });
}

void MatchingEngine::AmendOrder(const ClientId& clientId, const OrderId& orderId, const Order& order)
{
	// validation
	if (m_orderBook.OrderExists(orderId))
	{
		std::ostringstream ss;
		ss << order;
		NotifyAck(clientId, Ack{ true, "Amend " + ss.str() });
		AddTransactionToProcessingQueue({ orderId, order, TransactionType::Amend });
	}
	else
		NotifyAck(clientId, Ack{ false, "Order ID " + std::to_string(orderId) + " not found."});
}

void MatchingEngine::CancelOrder(const ClientId& clientId, const OrderId& orderId)
{
	// validation
	if (!m_orderBook.OrderExists(orderId))
	{
		NotifyAck(clientId, Ack{ true, "Cancel order with ID " + std::to_string(orderId) });
		AddTransactionToProcessingQueue({ orderId, Order(), TransactionType::Cancel });
	}
	else
		NotifyAck(clientId, Ack{ false, "Order ID " + std::to_string(orderId) + " not found." });
}

void MatchingEngine::ReceiveStreamMarketData(const std::vector<Order>& orders)
{
	for (auto i = 0; i < orders.size(); i++)
	{
		m_orderBook.InsertOrder(orders[i]);
	}
}

// PRIVATE METHODS
void MatchingEngine::AddTransactionToProcessingQueue(const Transaction& transaction)
{
	std::unique_lock<std::mutex> locker(m_mu);
	m_cv.wait(locker, []() { return true; });
	m_transactionsQueue->push_back(transaction);
	locker.unlock();
	m_cv.notify_one();
}

void MatchingEngine::ProcessingQueue()
{
	while (m_isProcessing)
	{
		std::unique_lock<std::mutex> lock(m_mu);
		if (m_transactionsQueue->empty())
			m_cv.wait_for(lock, std::chrono::milliseconds(100)); // to avoid cpu overclocking if the queue is empty
		
		if (!m_transactionsQueue->empty())
		{
			const auto& transaction = m_transactionsQueue->front();
			m_transactionsQueue->pop_front();

			switch (transaction.type)
			{
			case TransactionType::Insert:
				m_orderBook.InsertOrder(transaction.order);
				break;
			case TransactionType::Amend:
				m_orderBook.AmendOrder(transaction.orderId, transaction.order);
				break;
			case TransactionType::Cancel:
				m_orderBook.CancelOrder(transaction.orderId);
				break;
			}			
		}
	}
}

void MatchingEngine::NotifyAck(const ClientId& clientId, const Ack& ack)
{
	if (m_clientMap.count(clientId) == 0)
		return;
	m_clientMap[clientId]->Notify(ack);
}

void MatchingEngine::NotifyOrderUpdate(OrderUpdate* orderUpdate)
{
	if (m_clientMap.count(orderUpdate->clientId) == 0)
		return;
	m_clientMap[orderUpdate->clientId]->Notify(*orderUpdate);
}
