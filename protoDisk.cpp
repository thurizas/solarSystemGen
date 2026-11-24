#include "protoDisk.h"
#include "star.h"

protoDisk::protoDisk(star* primary, std::mt19937* pgen,int rings) : m_pPrimary(primary), m_pGen(pgen), m_cntRings(rings)
{
  genDisk();
}

protoDisk::~protoDisk()
{

}

int protoDisk::getRingIndex(double r) const
{
  if (r < m_innerEdge || r >= m_outerEdge || m_rings.empty()) 
    return -1;
  double logMin = std::log(m_innerEdge);
  double logMax = std::log(m_outerEdge);
  
  int idx = static_cast<int>(((std::log(r) - logMin) / (logMax - logMin)) * m_rings.size());
  return std::clamp(idx, 0, static_cast<int>(m_rings.size()) - 1);
}

double protoDisk::getTotalRemainingGas() const
{
  double sum = 0.0;
  for (const auto& r : m_rings) 
    sum += r.gasMass;
  return sum;
}

double protoDisk::getTotalRemainingDust() const
{
   double sum = 0.0;
   for (const auto& r : m_rings) 
     sum += r.dustMass;
   return sum;
}

void protoDisk::genDisk()
{
  m_rings.clear();
  double totalDiskMass = 0.025 * m_pPrimary->mass();
  m_initialGasMass = totalDiskMass * 0.99; // Standard 100:1 gas-to-dust ratio
  m_initialDustMass = 0.0;

  m_innerEdge = std::max(m_pPrimary->ISL().first * 1.2, m_pPrimary->radius() * 3.0);
  m_outerEdge = 35.0 * std::pow(m_pPrimary->mass(), 0.5);
  m_gasDissipation = 1.0;
  m_lifetimeYr = 3.0e6 / std::pow(m_pPrimary->mass(), 0.7);
  
  double integralFactor = 4.0 * PI * (std::sqrt(m_outerEdge) - std::sqrt(m_innerEdge));
  double sigmaGas0 = m_initialGasMass / integralFactor;
  
  // Logarithmic radial spacing gives better resolution in the inner terrestrial/HZ region
  double logMin = std::log(m_innerEdge);
  double logMax = std::log(m_outerEdge);
  double dLog = (logMax - logMin) / m_cntRings;

  for (int i = 0; i < m_cntRings; ++i) 
  {
    DiskRing ring;
    ring.rInner = std::exp(logMin + i * dLog);
    ring.rOuter = std::exp(logMin + (i + 1) * dLog);
    ring.areaAU2 = PI * (ring.rOuter * ring.rOuter - ring.rInner * ring.rInner);

    double rMid = 0.5 * (ring.rInner + ring.rOuter);
    double sigmaGas = sigmaGas0 * std::pow(rMid, -1.5);
    ring.gasMass = sigmaGas * ring.areaAU2;

    // Dust is 1% of gas inside snow line, boosted 3x by water ice condensation outside snow line
    double dustToGas = (rMid > m_pPrimary->frostline()) ? 0.03 : 0.01;
    ring.dustMass = ring.gasMass * dustToGas;
    m_initialDustMass += ring.dustMass;

    m_rings.push_back(ring);
  }
}
