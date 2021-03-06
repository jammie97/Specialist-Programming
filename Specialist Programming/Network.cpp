#pragma comment(lib, "Ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>       // Windows networking
#include "Network.h"
//#include "dynamicObjects.h"
//#include "ErrorLogger.h"    // WriteLn & error reporting
//#include "game.h"           // GameTimer
#include <string>           // use of strings for debugging
#include <WS2tcpip.h>       // inet_pton()

// For getting IP address
#include <fstream>
#include <locale>
#include <codecvt>


Network* Network::m_pInst = nullptr;
bool Network::m_isActive = false;

using namespace std;

Network::Network()
{
	//memset(&data, 0, sizeof(NetData));
	m_isHost = true;
	m_isHost = 1;
	m_dataRecieved = 0;
	m_nonBlocking = 1;
	m_MyIP = L"";
}

wchar_t * Network::GetLocalIP()
{
	string line;
	ifstream IPFile;
	int offset;
	char* search0 = "IPv4 Address. . . . . . . . . . . :";      // search pattern

	system("ipconfig > ip.txt");

	IPFile.open("ip.txt");
	if (IPFile.is_open())
	{
		while (!IPFile.eof())
		{
			getline(IPFile, line);
			if ((offset = line.find(search0, 0)) != string::npos)
			{
				//   IPv4 Address. . . . . . . . . . . : 1
				//1234567890123456789012345678901234567890     
				line.erase(0, 39);
				//cout << line << endl;
				IPFile.close();

				break;
			}
		}
	}

	wchar_t l_temp[15];
	for (int i = 0; i < 15; i++)
	{
		if (i < line.length())
			l_temp[i] = line[i];
		else
			l_temp[i] = '\0';
	}

	size_t l_outputLen = wcslen(l_temp);
	wchar_t* l_output = new wchar_t[l_outputLen+1];
	wcscpy(l_output, l_temp);
	//m_MyIP = l_output;
	//memset(m_MyIP, 10000, 10000);
	//memcpy(m_MyIP, &l_output, sizeof(&l_output));
	return l_output;
}

void Network::CloseConnection()
{
	ExitClents();
	m_isActive = false;
	closesocket(m_socket);
	WSACleanup();
}

void Network::ExitClents()
{
	char l_exit[5] = "exit";

	for (int i = 0; i < m_clients.size(); i++)
	{
		sendto(m_socket, l_exit, sizeof(l_exit), 0,
			(struct sockaddr*) &m_clients[i], sizeof(m_clients[i]));
	}


}

void Network::ExitHost()
{
	char l_exit[5] = "exit";
	//int l_addressLength = sizeof(m_serverAddress);

	int check = sendto(m_socket, l_exit, sizeof(l_exit), 0,
		(struct sockaddr*) &m_serverAddress, sizeof(m_serverAddress));
	for (int i = 0; i < 1000; i++)
	{
		for (int i = 0; i < 500; i++)
		{
			check = sendto(m_socket, l_exit, sizeof(l_exit), 0,
				(struct sockaddr*) &m_serverAddress, sizeof(m_serverAddress));
			if (check >= 0)
				break;
		}

	}

	check = check;

}

bool Network::WSASetup()
{
	bool l_error = true;

	// Start up WSA
	if (WSAStartup(MAKEWORD(2, 0), &m_wsaData) != 0)
	{
		l_error = false;
	}

	return l_error;
}

void Network::Release()
{
	if (m_pInst)
	{
		// Close all connections before delete
		m_pInst->CloseConnection();
		// Delete and set to nullptr
		delete m_pInst;
		m_pInst = nullptr;
	}
}

