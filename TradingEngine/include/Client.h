#pragma once
#include "MatchingEngine.h"
#include "OrderUpdate.h"
#include "IObserver.h"

static ClientId clientIdCount = 100;

class Client : public IObserver
{
public:
	Client(const std::shared_ptr<MatchingEngine> engine);

	void InsertOrder(const Order& order) const;

	void Notify(const Ack& ack) override;

	void Notify(const OrderUpdate& orderUpdate) override;

	void Notify(const Trade& trade) override;

public:
	ClientId id;

private:	
	std::shared_ptr<MatchingEngine> m_engine;
};

