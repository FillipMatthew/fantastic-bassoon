#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string_view>
#include <vector>

using namespace std;

enum EDuplicateFilterType
{
	DF_NONE = 0,
	DF_LATEST = 1,
	DF_OLDEST = 2
};

struct SVehicleData
{
	int32_t id;
	string registration;
	float latitude;
	float longitude;
	uint64_t recordedTimeUTC;
};

bool LoadDB(const string & fileName, vector<SVehicleData> & vehicles);
bool ReadEntry(char * buffer, size_t bufferLen, SVehicleData & vehicle, size_t & read);
void OutputVehicleDetails(const SVehicleData & vehicle);
bool ParseParams(int argc, char ** argv, EDuplicateFilterType & duplicateFilter, uint32_t & blockSize);
void PrintUsage();

int main(int argc, char ** argv)
{
	if (argc < 3)
	{
		PrintUsage();
		return -1;
	}

	EDuplicateFilterType duplicateFilter = DF_NONE;
	uint32_t blockSize = 100;
	if (!ParseParams(argc, argv, duplicateFilter, blockSize))
		return -1;

	cout << "Using block size of " << blockSize << endl;

	vector<SVehicleData> vehicles;
	if (!LoadDB(argv[1], vehicles))
		return -1;

	cout << "Loaded " << vehicles.size() << " entries." << endl;

	map<string, const SVehicleData *> filteredVehicles;

	if (duplicateFilter == DF_NONE)
	{
		for (const auto & vehicle : vehicles)
		{
		}
	}
	else
	{
		size_t duplicates = 0;
		for (const auto & vehicle : vehicles)
		{
			auto iter = filteredVehicles.find(vehicle.registration);
			if (iter == filteredVehicles.end())
				filteredVehicles[vehicle.registration] = &vehicle;
			else
			{
				++duplicates;
				if ((duplicateFilter == DF_LATEST && vehicle.recordedTimeUTC > iter->second->recordedTimeUTC)
				    || (duplicateFilter == DF_OLDEST && vehicle.recordedTimeUTC <= iter->second->recordedTimeUTC))
				{
					iter->second = &vehicle;
				}
			}
		}

		cout << duplicates << " duplicates filtered." << endl;
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

bool LoadDB(const string & fileName, vector<SVehicleData> & vehicles)
{
	ifstream dbFile;
	dbFile.open(fileName, ios::binary);
	if (!dbFile.is_open())
	{
		cout << "Could not open the db file: " << fileName;
		return false;
	}

	static const size_t BUFFER_SIZE = 1024 * 1024;
	array<char, BUFFER_SIZE> buffer;

	size_t bufferLen = 0;
	while (dbFile)
	{
		dbFile.read(buffer.data() + bufferLen, BUFFER_SIZE - bufferLen);
		bufferLen += dbFile.gcount();

		size_t processed = 0;
		SVehicleData vehicle;
		size_t read = 0;
		while (ReadEntry(buffer.data() + processed, bufferLen - processed, vehicle, read))
		{
			processed += read;
			vehicles.push_back(vehicle);
		}

		bufferLen -= processed;

		if (bufferLen > 0)
			memmove(buffer.data(), buffer.data() + processed, bufferLen);
	}

	return true;
}

bool ReadEntry(char * buffer, size_t bufferLen, SVehicleData & vehicle, size_t & read)
{
	constexpr size_t LAST_PART_SIZE = sizeof(vehicle.latitude) + sizeof(vehicle.longitude) + sizeof(vehicle.recordedTimeUTC);
	constexpr size_t MIN_SIZE = sizeof(vehicle.id) + sizeof(char) /*null*/ + LAST_PART_SIZE;
	if (bufferLen < MIN_SIZE)
		return false;

	vehicle.id = *reinterpret_cast<int32_t *>(buffer);
	read = sizeof(vehicle.id);
	for (; read < bufferLen; ++read)
	{
		if (buffer[read] == '\0')
		{
			++read;
			break;
		}
	}

	if ((bufferLen - read) >= LAST_PART_SIZE)
	{
		vehicle.registration = string_view(buffer + sizeof(vehicle.id), (read - 1 /*don't include null*/) - sizeof(vehicle.id));
		vehicle.latitude = *reinterpret_cast<float *>(buffer + read);
		read += sizeof(vehicle.latitude);
		vehicle.longitude = *reinterpret_cast<float *>(buffer + read);
		read += sizeof(vehicle.longitude);
		vehicle.recordedTimeUTC = *reinterpret_cast<uint64_t *>(buffer + read);
		read += sizeof(vehicle.recordedTimeUTC);
		return true;
	}

	return false;
}

void OutputVehicleDetails(const SVehicleData & vehicle)
{
	cout << "ID: " << vehicle.id << " Reg: '" << vehicle.registration << "' lat: " << vehicle.latitude << " long: " << vehicle.longitude
	     << " time: " << vehicle.recordedTimeUTC << endl;
}

bool ParseParams(int argc, char ** argv, EDuplicateFilterType & duplicateFilter, uint32_t & blockSize)
{
	// The params parsing is a mess :|
	if (argc > 3)
	{
		if (string(argv[3]) == "latest")
			duplicateFilter = DF_LATEST;
		else if (string(argv[3]) == "oldest")
			duplicateFilter = DF_OLDEST;
		else
		{
			try
			{
				blockSize = stoul(argv[3]);
			}
			catch (const invalid_argument & e)
			{
				cout << "Invalid params!" << endl;
				PrintUsage();
				return false;
			}
			catch (const out_of_range & e)
			{
				cout << "Invalid params!" << endl;
				PrintUsage();
				return false;
			}
		}

		if (argc > 4)
		{
			if (duplicateFilter == DF_NONE && string(argv[3]) == "latest")
				duplicateFilter = DF_LATEST;
			else if (duplicateFilter == DF_NONE && string(argv[3]) == "oldest")
				duplicateFilter = DF_OLDEST;
			else
			{
				try
				{
					blockSize = stoul(argv[3]);
				}
				catch (const invalid_argument & e)
				{
					cout << "Invalid params!" << endl;
					PrintUsage();
					return false;
				}
				catch (const out_of_range & e)
				{
					cout << "Invalid params!" << endl;
					PrintUsage();
					return false;
				}
			}
		}
	}

	return true;
}

void PrintUsage()
{
	cout << "Usage: fantastic-bassoon <db.dat> <searchParams.txt> [latest/oldest] [block size]";
}
