# fantastic-bassoon
Yes, this was the random name suggestion from the Github interface.

This is an assessment project that a friend passed on to me from stuff they had done. I've done my implementation in C++.

**Build with Meson:**<br>
meson setup build<br>
cd build<br>
meson install

gcc on Windows requires 'libstdc++-6.dll' to run the built executible.
The executible is output to bin/fantastic-bassoon.exe

**Usage:**<br>
fantastic-bassoon db.dat search-params.txt

## Nearest Vehicle Positions:

Provided with a binary data file which contains a position for each of 2 million vehicles. Your task is to write a program that can find and list the registration numbers of the nearest vehicle position in the data file to each of the 10 co-ordinates provided below. In addition to being able to do this, however, your program must be able to complete all 10 lookups in less time than our benchmark. This benchmark is based on simply looping through each of the 2 million positions and keeping the closest to each given co-ordinate. This is simply repeated for each of the 10 provided co-ordinates.

The challenge set to you is to think of a more efficient way to achieve this and to implement it -- not just parallelising the brute force method explained above doing a total of 20 million distance calculations. Please do not make use of any external libraries. This purpose of the assessment is to evaluate your coding and problem-solving skills, not your NuGet package management skills.

Having code as described is the pseudo code below (or similar), resulting in a distance calculation between each of the test points and each of the vehicles (20 million in this case), will result in an instant disqualification.
```
for each(firstListItem in firstList)
{
  for each(secondListItem in secondList)
  {
  calculate distance between two items
  }
}
```

Ideally, your solution should be built as a Windows Console Application.

The structure of the binary data file is as follows:
|  Field  |  Data Type  |
|---------|-------------|
|  VehicleId  |  Int32  |
|  VehicleRegistration  |  Null Terminated ASCII String  |
|  Latitude  |  Float (4 byte floating-point number)  |
|  Longitude  |  Float (4 byte floating-point number)  |
|  RecordedTimeUTC  |  UInt64 (number of seconds since Epoch)  |

The 10 co-ordinates to find the closed vehicle positions to are as follows:<br>
|  Position  #  |  Latitude  |  Longitude  |
|---------------|------------|-------------|
|  1  |  34.544909  |  -102.100843  |
|  2  |  32.345544  |  -99.123124  |
|  3  |  33.234235  |  -100.214124  |
|  4  |  35.195739  |  -95.348899  |
|  5  |  31.895839  |  -97.789573  |
|  6  |  32.895839  |  -101.789573  |
|  7  |  34.115839  |  -100.225732  |
|  8  |  32.335839  |  -99.992232  |
|  9  |  33.535339  |  -94.792232  |
|  10  |  32.234235  |  -100.222222  |
