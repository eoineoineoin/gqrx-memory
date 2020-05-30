#include <GqrxConnection.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <signal.h>

GqrxConnection::GqrxConnection()
{
	signal(SIGPIPE, SIG_IGN);
	m_socket = socket(AF_INET, SOCK_STREAM, 0);

	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0; 

	const char* hostname = "localhost"; 
	const char* servicename = "7356";
	struct addrinfo* result;
	getaddrinfo(hostname, servicename, &hints, &result);

	connect(m_socket, result->ai_addr, result->ai_addrlen);

	freeaddrinfo(result);
}

GqrxConnection::~GqrxConnection()
{
	if(m_socket >= 0)
	{
		//close(m_socket);
	}
}

auto GqrxConnection::jumpToMark(const Bookmark& mark) -> Result
{
	std::string buf;
	sendCommand("F %li\n", mark.m_frequency);
	sendCommand("L SQL %0.2f\n", mark.m_squelch);
	sendCommand("M %s %i\n", mark.m_mode, mark.m_passband);
	return Result::SUCCESS;
}

template<typename... FORMAT>
auto GqrxConnection::sendCommand(FORMAT... args) -> Result
{
	char buf[1024];
	int nMessageBytes = snprintf(buf, sizeof(buf), args...);
	int nBytesSent = send(m_socket, buf, nMessageBytes, 0);
	if(nMessageBytes != nBytesSent)
	{
		return Result::FAIL;
	}

	int nResponseBytes = recv(m_socket, buf, sizeof(buf), 0);
	const char* successMessage = "RPRT 0";
	if(std::strncmp(successMessage, buf, nResponseBytes) != 0)
	{
		return Result::FAIL;
	}
	return Result::SUCCESS;
}

auto GqrxConnection::getMark(Bookmark& markOut) -> Result
{
	getParam("f\n", markOut.m_frequency);
	getParam("l SQL\n", markOut.m_squelch);

	std::string modeStr;
	getParam("m\n", modeStr);

	size_t newlineIdx = modeStr.find('\n');
	size_t passBandStart = 1 + newlineIdx;
	
	convertString(modeStr.c_str() + passBandStart, markOut.m_passband);
	if(newlineIdx > sizeof(markOut.m_mode))
	{
		return Result::FAIL;
	}
	std::memcpy(&markOut.m_mode, modeStr.c_str(), newlineIdx);

	return Result::SUCCESS;
}

template<typename OUT>
void GqrxConnection::getParam(const char* cmd, OUT& out)
{
	send(m_socket, cmd, std::strlen(cmd), 0);

	char buf[1024] = {0};
	int nBytes = recv(m_socket, buf, sizeof(buf), 0);

	convertString(buf, out);
}

void GqrxConnection::convertString(const char* in, uint64_t& out)
{
	out = std::atoll(in);
}

void GqrxConnection::convertString(const char* in, uint32_t& out)
{
	out = std::atol(in);
}


void GqrxConnection::convertString(const char* in, float& out)
{
	out = std::atof(in);
}

void GqrxConnection::convertString(const char* in, std::string& out)
{
	out = in;
}
