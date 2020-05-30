#include <GqrxConnection.h>
#include <XlibKeyConnection.h>
#include <iostream>
#include <unistd.h>

int main()
{
	GqrxConnection a;
	XlibKeyConnection b;

	std::optional<Bookmark> marks[12];

	b.m_memoryCallback = [&](XlibKeyConnection::Mode mode, int slot)
	{
		if(mode == XlibKeyConnection::Mode::SAVE)
		{
			Bookmark cur;
			std::cout << "Saving to slot " << slot << "\n";
			if(a.getMark(cur) == GqrxConnection::Result::SUCCESS)
			{
				marks[slot] = cur;
				std::cout << "\t" << marks[slot]->m_frequency << "Hz\n";
			}
		}
		else if(mode == XlibKeyConnection::Mode::LOAD && marks[slot].has_value())
		{
			std::cout << "Jumping to slot " << slot << "\n";
			a.jumpToMark(*marks[slot]);
		}
	};

	b.run();

	return 0;
}
