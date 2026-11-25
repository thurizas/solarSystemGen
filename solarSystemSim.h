#ifndef _solarsystemsim_h_
#define _solarsystemsim_h_

#include <random>
#include <vector>

#include "star.h"
#include "protoDisk.h"
#include "math.h"


struct Body
{
  int32_t   m_id;
  double_t  m_mass;        // Total mass (M_sun)
  double_t  m_solidMass;   // Rock/ice core mass (M_sun)
  double_t  m_gasEnvelope; // Accreted H/He envelope mass (M_sun)
  double_t  m_densityCGS;
  double_t  m_radiusAU;    // radius of body in AU's
  Vec3      m_pos;
  Vec3      m_vel;
  Vec3      m_acc;
  sf::Color m_color;
  bool      m_active = true;
  bool      m_isGiant = false;
  bool      m_inHabitableZone = false;
  std::vector<Vec3> m_trail;
};


class solarSystemSim
{
public:
  solarSystemSim(star*, protoDisk*, std::mt19937*,int32_t n = 45);
  ~solarSystemSim();

  const star* getStar() const { return m_pPrimary; }
  const protoDisk* getDisk() const { return m_pDisk; }
  const std::vector<Body>& getBodies() const { return m_bodies; }
  double getSimTime() const { return m_simTime; }
  int getTidallyDisruptedCount() const { return m_tidallyDisruptedCount; }
  bool isOligarchic() const { return m_oligarchicPhase; }

  void resetSim();
  void recenterBarycenter();
  int32_t injectNucleus();
  void computeAccelerations();
  void applyDiskPhysicsAndMigration(double_t);
  void resolveCollisions();
  void step(double dtYears);
  void recordTrails();


private:
  star*             m_pPrimary;
  protoDisk*        m_pDisk;
  std::mt19937*     m_pGen;

  std::vector<Body>   m_bodies;

  int32_t    m_cntInitialNuclei;
  double_t   m_simTime = 0.0;
  double_t   m_timeSinceLastInjection = 0.0;
  int32_t    m_nextBodyId = 0;
  int32_t    m_tidallyDisruptedCount = 0;
  bool       m_oligarchicPhase = false;
};

#endif



