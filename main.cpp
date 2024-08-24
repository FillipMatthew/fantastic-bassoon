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

template <typename T> struct SLocation
{
	T latitude;
	T longitude;

	bool operator<(const SLocation<T> & rhv) const
	{
		if (latitude != rhv.latitude)
			return latitude < rhv.latitude;

		if (longitude != rhv.longitude)
			return longitude < rhv.longitude;

		return false;
	}
};

struct SGPSData
{
	SLocation<float> location;
	uint64_t recordedTimeUTC;
};

struct SVehicleData
{
	int32_t id;
	string registration;
	vector<SGPSData> gpsData;
};

bool LoadDB(const string & fileName, map<string, SVehicleData> & vehicles);
bool ReadEntry(char * buffer, size_t bufferLen, SVehicleData & vehicle, size_t & read);
void OutputVehicleDetails(const SVehicleData & vehicle);
bool ParseParams(int argc, char ** argv, EDuplicateFilterType & duplicateFilter, float & blockSize);
void PrintUsage();

int main(int argc, char ** argv)
{
	if (argc < 3)
	{
		PrintUsage();
		return -1;
	}

	EDuplicateFilterType duplicateFilter = DF_NONE;
	float blockSize = 5;
	if (!ParseParams(argc, argv, duplicateFilter, blockSize))
		return -1;

	cout << "Using block size of " << blockSize << endl;

	map<string, SVehicleData> vehicles;
	if (!LoadDB(argv[1], vehicles))
		return -1;

	map<SLocation<int32_t>, vector<const SVehicleData *>> sortedVehicles;

	if (duplicateFilter == DF_NONE)
	{
		for (const auto & vehicleIter : vehicles)
		{
			const auto & vehicle = vehicleIter.second;
			for (const auto & gpsData : vehicle.gpsData)
			{
				SLocation<int32_t> loc { static_cast<int32_t>(gpsData.location.latitude / blockSize),
					                     static_cast<int32_t>(gpsData.location.longitude / blockSize) };
				sortedVehicles[loc].push_back(&vehicle);
			}
		}
	}
	else
	{
		for (const auto & vehicleIter : vehicles)
		{
			const auto & vehicle = vehicleIter.second;

			SGPSData filterMatch;
			bool bFirst = true;
			for (const auto & gpsData : vehicle.gpsData)
			{
				if (bFirst)
				{
					bFirst = false;
					filterMatch = gpsData;
				}

				if ((duplicateFilter == DF_LATEST && gpsData.recordedTimeUTC > filterMatch.recordedTimeUTC)
				    || (duplicateFilter == DF_OLDEST && gpsData.recordedTimeUTC <= filterMatch.recordedTimeUTC))
				{
					filterMatch = gpsData;
				}
			}

			SLocation<int32_t> loc { static_cast<int32_t>(filterMatch.location.latitude / blockSize),
				                     static_cast<int32_t>(filterMatch.location.longitude / blockSize) };
			sortedVehicles[loc].push_back(&vehicle);
		}
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
		double latitude, longitude;
		iss >> index >> latitude >> longitude;
		cout << "Searching (" << index << ") " << latitude << ", " << longitude << endl;
	}

	cout << "Finished!" << endl;
	return 0;
}

bool LoadDB(const string & fileName, map<string, SVehicleData> & vehicles)
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

	size_t duplicates = 0;
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
			auto result = vehicles.insert({ vehicle.registration, vehicle });
			if (!result.second) // Already exist, add to GPS data
			{
				++duplicates;
				result.first->second.gpsData.insert(result.first->second.gpsData.end(), vehicle.gpsData.begin(), vehicle.gpsData.end());
			}
		}

		bufferLen -= processed;

		if (bufferLen > 0)
			memmove(buffer.data(), buffer.data() + processed, bufferLen);
	}

	cout << "Loaded " << vehicles.size() << " entries. " << duplicates << " duplicates." << endl;

	return true;
}

bool ReadEntry(char * buffer, size_t bufferLen, SVehicleData & vehicle, size_t & read)
{
	constexpr size_t LAST_PART_SIZE = sizeof(SGPSData::location.latitude) + sizeof(SGPSData::location.longitude) + sizeof(SGPSData::recordedTimeUTC);
	constexpr size_t MIN_SIZE = sizeof(SVehicleData::id) + sizeof(char) /*null*/ + LAST_PART_SIZE;
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
		SGPSData gpsData;
		gpsData.location.latitude = *reinterpret_cast<float *>(buffer + read);
		read += sizeof(SGPSData::location.latitude);
		gpsData.location.longitude = *reinterpret_cast<float *>(buffer + read);
		read += sizeof(SGPSData::location.longitude);
		gpsData.recordedTimeUTC = *reinterpret_cast<uint64_t *>(buffer + read);
		read += sizeof(SGPSData::recordedTimeUTC);
		vehicle.gpsData.clear();
		vehicle.gpsData.push_back(gpsData);
		return true;
	}

	return false;
}

void OutputVehicleDetails(const SVehicleData & vehicle)
{
	cout << "ID: " << vehicle.id << "\tReg: '" << vehicle.registration << endl;
	for (const auto & gpsData : vehicle.gpsData)
		cout << "\t'lat: " << gpsData.location.latitude << "\tlong: " << gpsData.location.longitude << "\ttime: " << gpsData.recordedTimeUTC << endl;
}

bool ParseParams(int argc, char ** argv, EDuplicateFilterType & duplicateFilter, float & blockSize)
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
				blockSize = stof(argv[3]);
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
					blockSize = stof(argv[3]);
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
