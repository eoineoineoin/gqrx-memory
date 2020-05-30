#pragma once
#include <functional>

class XlibKeyConnection
{
public:
	XlibKeyConnection();
	~XlibKeyConnection();

	void run();

	enum class Mode
	{
		SAVE,
		LOAD
	};

	std::function<void(Mode, int)> m_memoryCallback;
};

