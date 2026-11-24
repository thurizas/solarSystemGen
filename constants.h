#ifndef _constants_h_
#define _constants_h_

// Astronomical Units: Length = AU, Time = Earth Years, Mass = Solar Masses (M_sun)
constexpr double PI = 3.14159265358979323846;
constexpr double G = 4.0 * PI * PI;             // AU^3 / (M_sun * yr^2)
constexpr double AU2KM = 1.496E8;               // 1 AU = 1.496E8 km
constexpr double C_AU_YR = 63241.077;           // speed of light in AU/year
constexpr double M_EARTH = 3.003e-6;            // Earth mass in M_sun
constexpr double M_JUPITER = 9.545e-4;          // Jupiter mass in M_sun
constexpr double AU_TO_R_SUN = 215.032;         // 1 AU in Solar Radii
constexpr double RHO_SUN_CGS = 1.408;           // Solar mean density (g/cm^3)

const uint8_t MAJOR = 1;
const uint8_t MINOR = 0;
const uint16_t PATCH = 0;



#endif
