#include "star.h"

#include <random>

star::star(std::mt19937* pgen, bool earthlike) : m_pGen(pgen), m_bEarthLike(earthlike)
{
  // Note: O and B weights slightly boosted from 0.00003% so you occasionally see them
  m_classes = { {"O", 0.002, 16.0, 60.0, 30000.0, 50000.0,sf::Color(155, 176, 255)},
                {"B", 0.010,  2.1, 16.0, 10000.0, 30000.0,sf::Color(170, 191, 255)},
                {"A", 0.025,  1.4,  2.1,  7500.0, 10000.0,sf::Color(202, 215, 255)},
                {"F", 0.045, 1.04,  1.4,  6000.0,  7500.0,sf::Color(248, 247, 255)},
                {"G", 0.088, 0.80, 1.04,  5200.0,  6000.0,sf::Color(255, 244, 234)},
                {"K", 0.130, 0.45, 0.80,  3700.0,  5200.0,sf::Color(255, 210, 161)},
                {"M", 0.700, 0.08, 0.45,  2400.0,  3700.0,sf::Color(255, 160, 110)}
    };

  generateStar();
}

star::~star()
{  }

void star::generateStar()
{

  std::uniform_real_distribution<double> uDist(0.0, 1.0);

  if (m_bEarthLike)
  {
    std::normal_distribution<double_t>  massDist(1.00, 0.02);
    std::normal_distribution<double_t> tempDist(5778.0, 50.0);

    m_HRType = "GV2";
    m_mass = std::clamp(massDist(*m_pGen), 0.95, 1.05);
    m_temp = std::clamp(tempDist(*m_pGen), 5650.0, 5900.0);
    m_color = sf::Color(255, 244, 234);
  }
  else
  {
    std::vector<double> freq;
    for (const auto& c : m_classes) freq.push_back(c.freq);

    std::discrete_distribution<size_t> classDist(freq.begin(), freq.end());
    const auto& chosen = m_classes[classDist(*m_pGen)];

    double t = std::pow(uDist(*m_pGen), 1.8);
    m_HRType = chosen.type;
    m_mass = chosen.minMass + t * (chosen.maxMass - chosen.minMass);
    m_temp = chosen.minTemp + t * (chosen.maxTemp - chosen.minTemp);
    m_color = chosen.color;
  }

  // Main-sequence scaling relations
  m_luminosity = std::pow(m_mass, 3.5);
  double radiusSolar = std::pow(m_mass, 0.8);
  m_radius = radiusSolar / AU_TO_R_SUN;
  m_density = RHO_SUN_CGS * (m_mass / std::pow(radiusSolar, 3.0));

  // set intital position, speed and accelleration
  m_pos = { 0.0, 0.0, 0.0 };
  m_vel = { 0.0, 0.0, 0.0 };
  m_acc = { 0.0, 0.0, 0.0 };

  // 1. Inner System Limit: Fluid Roche Limit d = 2.44 * R_* * (rho_* / rho_p)^(1/3)
  m_ISLRock = 2.44 * m_radius * std::cbrt(m_density / 5.51);
  m_ISLGas = 2.44 * m_radius * std::cbrt(m_density / 1.33);

  // 2. Goldilocks (Habitable) Zone (Kopparapu et al. temperature-adjusted flux limits)
  double dT = m_temp - 5780.0;
  double sInner = 1.10 + 1.1e-4 * dT + 1.4e-8 * dT * dT;
  double sOuter = 0.356 + 6.1e-5 * dT + 3.2e-9 * dT * dT;
  m_minHZ = std::sqrt(m_luminosity / sInner);
  m_maxHZ = std::sqrt(m_luminosity / sOuter);

  // Snow line (water condensation boundary)
  m_snowLine = 2.7 * std::sqrt(m_luminosity);

  // 3. Outer System Limit:
  // (a) Galactic Tidal Hill Sphere where Galactic curvature dominates stellar gravity
  m_outerHillLimit = 200000.0 * std::cbrt(m_mass);
  // (b) Spacetime Curvature Threshold where dimensionless metric perturbation GM/(r*c^2) < 1e-13
  double_t curvatureThreshold = 1.0e-13;
  m_curvatureCutoff = (G * m_mass) / (C_AU_YR * C_AU_YR * curvatureThreshold);
}

std::ostream& operator<<(std::ostream& os, star& s)
{
  os << "*** primary star ***";
  os << "mass: " << s.mass() << " solar units\n";
  os << "spectral class: " << s.HRType() << "\n";
  os << "inner system limit (rocky planets): " << s.ISL().first << " AU\n";
  os << "inner system limit (gaseous planets): " << s.ISL().second << " AU\n";
  os << "habitable zone, min: " << s.HZ().first << " AU\n";
  os << "habitable zone, max: " << s.HZ().second << " AU\n";
  os << "frost line (water): " << s.frostline() << " AU\n";
  os << "outer system limit: " << s.OSL().second << " AU\n";

  return os;
}