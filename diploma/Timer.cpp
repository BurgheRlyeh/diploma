#include "Timer.h"

using namespace std;
using namespace std::chrono;

void CPUTimer::start() {
	m_timeStart = high_resolution_clock::now();
}

void CPUTimer::stop() {
	m_timeStop = high_resolution_clock::now();
}

double CPUTimer::getTime() {
	return duration<double, milli>(m_timeStop - m_timeStart).count();
}

double CPUTimer::getCurrent() {
	steady_clock::time_point timeCurr{ high_resolution_clock::now() };
	return duration<double, milli>(timeCurr - m_timeStart).count();
}

GPUTimer::GPUTimer(ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext) :
	m_pDevice(pDevice),
	m_pDeviceContext(pDeviceContext)
{
	D3D11_QUERY_DESC desc{ D3D11_QUERY_TIMESTAMP };
	THROW_IF_FAILED(m_pDevice->CreateQuery(&desc, &m_pTimeStart));
	THROW_IF_FAILED(m_pDevice->CreateQuery(&desc, &m_pTimeStop));

	desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	THROW_IF_FAILED(m_pDevice->CreateQuery(&desc, &m_pDisjoint));
}

GPUTimer::~GPUTimer() {
	SAFE_RELEASE(m_pTimeStart);
	SAFE_RELEASE(m_pTimeStop);
	SAFE_RELEASE(m_pDisjoint);
}

void GPUTimer::start() {
	m_pDeviceContext->Begin(m_pDisjoint);
	m_pDeviceContext->End(m_pTimeStart);
}

void GPUTimer::stop() {
	m_pDeviceContext->End(m_pTimeStop);
	m_pDeviceContext->End(m_pDisjoint);
}

double GPUTimer::getTime() {
	UINT64 t0, t;
	while (m_pDeviceContext->GetData(m_pTimeStart, &t0, sizeof(t0), 0) != S_OK);
	while (m_pDeviceContext->GetData(m_pTimeStop, &t, sizeof(t), 0) != S_OK);

	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
	m_pDeviceContext->GetData(m_pDisjoint, &disjoint, sizeof(disjoint), 0);

	return disjoint.Disjoint ? 0.0 : 1e3 * (t - t0) / disjoint.Frequency;
}
