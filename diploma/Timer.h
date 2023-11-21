#pragma once

#include <chrono>

#include "framework.h"

class ITimer {
public:
	virtual void start() = 0;
	virtual void stop() = 0;

	virtual double getTime() = 0;
	virtual double getAcc() = 0;
};

class CPUTimer : ITimer {
	std::chrono::steady_clock::time_point m_timeStart{};
	std::chrono::steady_clock::time_point m_timeStop{};

	double m_acc{};

public:
	void start() override;
	void stop() override;

	double getTime() override;
	double getAcc() override;

	double curr();
};

class GPUTimer : ITimer {
	ID3D11Device* m_pDevice{};
	ID3D11DeviceContext* m_pDeviceContext{};

	ID3D11Query* m_pTimeStart{};
	ID3D11Query* m_pTimeStop{};
	ID3D11Query* m_pDisjoint{};

	double m_acc{};

public:
	GPUTimer() = delete;
	GPUTimer(ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext);
	~GPUTimer();

	void start() override;
	void stop() override;

	double getTime() override;
	double getAcc() override;
};
