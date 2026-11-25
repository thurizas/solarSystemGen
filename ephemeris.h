#ifndef _ephemeris_h_
#define _ephemeris_h_

#include <cstdint>

#pragma pack(push, 1)                          // packed, byte alignment

struct EphemerisHeader
{
    char     magic[4] = {'S', 'S', 'E', 'P'};
    uint8_t  endianness = 0;                    // 1 = little endian, 2= big endian
    uint16_t version = 1;
    char     runDateTime[32] = {0};             // null-terminated time stamp
    uint8_t  earthLike = 0;                     // earth like? 1 = true, 0 = false
    uint32_t seed = 0;                          // seed used in the simulation
    double   simTimeYr = 0.0;                   // final simulation time
    double   starMass = 0.0;                    // primary mass in solar units
    uint32_t bodyCount = 0;                     // total count of tracked bodies
};

struct PlanetEphemeris
{
    int32_t id = 0;
    uint8_t isGiant = 0;
    uint8_t inHabitableZone = 0;
    double  massEarth = 0.0;                    // Planetary mass in M_earth
    double  semiMajorAxisAU = 0.0;              // a (AU)
    double  eccentricity = 0.0;                 // e (dimensionless)
    double  inclinationDeg = 0.0;               // i (degrees, [0, 180])
    double  raanDeg = 0.0;                      // Longitude of ascending node, Omega (degrees, [0, 360))
    double  argPeriapsisDeg = 0.0;              // Argument of periapsis, omega (degrees, [0, 360))
    double  periodYr = 0.0;                     // Orbital period of revolution (Earth years)
};

#pragma pack(pop)


#endif
