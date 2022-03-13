#pragma once
#include "Order.h"
#include "Ack.h"
#include "OrderBook.h"
#include "IObserver.h"
#include <cstdint>
#include <memory>
#include <map>
#include <deque>
#include <mutex>
#include <functional>
#include <vector>


enum class TransactionType { Insert, Amend, Cancel };

struct Transaction
{
	OrderId orderId;
	Order order;
	TransactionType type;
};


class MatchingEngine
{
public:
	MatchingEngine();

	void Stop();

	void InsertOrder(const ClientId& clientId, const Order& order);

	void AmendOrder(const ClientId& clientId, const OrderId& orderId, const Order& order);

	void CancelOrder(const ClientId& clientId, const OrderId& orderId);

	void ReceiveStreamMarketData(const std::vector<Order> &orders);

	inline void SubscribeClient(ClientId clientId, std::shared_ptr<IObserver> observer) { m_clientMap.emplace(clientId, observer); }

	inline void SetTest(bool isTest) { m_orderBook.SetTest(isTest); };

private:
	void AddTransactionToProcessingQueue(const Transaction& transaction);

	void ProcessingQueue();

	void NotifyAck(const ClientId& clientId, const Ack& ack);

	void NotifyOrderUpdate(OrderUpdate* orderUpdate);	

private:
	OrderBook m_orderBook{};

	std::shared_ptr<std::deque<Transaction>> m_transactionsQueue = 0;
	mutable std::mutex m_mu;
	std::condition_variable m_cv;
	std::shared_ptr<std::thread> m_processingThread = 0;
	bool m_isProcessing = false;

	std::map<ClientId, std::shared_ptr<IObserver>> m_clientMap{};
};
