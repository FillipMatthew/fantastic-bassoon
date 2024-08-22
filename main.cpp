#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

using namespace std;

struct SVehicleData
{
	int32_t id;
	std::string registration;
	float latitude;
	float longitude;
	uint64_t recordedTimeUTC;
};

bool LoadDB(const std::string & fileName);
size_t ReadEntry(char * buffer, size_t bufferLen, SVehicleData & vehicle);

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

	if (!LoadDB(argv[1]))
		return -1;

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

bool LoadDB(const std::string & fileName)
{
	ifstream dbFile;
	dbFile.open(fileName, ios::binary);
	if (!dbFile.is_open())
	{
		cout << "Could not open the db file: " << fileName;
		return false;
	}

	static const size_t BUFFER_SIZE = 1024;
	std::array<char, BUFFER_SIZE> buffer;

	std::set<int32_t> vehicleIds;

	size_t bufferLen = 0;
	while (!dbFile.eof() && !dbFile.fail())
	{
		bufferLen += dbFile.readsome(buffer.data(), BUFFER_SIZE - bufferLen);

		size_t processed = 0;
		SVehicleData vehicle;
		size_t read = 0;
		while ((read = ReadEntry(buffer.data() + processed, bufferLen - processed, vehicle)) > 0)
		{
			if (vehicleIds.count(vehicle.id))
				cout << "Multiple entries for: " << vehicle.id << endl;

			vehicleIds.insert(vehicle.id);

			processed += read;
		}

		if (processed < bufferLen)
		{
			memmove(buffer.data(), buffer.data() + processed, bufferLen - processed);
			bufferLen -= processed;
		}
	}

	return true;
}

size_t ReadEntry(char * buffer, size_t bufferLen, SVehicleData & vehicle)
{
	return 0;
}
