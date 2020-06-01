#include <GqrxConnection.h>
#include <XlibKeyConnection.h>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <cstring>
#include <getopt.h>


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

struct Options
{
	const char* m_savePath = nullptr;
	const char* m_gqrxHost = "localhost";
	const char* m_gqrxPort = "7356";
};

void printHelp()
{
	const char* version = "1.0";
	std::cout << "gqrx-memory " << version << std::endl;
	std::cout << "Arguments:" << std::endl;
	std::cout << "  -h, --help             : This help message" << std::endl;
	std::cout << "  -s, --savefile <file>  : Load/save memory to <file>" << std::endl;
	std::cout << "  -c, --host <hostname>  : Connect to gqrx on <hostname> (default: localhost)" << std::endl;
	std::cout << "  -p, --port <port>      : Connect to gqrx on <port> (default: 7356)" << std::endl;
}

Options parseOptions(int argc, char** argv)
{
	Options parsed;
	static struct option long_options[] = {
		{"help", no_argument, 0, 0},
		{"savefile", required_argument, 0, 0},
		{"host", required_argument, 0, 0},
		{"port", required_argument, 0, 0},
		{0, 0, 0, 0},
	};
	int c;
	while(1) {
		int option_index = 0;
		c = getopt_long (argc, argv, "hs:c:p:", long_options, &option_index);
		if (c == -1)
			break;
		if(c == '?' || c == 'h') {
			printHelp();
			exit(1);
		}
		else if(c == 's' || (c == 0 && std::strcmp(long_options[option_index].name, "savefile") == 0))
			parsed.m_savePath = optarg;
		else if(c == 'c' || (c == 0 && std::strcmp(long_options[option_index].name, "host") == 0))
			parsed.m_gqrxHost = optarg;
		else if(c == 'p' || (c == 0 && std::strcmp(long_options[option_index].name, "port") == 0))
			parsed.m_gqrxPort = optarg;
	}
	return parsed;
}

int main(int argc, char** argv)
{
	Options options = parseOptions(argc, argv);

	GqrxConnection a(options.m_gqrxHost, options.m_gqrxPort);
	XlibKeyConnection b;

	const char* savePath = "/home/eoin/.config/gqrx/savedMarks.dat";
	const int numMarks = 12;
	auto marks = getBookmarkData(savePath, numMarks);

	b.m_memoryCallback = [&](XlibKeyConnection::Mode mode, int slot)
	{
		if(mode == XlibKeyConnection::Mode::SAVE)
		{
			Bookmark cur;
			if(a.getMark(cur))
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
