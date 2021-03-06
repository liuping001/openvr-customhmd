#include "ServerDriver.h"
#include "TrackedHMD.h"
#include "TrackedController.h"
#include <process.h>

EVRInitError CServerDriver::Init(IDriverLog * pDriverLog, IServerDriverHost * pDriverHost, const char * pchUserDriverConfigDir, const char * pchDriverInstallDir)
{	
	timeBeginPeriod(1);
	m_CurrTick = m_LastTick = GetTickCount();

	m_pLog = new CDriverLog(pDriverLog);
	m_LastTypeSequence = 0;
	_LOG(__FUNCTION__" start");

	m_pDriverHost = pDriverHost;
	m_UserDriverConfigDir = pchUserDriverConfigDir;
	m_DriverInstallDir = pchDriverInstallDir;

	m_HMDAdded = m_RightCtlAdded = m_LeftCtlAdded = false;

	m_pSettings = pDriverHost ? pDriverHost->GetSettings(IVRSettings_Version) : nullptr;
	m_Align = { 0 };
	m_Relative = { 0 };
	if (m_pSettings)
	{
		m_Align.v[0] = m_pSettings->GetFloat("driver_customhmd", "eoX");
		m_Align.v[1] = m_pSettings->GetFloat("driver_customhmd", "eoY");
		m_Align.v[2] = m_pSettings->GetFloat("driver_customhmd", "eoZ");		
	}
	
	m_TrackedDevices.push_back(new CTrackedHMD("HMD", this)); //only add hmd

	//m_TrackedDevices.push_back(new CTrackedController(TrackedControllerRole_RightHand, "RIGHT CONTROLLER", this));
	//m_TrackedDevices.push_back(new CTrackedController(TrackedControllerRole_LeftHand, "LEFT CONTROLLER", this));

	m_hThread = nullptr;
	m_IsRunning = false;

	m_hThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, ProcessThread, this, CREATE_SUSPENDED, nullptr));
	if (m_hThread)
	{
		m_IsRunning = true;
		ResumeThread(m_hThread);
	}

	_LOG(__FUNCTION__" end");
	return VRInitError_None;
}

void CServerDriver::Cleanup()
{
	m_IsRunning = false;
	if (m_hThread)
	{
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread);
		m_hThread = nullptr;
	}

	_LOG(__FUNCTION__);
	for (auto iter = m_TrackedDevices.begin(); iter != m_TrackedDevices.end(); iter++)
		delete (*iter);
	m_TrackedDevices.clear();

	m_pDriverHost = nullptr;
	m_UserDriverConfigDir.clear();
	m_DriverInstallDir.clear();
	delete m_pLog;
	m_pLog = nullptr;
	timeEndPeriod(1);
}

unsigned int WINAPI CServerDriver::ProcessThread(void *p)
{
	auto serverDriver = static_cast<CServerDriver *>(p);
	if (serverDriver)					
		serverDriver->Run();	
	_endthreadex(0);
	return 0;
}

void CServerDriver::OpenUSB(hid_device **ppHandle)
{
	CloseUSB(ppHandle);
	hid_device *handle = hid_open(0x1974, 0x0001, nullptr);
	if (!handle)
		return;
	*ppHandle = handle;
	
	#define MAX_STR 255
	wchar_t wstr[MAX_STR];
	int res = hid_get_manufacturer_string(handle, wstr, MAX_STR);
	res = hid_get_product_string(handle, wstr, MAX_STR);
	res = hid_get_serial_number_string(handle, wstr, MAX_STR);
	hid_set_nonblocking(handle, 1);
}

void CServerDriver::CloseUSB(hid_device **ppHandle)
{
	if (!ppHandle || !*ppHandle)
		return;
	hid_close(*ppHandle);
	*ppHandle = nullptr;
}

void CServerDriver::SendUSBCommand(USBPacket *command)
{
	m_CommandQueue.push_back(command);
}

void CServerDriver::ScanSyncReceived(uint64_t syncTime)
{
	_LOG(__FUNCTION__" sync @ %I64u" , syncTime);
}

void CServerDriver::RemoveTrackedDevice(CTrackedDevice *pDevice)
{
	for (auto iter = m_TrackedDevices.begin(); iter != m_TrackedDevices.end(); iter++)
	{
		if (*iter == pDevice)
		{
			delete (*iter);
			return;
		}
	}
}