bool Network::SetupServer()
{
	bool l_error = true;

	if(m_MyIP == L"")
	m_MyIP = GetLocalIP();

	// Setup windows socket
	WSASetup();

	// Create UDP
	if ((m_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		//cout << "Error with socket function!" << endl;
		l_error = false;
	}

	// 
	memset(&m_serverAddress, 0, sizeof(m_serverAddress));
	m_serverAddress.sin_family = AF_INET;
	m_serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	m_serverAddress.sin_port = htons(PORT);

	ioctlsocket(m_socket, FIONBIO, &m_nonBlocking);

	// Binds socket to be used
	bind(m_socket, (struct sockaddr *)&m_serverAddress, sizeof(m_serverAddress));
	//{
	//	cout << "Error with binding socket!" << endl;
	//	l_error = false;
	//}

	return l_error;


}

void Network::CheckNewClients()
{
	// First make an address for the new client
	sockaddr_in l_clientAddress;
	memset(&l_clientAddress, 0, sizeof(l_clientAddress));
	int l_clientLength = sizeof(l_clientAddress);
	// Message to send to new client

	// Connected message
	char l_buffer[3];
	memset(&l_buffer, 0, sizeof(l_buffer));


	recvfrom(m_socket, l_buffer, sizeof(l_buffer), 0,
		(struct sockaddr*)&l_clientAddress, &l_clientLength);


	char l_connectMessage[] = "hi";

	// If client connects, send connected message and proceed to add client
	if (strcmp(l_buffer, l_connectMessage) == 0)
	{
		//std::cout << "New Client!" << std::endl;
		SaveClents(l_clientAddress);
		InitData l_dataToSend;
		l_dataToSend.m_dataSent = 6.9;
		// Setup initial data here to send to new clients
		if (!sendto(m_socket, (char*)&l_dataToSend, sizeof(InitData), 0,
			(struct sockaddr*) &l_clientAddress, sizeof(l_clientAddress)))
		{
			//cout << "Failed to send initial data!" << endl;
		}
	}

}

void Network::SaveClents(sockaddr_in _address)
{
	// Saves the client address so the Network class
	//  remebers
	m_clients.push_back(_address);
}



void Network::Send(SendData _data)
{
	// Check for any exit code
	char l_buffer[sizeof(SendData)];
	memset(&l_buffer, 0, sizeof(SendData));
	memset(&m_data, 0, sizeof(SendData));

	int l_addresLength = sizeof(m_serverAddress);
	int recvLen = 0;

	for (int i = 0; i < m_clients.size(); i++)
	{
		int l_error = recvfrom(m_socket, l_buffer, sizeof(SendData), 0,
			(struct sockaddr*) &m_clients[i], &l_addresLength);

		char exit[] = "exit";
		if (l_error == 5)
		{
			//cout << "Exiting" << endl;
			m_clients.erase(m_clients.begin() + i);
		}

	}

	// Send data to all clients
	m_data = _data;
	for (int i = 0; i < m_clients.size(); i++)
	{

		sendto(m_socket, (char*)&m_data, sizeof(SendData), 0, (struct sockaddr*) &m_clients[i], sizeof(m_clients[i]));
	}

}


bool Network::Recieve()
{
	char l_buffer[sizeof(SendData)];
	memset(&l_buffer, 0, sizeof(SendData));
	memset(&m_data, 0, sizeof(SendData));

	int l_addressLength = sizeof(m_serverAddress);
	int recvLen = 0;
	int l_error = recvfrom(m_socket, l_buffer, sizeof(SendData), 0,
		(struct sockaddr*) &m_serverAddress, &l_addressLength);

	char exit[] = "exit";
	if (strcmp(l_buffer, exit) == 0)
	{
		//cout << "Exiting" << endl;
		return false;
	}

	// Dont overwrite if data didnt recieve this cycle.
	if(l_error != -1)
		memcpy(&m_data, &l_buffer, sizeof(SendData));
}



bool Network::ConnectToServer()
{
	// Connect to the server
	bool l_error = true;
	// First set up windows sockets
	WSASetup();

	// Next create the socket
	if ((m_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		m_isHost = true;
		l_error = false;

		//return l_error;
	}

	// Setup memory
	memset(&m_serverAddress, 0, sizeof(m_serverAddress));
	m_serverAddress.sin_family = AF_INET;
	m_serverAddress.sin_addr.s_addr = inet_addr(IP);
	m_serverAddress.sin_port = htons(PORT);

	char l_connect[] = "hi";
	if (sendto(m_socket, l_connect, strlen(l_connect), 0,
		(struct sockaddr*) &m_serverAddress, sizeof(m_serverAddress))
		!= strlen(l_connect))
	{
		//cout << "Could not make initial connection\n";
		m_isHost = true;
		l_error = false;

		//return l_error;
	}

	// Create initdata struct and fill it
	InitData l_initData;
	// Create anything needed to receive initdata
	int l_recv = 0;
	int l_addressLength = sizeof(m_serverAddress);
	char l_buffer[sizeof(l_initData)];
	memset(&l_buffer, 0, sizeof(l_initData));

	l_recv = recvfrom(m_socket, l_buffer, sizeof(l_initData), 0,
		(struct sockaddr*)&m_serverAddress, &l_addressLength);
	// Get data
	//if (l_recv != -1)
	//{
	//	cout << "Receive Data failed\n";
	//	//return false;
	//}

	if (l_recv != -1)
	{
		// Fill initdata with l_buffer
		memcpy(&l_initData, &l_buffer, sizeof(l_initData));

		m_dataRecieved = l_initData.m_dataSent;




		//cout << "Receive Data failed\n";
	}
	//else
	//{
	//	l_error = false;
	//	//return l_error;
	//}

	ioctlsocket(m_socket, FIONBIO, &m_nonBlocking);

	return l_error;
}
