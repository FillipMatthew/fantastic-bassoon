#include <array>
#include <chrono>
#include <cmath>
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
	int32_t id;
	SLocation<float> location;
	uint64_t recordedTimeUTC;
};

struct SVehicleData
{
	string registration;
	vector<SGPSData> gpsData;
};

struct SSortedVehicleData
{
	SLocation<float> location;
	const SVehicleData * vehicle;
};

struct SSearchBlock
{
	SLocation<int32_t> location;
	float minDistance;
	float maxDistance;
};

bool LoadDB(const string & fileName, map<string, SVehicleData> & vehicles);
bool ReadEntry(char * buffer, size_t bufferLen, SVehicleData & vehicle, size_t & read);
const SVehicleData * FindNearestVehicle(const SLocation<float> location, const map<SLocation<int32_t>, vector<SSortedVehicleData>> & sortedVehicles);
void GetBlockDistances(const SLocation<int32_t> & blockLocation1, const SLocation<int32_t> & blockLocation2, float & minDistance, float & maxDistance);
void OutputVehicleDetails(const SVehicleData & vehicle);
bool ParseParams(int argc, char ** argv, EDuplicateFilterType & duplicateFilter, float & blockSize);
void PrintUsage();

float g_blockSize = 0.25;

int main(int argc, char ** argv)
{
	if (argc < 3)
	{
		PrintUsage();
		return -1;
	}

	EDuplicateFilterType duplicateFilter = DF_NONE;
	if (!ParseParams(argc, argv, duplicateFilter, g_blockSize))
		return -1;

	cout << "Using block size of " << g_blockSize << endl;

	auto startTime = chrono::system_clock::now();
	map<string, SVehicleData> vehicles;
	if (!LoadDB(argv[1], vehicles))
		return -1;

	auto loadedDataTime = chrono::system_clock::now();
	cout << "Data loaded in: " << chrono::duration<double>(loadedDataTime - startTime) << endl;

	auto startSortTime = chrono::system_clock::now();
	map<SLocation<int32_t>, vector<SSortedVehicleData>> sortedVehicles;

	if (duplicateFilter == DF_NONE)
	{
		for (const auto & vehicleIter : vehicles)
		{
			const auto & vehicle = vehicleIter.second;
			for (const auto & gpsData : vehicle.gpsData)
			{
				SLocation<int32_t> loc { static_cast<int32_t>(gpsData.location.latitude / g_blockSize),
					                     static_cast<int32_t>(gpsData.location.longitude / g_blockSize) };
				sortedVehicles[loc].push_back(SSortedVehicleData { gpsData.location, &vehicle });
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

			SLocation<int32_t> loc { static_cast<int32_t>(filterMatch.location.latitude / g_blockSize),
				                     static_cast<int32_t>(filterMatch.location.longitude / g_blockSize) };
			sortedVehicles[loc].push_back(SSortedVehicleData { filterMatch.location, &vehicle });
		}
	}

	auto finishedSortTime = chrono::system_clock::now();
	cout << "Sorted data in: " << chrono::duration<double>(finishedSortTime - startSortTime) << endl;

	ifstream searchParamFile;
	searchParamFile.open(argv[2]);
	if (!searchParamFile.is_open())
	{
		cout << "Could not open the search params file: " << argv[1];
		return -1;
	}

	auto startSearchTime = chrono::system_clock::now();
	for (string line; getline(searchParamFile, line);)
	{
		istringstream iss(line);
		long index;
		float latitude, longitude;
		iss >> index >> latitude >> longitude;
		cout << "\nSearching (" << index << ") " << latitude << ", " << longitude << endl;
		auto vehicle = FindNearestVehicle(SLocation<float> { latitude, longitude }, sortedVehicles);
		cout << "Found:" << endl;
		OutputVehicleDetails(*vehicle);
	}

	auto finishedSearchTime = chrono::system_clock::now();
	cout << "\nFinished search in: " << chrono::duration<double>(finishedSearchTime - startSearchTime) << endl;

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

	static const size_t BUFFER_SIZE = 1024 * 1024; // 1MB chunks
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

	cout << "Loaded " << vehicles.size() << " entries. " << duplicates << " duplicates (repeated registration, different location/time)." << endl;

	return true;
}

bool ReadEntry(char * buffer, size_t bufferLen, SVehicleData & vehicle, size_t & read)
{
	constexpr size_t LAST_PART_SIZE = sizeof(SGPSData::location.latitude) + sizeof(SGPSData::location.longitude) + sizeof(SGPSData::recordedTimeUTC);
	constexpr size_t MIN_SIZE = sizeof(SGPSData::id) + sizeof(char) /*null*/ + LAST_PART_SIZE;
	if (bufferLen < MIN_SIZE)
		return false;

	SGPSData gpsData;
	gpsData.id = *reinterpret_cast<int32_t *>(buffer);
	read = sizeof(SGPSData::id);
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
		vehicle.registration = string_view(buffer + sizeof(SGPSData::id), (read - 1 /*don't include null*/) - sizeof(SGPSData::id));
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

const SVehicleData * FindNearestVehicle(const SLocation<float> location, const map<SLocation<int32_t>, vector<SSortedVehicleData>> & sortedVehicles)
{
	// Find the nearest blocks with overlapping distance range from location
	SLocation<int32_t> blockLocation { static_cast<int32_t>(location.latitude / g_blockSize), static_cast<int32_t>(location.longitude / g_blockSize) };
	vector<SSearchBlock> nearestBlocks;
	float currentMaxDistance;
	for (const auto & block : sortedVehicles)
	{
		float minDistance, maxDistance;
		GetBlockDistances(blockLocation, block.first, minDistance, maxDistance);

		bool bFirst = nearestBlocks.empty();
		if (!bFirst && minDistance > currentMaxDistance)
			continue;

		if (bFirst || maxDistance < currentMaxDistance)
			currentMaxDistance = maxDistance;

		// Remove any block now outside the new max distance
		for (auto iter = nearestBlocks.begin(); iter != nearestBlocks.end();)
		{
			if (iter->minDistance > currentMaxDistance)
				iter = nearestBlocks.erase(iter);
			else
				++iter;
		}

		nearestBlocks.push_back(SSearchBlock { block.first, minDistance, maxDistance });
	}

	// Search blocks for nearest vehicle
	float nearestVehicleDistance;
	const SVehicleData * nearestVehicle = nullptr;
	for (const auto & block : nearestBlocks)
	{
		auto vehicles = sortedVehicles.find(block.location)->second;
		for (const auto & vehicleData : vehicles)
		{
			float deltaLat = vehicleData.location.latitude - location.latitude;
			float deltaLong = vehicleData.location.longitude - location.longitude;
			float distance = sqrt(deltaLat * deltaLat + deltaLong * deltaLong);

			if (nearestVehicle == nullptr || distance < nearestVehicleDistance)
			{
				nearestVehicleDistance = distance;
				nearestVehicle = vehicleData.vehicle;
			}
		}
	}

	return nearestVehicle;
}

void GetBlockDistances(const SLocation<int32_t> & blockLocation1, const SLocation<int32_t> & blockLocation2, float & minDistance, float & maxDistance)
{
	float deltaLat = abs(blockLocation2.latitude - blockLocation1.latitude);
	float deltaLong = abs(blockLocation2.longitude - blockLocation1.longitude);
	float minDeltaLat = deltaLat - g_blockSize;
	float minDeltaLong = deltaLong - g_blockSize;
	float maxDeltaLat = deltaLat + g_blockSize;
	float maxDeltaLong = deltaLong + g_blockSize;
	minDistance = sqrt(minDeltaLat * minDeltaLat + minDeltaLong * minDeltaLong);
	maxDistance = sqrt(maxDeltaLat * maxDeltaLat + maxDeltaLong * maxDeltaLong);
}

void OutputVehicleDetails(const SVehicleData & vehicle)
{
	cout << "Reg: '" << vehicle.registration << "'" << endl;
	for (const auto & gpsData : vehicle.gpsData)
		cout << "\tID: " << gpsData.id << "\tlat: " << gpsData.location.latitude << "\tlong: " << gpsData.location.longitude
		     << "\ttime: " << chrono::system_clock::time_point { chrono::seconds { gpsData.recordedTimeUTC } } << " (" << gpsData.recordedTimeUTC << ")"
		     << endl;
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
