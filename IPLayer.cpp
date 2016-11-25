#include "StdAfx.h"
#include "IPLayer.h"
#include "RouterDlg.h"

CIPLayer::CIPLayer(char* pName) : CBaseLayer(pName) 
{ 
	ResetHeader();
}

CIPLayer::~CIPLayer() 
{ 
}

void CIPLayer::SetDstIP(unsigned char* ip, int dev_num)
{
	if(dev_num == 1)
		memcpy(dev_1_dst_ip_addr, ip , 4 );
	else
		memcpy(dev_2_dst_ip_addr, ip , 4 );

	memcpy(&Ip_header.Ip_dstAddress,ip,4);
}

void CIPLayer::SetSrcIP(unsigned char* ip ,int dev_num)
{
	if(dev_num == 1)
		memcpy(dev_1_ip_addr , ip , 4 );
	else
		memcpy(dev_2_ip_addr , ip , 4 );

	memcpy(&Ip_header.Ip_srcAddress,ip,4);
}

unsigned char* CIPLayer::GetDstIP(int dev_num)
{
	if(dev_num == 1)
		return dev_1_dst_ip_addr;
	return dev_2_dst_ip_addr;
}

unsigned char* CIPLayer::GetSrcIP(int dev_num)
{
	if(dev_num == 1)
		return dev_1_ip_addr;
	return dev_2_ip_addr;
}

unsigned char* CIPLayer::GetSrcFromPacket()
{
	return receivedPacket->Ip_srcAddressByte;
}

unsigned char* CIPLayer::GetDstFromPacket()
{
	return receivedPacket->Ip_dstAddressByte;
}

void CIPLayer::SetSrcPacketIP(unsigned char* ip)
{
	memcpy(receivedPacket->Ip_srcAddressByte, ip, 4);
}

void CIPLayer::SetDstPacketIP(unsigned char* ip)
{
	memcpy(receivedPacket->Ip_dstAddressByte, ip, 4);
}

unsigned char CIPLayer::GetProtocol(int dev_num) 
{
	if(dev_num == 1)
		return dev_1_protocol;
	return dev_2_protocol;
}

void CIPLayer::SetProtocol(unsigned char protocol, int dev_num)
{
	if(dev_num == 1)
		dev_1_protocol = protocol;
	else
		dev_2_protocol = protocol;

	Ip_header.Ip_protocol = protocol;
}

unsigned short CIPLayer::SetChecksum(unsigned char p_header[20])
{
	unsigned short word;
	unsigned int sum = 0;
	int i;

	for(i = 0; i < IP_HEADER_SIZE; i = i + 2) {
		if(i == 10) 
			continue;
		word = ((p_header[i] << 8) & 0xFF00) + (p_header[i+1] & 0xFF);
		sum = sum + (unsigned int) word;
	}

	while(sum >> 16)
		sum = (sum&0xFFFF) + (sum >> 16);

	sum = ~sum;
	return (unsigned short) sum;
}

BOOL CIPLayer::IsValidChecksum(unsigned char* p_header, unsigned short checksum)
{
	unsigned short word, ret;
	unsigned int sum = 0;
	int i;

	for(i = 0; i < IP_HEADER_SIZE; i = i + 2) {
		if(i == 10) continue;
		word = ((p_header[i] << 8) & 0xFF00) + (p_header[i+1] & 0xFF);
		sum = sum + (unsigned int) word;
	}

	while(sum >> 16)
		sum = (sum&0xFFFF) + (sum >> 16);

	ret = sum & checksum;
	return	ret == 0;
}

BOOL CIPLayer::Send(unsigned char* ppayload, int nlength, int dev_num)
{
	CRouterDlg* routerDlg = ((CRouterDlg *) (GetUpperLayer(0)->GetUpperLayer(0)));
	receivedPacket->Ip_checksum = htons(SetChecksum((unsigned char*) receivedPacket));
	BOOL bSuccess = GetUnderLayer()->Send((unsigned char*) receivedPacket, (int) ntohs(receivedPacket->Ip_len), dev_num);
	return bSuccess;
}

BOOL CIPLayer::Receive(unsigned char* ppayload, int dev_num)
{
	unsigned char broadcast[4] = { 0xff, 0xff, 0xff , 0xff };
	unsigned char multicast[4] = { 0xe0, 0, 0, 0x9 };
	
	CRouterDlg* routerDlg = ((CRouterDlg *) (GetUpperLayer(0)->GetUpperLayer(0)));
	receivedPacket = (PIpHeader) ppayload;

	if(!memcmp(receivedPacket->Ip_srcAddressByte, routerDlg->GetSrcIP(dev_num), 4)) //자신이 보낸 패킷은 버린다
		return FALSE;

	if(!IsValidChecksum((unsigned char*) receivedPacket, ntohs(receivedPacket->Ip_checksum)))
		return FALSE;

	if(receivedPacket->Ip_timeToLive == 0 ) //ttl이 0일 경우 버림
      return FALSE;

	if (receivedPacket->Ip_protocol == 0x01) { // icmp protocol (01) 확인
		return GetUpperLayer(0)->Receive((unsigned char *)receivedPacket->Ip_data, dev_num);
	}
	
	if (receivedPacket->Ip_protocol == 0x06) { // tcp protocol (06) 확인
//		((CTCPLayer*)GetUpperLayer(1))->SetReceivePseudoHeader(receivedPacket->Ip_srcAddressByte, receivedPacket->Ip_dstAddressByte, (unsigned short) htons(ntohs(receivedPacket->Ip_len) - IP_HEADER_SIZE));
		return GetUpperLayer(1)->Receive((unsigned char *)receivedPacket->Ip_data, dev_num);
	}

	if (receivedPacket->Ip_protocol == 0x11) { // udp protocol (17) 확인
		((CUDPLayer*)GetUpperLayer(2))->SetReceivePseudoHeader(receivedPacket->Ip_srcAddressByte, receivedPacket->Ip_dstAddressByte, (unsigned short) htons(ntohs(receivedPacket->Ip_len) - IP_HEADER_SIZE));
		return GetUpperLayer(2)->Receive((unsigned char *)receivedPacket->Ip_data, dev_num);
	}

	return FALSE;
}

void CIPLayer::ResetHeader( )
{
	Ip_header.Ip_version = 0x45; // version, header length
	Ip_header.Ip_typeOfService = 0x00;
	Ip_header.Ip_len = 0x0014; // 20byte default
	Ip_header.Ip_id = 0x0000;		
	Ip_header.Ip_fragmentOffset = 0x0000;
	Ip_header.Ip_timeToLive = 0x05; // ttl default
	Ip_header.Ip_protocol = 0x00; // setting
	Ip_header.Ip_checksum = 0x0000; 
	memset(Ip_header.Ip_srcAddressByte, 0, 4);
	memset(Ip_header.Ip_dstAddressByte, 0, 4);
	memset(Ip_header.Ip_data, 0, IP_MAX_DATA);
}

int CIPLayer::Forwarding(unsigned char destip[4]) 
{
	CRouterDlg::RoutingTable entry;
	int size = CRouterDlg::route_table.GetCount();
	unsigned char networkid[4];

	for(int index = 0; index < size; index++) {
		entry = CRouterDlg::route_table.GetAt(CRouterDlg::route_table.FindIndex(index));
		for(int i = 0; i < 4; i++)
			networkid[i] = destip[i] & entry.subnetmask[i];
		if(!memcmp(networkid, entry.ipAddress,  4)) 
			return index; // IP가 일치하는 Entry가 존재하면 그 index return.
	}
	return -1;
}
