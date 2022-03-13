#pragma once
#include <deque>
#include <forward_list>
#include <map>
#include <mutex>
#include "Order.h"
#include "Util.h"


class OrderBook
{
public:
	float GetBestBidPrice() const
	{
		return m_bids.front().front().price;
	};

	float GetBestAskPrice() const
	{
		return m_asks.front().front().price;
	};

	float GetBestBidAmount() const
	{
		return m_bids.front().front().unfilledAmount;
	};

	float GetBestAskAmount() const
	{
		return m_asks.front().front().unfilledAmount;
	};

	bool OrderExists(const OrderId& orderId) const
	{
		return m_ordersMap.count(orderId) > 0;
	}

	void InsertOrder(const Order& i_order)
	{
		auto order = i_order;

		if (order.isBuy)
		{
			while (order.unfilledAmount > 0 && order.price >= GetBestAskPrice())
			{
				float tradeAmount = std::min(order.unfilledAmount, m_asks.front().front().unfilledAmount);
				order.unfilledAmount -= tradeAmount;
				m_asks.front().front().unfilledAmount -= tradeAmount;

				if (FloatEqual(m_asks.front().front().unfilledAmount, 0))
				{
					m_asks.front().pop_front();
					if (m_asks.front().empty())
					{
						m_asks.pop_front();
					}
				}
			}

			if (order.unfilledAmount > 0)
			{
				if (order.price > GetBestBidPrice())
				{
					auto tmp = std::deque<Order>{ order };
					m_bids.push_front(tmp);
				}
				else
				{
					auto it = m_bids.begin();
					auto prevIt = it;
					while (order.price < it->front().price)
					{
						prevIt = it;
						it++;
					}
					if (prevIt == it)
					{
						m_bids.front().push_back(order);
					}
					else if (FloatEqual(prevIt->front().price, order.price))
					{
						prevIt->push_back(order);
					}
					else
					{
						auto tmp = std::deque<Order>{ order };
						m_bids.insert_after(prevIt, tmp);
					}
				}

				m_ordersMap[order.id] = order;
			}
		}
		else
		{
			while (order.unfilledAmount > 0 && order.price <= GetBestBidPrice())
			{
				float tradeAmount = std::min(order.unfilledAmount, m_bids.front().front().unfilledAmount);
				order.unfilledAmount -= tradeAmount;
				m_bids.front().front().unfilledAmount -= tradeAmount;

				if (FloatEqual(m_bids.front().front().unfilledAmount, 0))
				{
					m_bids.front().pop_front();
					if (m_bids.front().empty())
					{
						m_bids.pop_front();
					}
				}
			}

			if (order.unfilledAmount > 0)
			{
				if (order.price < GetBestAskPrice())
				{
					auto tmp = std::deque<Order>{ order };
					m_asks.push_front(tmp);
				}
				else
				{
					auto it = m_asks.begin();
					auto prevIt = it;
					while (order.price > it->front().price)
					{
						prevIt = it;
						it++;
					}
					if (prevIt == it)
					{
						m_asks.front().push_back(order);
					}
					else if (FloatEqual(prevIt->front().price, order.price))
					{
						prevIt->push_back(order);
					}
					else
					{
						auto tmp = std::deque<Order>{ order };
						m_asks.insert_after(prevIt, tmp);
					}
				}

				m_ordersMap[order.id] = order;
			}
		}
	}

	bool AmendOrder(const OrderId& orderId, const Order& i_order)
	{
		auto res = CancelOrder(orderId);
		if (!res)
			return false;

		auto order = i_order;
		order.id = orderId;
		InsertOrder(order);
		return true;
	}
		
	bool CancelOrder(const OrderId& orderId)
	{
		if (!OrderExists(orderId))
			return false;

		const auto order = m_ordersMap[orderId];
		m_ordersMap.erase(orderId);

		if (order.isBuy)
		{
			if (order.price > GetBestBidPrice())
			{
				return false;
			}
			else
			{
				auto it = m_bids.begin();
				auto prevIt = it;
				while (order.price != it->front().price)
				{
					prevIt = it;
					it++;
					if (it == m_bids.end() || order.price > it->front().price)
						return false;
				}
				auto res = CancelOrderFromQueue(order.id, *it);
				if (it->empty())
					if (it == prevIt)
						m_bids.pop_front();
					else
						m_bids.erase_after(prevIt);
				return res;
			}
		}
		else
		{
			if (order.price < GetBestAskPrice())
			{
				return false;
			}
			else
			{
				auto it = m_asks.begin();
				auto prevIt = it;
				while (order.price != it->front().price)
				{
					prevIt = it;
					it++;
					if (it == m_asks.end() || order.price < it->front().price)
						return false;
				}
				auto res = CancelOrderFromQueue(order.id, *it);
				if (it->empty())
					if (it == prevIt)
						m_asks.pop_front();
					else
						m_asks.erase_after(prevIt);
				return res;
			}
		}

		return true;
	}

	bool operator==(const OrderBook& other) const
	{
		auto it1 = m_asks.begin();
		auto it2 = other.m_asks.begin();

		while (it1 != m_asks.end() && it2 != other.m_asks.end())
		{
			auto q1 = *it1;
			auto q2 = *it2;

			if (q1.size() != q2.size())
				return false;
			while (q1.size() > 0)
			{
				if (q1.front() != q2.front())
					return false;
				q1.pop_front();
				q2.pop_front();
			}

			it1++;
			it2++;
		}

		if (it1 != m_asks.end() || it2 != other.m_asks.end())
			return false;

		return true;
	}

	bool operator!=(const OrderBook& other) const
	{
		return !(*this == other);
	}

private:
	bool CancelOrderFromQueue(const OrderId& orderId, std::deque<Order>& queue)
	{
		auto it = queue.begin();
		while (it != queue.end())
		{
			if (it->id == orderId)
			{
				queue.erase(it);
				return true;
			}
		}
		return false;
	}

public:
	std::forward_list<std::deque<Order>> m_bids;
	std::forward_list<std::deque<Order>> m_asks;

private:
	std::map<OrderId, Order> m_ordersMap; // auxiliary map for faster lookup of orders in the orderbook	
};