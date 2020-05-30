#pragma once
#include <Bookmark.h>
#include <string>

class GqrxConnection
{
public:
	GqrxConnection();
	~GqrxConnection();

	GqrxConnection(const GqrxConnection&) = delete;
	GqrxConnection& operator=(const GqrxConnection&) = delete;

	enum class Result
	{
		SUCCESS,
		FAIL
	};

	Result jumpToMark(const Bookmark& mark);

	Result getMark(Bookmark& markOut);

protected:
	template<typename... FORMAT>
	Result sendCommand(FORMAT... args);

	template<typename OUT>
	void getParam(const char* cmd, OUT& out);

	void convertString(const char* in, uint64_t& out);
	void convertString(const char* in, uint32_t& out);
	void convertString(const char* in, float& out);
	void convertString(const char* in, std::string& out);

	int m_socket;
};

