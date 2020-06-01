#pragma once
#include <Bookmark.h>
#include <string>

class GqrxConnection
{
public:
	GqrxConnection(const char* host, const char* port);
	~GqrxConnection();

	GqrxConnection(const GqrxConnection&) = delete;
	GqrxConnection& operator=(const GqrxConnection&) = delete;

	struct Result
	{
		enum Enum
		{
			SUCCESS,
			FAIL
		};

		Result(Enum v) : m_value(v) {}
		operator bool() const { return m_value == SUCCESS; }
		Result operator&&(const Result& other) { return (*this && other) ? SUCCESS : FAIL; }
	protected:
		Enum m_value;
	};

	Result jumpToMark(const Bookmark& mark);

	Result getMark(Bookmark& markOut);

protected:
	void reconnect();

	template<typename... FORMAT>
	Result sendCommand(FORMAT... args);

	template<typename OUT>
	void getParam(const char* cmd, OUT& out);

	void convertString(const char* in, uint64_t& out);
	void convertString(const char* in, uint32_t& out);
	void convertString(const char* in, float& out);
	void convertString(const char* in, std::string& out);

	const char* m_host;
	const char* m_port;
	int m_socket = -1;
};