void CServerDriver::Run()
{	
	int pos = 0;
	int res;

	hid_init();
	
	long count = 0;

	unsigned char buf[33] = { 0 };

	struct USBPacket *pUSBPacket = (struct USBPacket *)buf;

	hid_device *pHandle = nullptr;
	DWORD lastTick = GetTickCount();
	DWORD lastCalibSend = 0;
	uint8_t calibDeviceIndex = 0;

	USBCalibrationData calibrationData[3] = { 0 };
	for (auto i = 0; i < 3; i++)
	{	
		//char sectionName[32];
		//sprintf_s(sectionName, "offsetAccX_%d", i); calibrationData[i].OffsetAccel[0] = (int16_t) m_pSettings->GetInt32("driver_customhmd", sectionName, 0);
		//sprintf_s(sectionName, "offsetAccY_%d", i); calibrationData[i].OffsetAccel[1] = (int16_t)m_pSettings->GetInt32("driver_customhmd", sectionName, 0);
		//sprintf_s(sectionName, "offsetAccZ_%d", i); calibrationData[i].OffsetAccel[2] = (int16_t)m_pSettings->GetInt32("driver_customhmd", sectionName, 0);
		//sprintf_s(sectionName, "offsetGyroX_%d", i); calibrationData[i].OffsetGyro[0] = (int16_t)m_pSettings->GetInt32("driver_customhmd", sectionName, 0);
		//sprintf_s(sectionName, "offsetGyroY_%d", i); calibrationData[i].OffsetGyro[1] = (int16_t)m_pSettings->GetInt32("driver_customhmd", sectionName, 0);
		//sprintf_s(sectionName, "offsetGyroZ_%d", i); calibrationData[i].OffsetGyro[2] = (int16_t)m_pSettings->GetInt32("driver_customhmd", sectionName, 0);
		//sprintf_s(sectionName, "offsetMagX_%d", i); calibrationData[i].OffsetMag[0] = (int16_t)m_pSettings->GetInt32("driver_customhmd", sectionName, 0);
		//sprintf_s(sectionName, "offsetMagY_%d", i); calibrationData[i].OffsetMag[1] = (int16_t)m_pSettings->GetInt32("driver_customhmd", sectionName, 0);
		//sprintf_s(sectionName, "offsetMagZ_%d", i); calibrationData[i].OffsetMag[2] = (int16_t)m_pSettings->GetInt32("driver_customhmd", sectionName, 0);
	}



	while (m_IsRunning)
	{
		//pOverlayManager->HandleEvents();
		if (!pHandle)
		{
			OpenUSB(&pHandle);
			lastCalibSend = lastTick = GetTickCount();
			if (!pHandle)
			{
				//MessageBox(nullptr, L"No USB", L"Info", 0);
				Sleep(1000);
				continue;
			}
		}
		else
		{ 
			DWORD now = GetTickCount();
			//if (now - lastCalibSend >= 3000)
			//{
			//	//send calibration data for each device every 9 seconds 
			// calibrationData[i].SensorMask = SENSOR_ACCEL | SENSOR_GYRO | SENSOR_MAG;
			//	USBPacket *pPacket = new USBPacket();
			//	ZeroMemory(pPacket, sizeof(USBPacket));
			//	pPacket->Header.Type = calibDeviceIndex | COMMAND_DATA;
			//	pPacket->Header.Crc8 = 0;
			//	pPacket->Header.Sequence = (uint16_t)GetTickCount(); //put timestamp as sequence to hopefully prevent duplicates on target
			//	pPacket->Command.Command = CMD_CALIBRATE;
			//	pPacket->Command.Data.Calibration = calibrationData[calibDeviceIndex];
			//	SetPacketCrc(pPacket);
			//	SendUSBCommand(pPacket);

			//	calibDeviceIndex++;
			//	if (calibDeviceIndex > 2) //hmd=0, left = 1, right = 2
			//		calibDeviceIndex = 0;
			//	lastCalibSend = now;
			//}

			if (m_CommandQueue.size() > 0)
			{	
				buf[0] = 0x00;
				USBPacket *pPacket = m_CommandQueue.front();
				*((USBPacket*)(buf + 1)) = *pPacket;
				delete pPacket;
				m_CommandQueue.pop_front();				
				res = hid_write(pHandle, buf, sizeof(buf));
			}


			res = hid_read_timeout(pHandle, buf, sizeof(buf), 10); 
			if (res > 0)
			{
				lastTick = now;

				//auto crcTemp = pUSBPacket->Header.Crc8;
				//pUSBPacket->Header.Crc8 = 0;
				//uint8_t* data = (uint8_t*)pUSBPacket;
				//uint8_t crc = 0;
				//for (int i = 0; i<sizeof(USBPacket); i++)
				//	crc ^= data[i];
				//if (crc == crcTemp)
				if (CheckPacketCrc(pUSBPacket))
				{
					switch (pUSBPacket->Header.Type & 0x0F)
					{
					case BASESTATION_SOURCE:
						if ((pUSBPacket->Header.Type & 0xF0) == COMMAND_DATA)
							ScanSyncReceived(pUSBPacket->Command.Data.Sync.SyncTime);
						break;							
					case LEFTCTL_SOURCE:
						if (!m_LeftCtlAdded)
						{
							m_LeftCtlAdded = true;
							m_TrackedDevices.push_back(new CTrackedController(TrackedControllerRole_LeftHand, "LEFT CONTROLLER", this));
						}
						break;
					case RIGHTCTL_SOURCE:
						if (!m_RightCtlAdded)
						{
							m_RightCtlAdded = true;
							m_TrackedDevices.push_back(new CTrackedController(TrackedControllerRole_RightHand, "RIGHT CONTROLLER", this));
						}
						break;
					}
					for (auto iter = m_TrackedDevices.begin(); iter != m_TrackedDevices.end(); iter++)
						(*iter)->PoseUpdate(pUSBPacket, &m_Align, &m_Relative);
				}
			}
			else if (res < 0)
			{
				//usb fucked up?				
				//MessageBox(nullptr, L"Disco", L"Info", 0);
				CloseUSB(&pHandle);
				Sleep(1000);
			}
			else
			{
				if (now - lastTick >= 5000) //reset usb if no data for 5 secs
				{
					CloseUSB(&pHandle);
					Sleep(1000);
				}
			}
			Sleep(1);
		}

		//m_pDriverHost->TrackedDevicePoseUpdated(m_unObjectId, m_HMDData.Pose);
	}

	CloseUSB(&pHandle);
	hid_exit();	

}

