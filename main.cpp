#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

int main(int argc, char ** argv)
{
	if (argc < 3)
	{
		cout << "Usage: fantastic-bassoon <db.dat> <searchParams.txt> <block size>";
		return -1;
	}

	unsigned long blockSize = 100;
	if (argc == 4)
	{
		try
		{
			blockSize = stoul(argv[3]);
		}
		catch (const invalid_argument & e)
		{
			cout << "Invalid block size!";
			return -1;
		}
		catch (const out_of_range & e)
		{
			cout << "Block size out of range!";
			return -1;
		}
	}

	cout << "Using block size of " << blockSize << endl;

	ifstream dbFile;
	dbFile.open(argv[1], ios::binary);
	if (!dbFile.is_open())
	{
		cout << "Could not open the db file: " << argv[1];
		return -1;
	}

	ifstream searchParamFile;
	searchParamFile.open(argv[2]);
	if (!searchParamFile.is_open())
	{
		cout << "Could not open the search params file: " << argv[1];
		return -1;
	}

	for (string line; getline(searchParamFile, line);)
	{
		istringstream iss(line);
		long index;
		double longitude, latitude;
		iss >> index >> longitude >> latitude;
		cout << "Searching (" << index << ") " << longitude << ", " << latitude << endl;
	}

	cout << "Finished!" << endl;
	return 0;
}
