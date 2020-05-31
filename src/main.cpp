#include <GqrxConnection.h>
#include <XlibKeyConnection.h>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

// Return \a numMarks writable Bookmarks stored in \a path, creating
// the file if it does not exist.
std::optional<Bookmark>* getBookmarkData(const char* path, int numMarks)
{
	int filefd = open(path, O_CREAT | O_RDWR, 0666);
	if(filefd < 0)
		return nullptr;
	const unsigned markSize = sizeof(std::optional<Bookmark>);

	// Ensure a minimum file size by writing up to numMarks empty marks:
	while(lseek(filefd, 0, SEEK_END) < markSize * numMarks)
	{
		char empty[markSize] = {0};
		write(filefd, empty, markSize);
	}

	lseek(filefd, 0, SEEK_SET);
	void* data = mmap(nullptr, markSize * numMarks, PROT_READ | PROT_WRITE, MAP_SHARED, filefd, 0);
	close(filefd);
	return reinterpret_cast<std::optional<Bookmark>*>(data);
}

int main()
{
	GqrxConnection a;
	XlibKeyConnection b;

	const char* savePath = "/home/eoin/.config/gqrx/savedMarks.dat";
	const int numMarks = 12;
	auto marks = getBookmarkData(savePath, numMarks);

	b.m_memoryCallback = [&](XlibKeyConnection::Mode mode, int slot)
	{
		if(mode == XlibKeyConnection::Mode::SAVE)
		{
			Bookmark cur;
			if(a.getMark(cur) == GqrxConnection::Result::SUCCESS)
			{
				marks[slot] = cur;
				std::cout << "Saving " << cur.m_frequency << " to slot " << slot << "\n";
			}
			else
			{
				std::cout << "gqrx connection error\n";
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