uint32_t CServerDriver::GetTrackedDeviceCount()
{
	_LOG(__FUNCTION__" returns %d", m_TrackedDevices.size());
	return (uint32_t)m_TrackedDevices.size();
}

ITrackedDeviceServerDriver * CServerDriver::GetTrackedDeviceDriver(uint32_t unWhich)
{
	_LOG(__FUNCTION__" idx: %d", unWhich);
	//if (0 != _stricmp(pchInterfaceVersion, ITrackedDeviceServerDriver_Version))
	//	return nullptr;	
	if (unWhich >= m_TrackedDevices.size())
		return nullptr;
	return m_TrackedDevices.at(unWhich);
}

ITrackedDeviceServerDriver * CServerDriver::FindTrackedDeviceDriver(const char * pchId)
{
	_LOG(__FUNCTION__" id: %s", pchId);
	for (auto iter = m_TrackedDevices.begin(); iter != m_TrackedDevices.end(); iter++)
	{
		if (0 == std::strcmp(pchId, (*iter)->Prop_SerialNumber.c_str()))
		{ 
			return *iter;
		}
	}
	return nullptr;
}

void CServerDriver::RunFrame()
{
	DWORD currTick = GetTickCount();
	for (auto iter = m_TrackedDevices.begin(); iter != m_TrackedDevices.end(); iter++)
		(*iter)->RunFrame(currTick);
}

bool CServerDriver::ShouldBlockStandbyMode()
{
	_LOG(__FUNCTION__);
	return false;
}

void CServerDriver::EnterStandby()
{
	_LOG(__FUNCTION__);
}

void CServerDriver::LeaveStandby()
{
	_LOG(__FUNCTION__);
}

const char * const * CServerDriver::GetInterfaceVersions()
{
	return k_InterfaceVersions;
}

void CServerDriver::AlignHMD(HmdVector3d_t *pAlign)
{
	m_Align = *pAlign;
	if (m_pSettings)
	{
		m_pSettings->SetFloat("driver_customhmd", "eoX", (float)m_Align.v[0]);
		m_pSettings->SetFloat("driver_customhmd", "eoY", (float)m_Align.v[1]);
		m_pSettings->SetFloat("driver_customhmd", "eoZ", (float)m_Align.v[2]);
		m_pSettings->Sync(true);
	}
}