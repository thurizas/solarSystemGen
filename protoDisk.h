#ifndef _protodisk_h_
#define _protodisk_h_

#include "star.h"

#include <random>


struct DiskRing {
  double rInner;      // inner radius in AU
  double rOuter;      // outer radius in AU
  double areaAU2;     // area of the annulus in AU^2
  double gasMass;     // mass of gaseous matter in annulus, relative to M_sun
  double dustMass;    // mass of dust in the annulus, relative to M_sun
};

class protoDisk
{
public:
  protoDisk(star*, std::mt19937*, int rings = 40);
  ~protoDisk();

  std::pair<double_t, double_t> edges() { return std::pair<double_t, double_t>(m_innerEdge, m_outerEdge); }

  int ringCnt() { return m_cntRings; }

  double_t initialGasMass() { return m_initialGasMass; }
  double_t initialDustMass() { return m_initialDustMass; }

  int getRingIndex(double_t r) const;
  double_t getTotalRemainingGas() const;
  double_t getTotalRemainingDust() const;
  
  void ring(int32_t ndx, double_t newDust) { m_rings.at(ndx).dustMass = newDust; }
  
  double_t gasDissipation() { return m_gasDissipation; }
  void gasDissipation(double_t d) { m_gasDissipation = d; }

  double_t lifetime() { return m_lifetimeYr; }

  std::vector<DiskRing>& rings() { return m_rings; }
  DiskRing& ring(int32_t ndx) { return m_rings.at(ndx); }
  //const DiskRing& ring(int32_t ndx) { return m_rings.at(ndx); }

private:
  star*           m_pPrimary;
  std::mt19937*   m_pGen;
  double_t        m_innerEdge;       // inner edge in AU
  double_t        m_outerEdge;       // outer edge in AU
  double_t        m_initialGasMass;  // total mass of gas initially
  double_t        m_initialDustMass; // total mass of dust initially
  double_t        m_sigma0;          // Surface density at 1 AU (M_sun / AU^2)
  double_t        m_gasDissipation;  // Exponential decay factor [0..1]
  double_t        m_lifetimeYr;      // Gas disk dispersal timescale (~3 Myr)
  int32_t         m_cntRings;
  std::vector<DiskRing> m_rings;     // array of rings in the proto-planetary disk
                  
  void genDisk();
};

#endif



