#include "solarSystemSim.h"
#include "math.h"
#include "star.h"
#include "protoDisk.h"

#include <vector>
#include <random>

solarSystemSim::solarSystemSim(star* s, protoDisk* d, std::mt19937* pgen, int32_t initNuclei) : m_pPrimary(s), m_pDisk(d), m_pGen(pgen), m_cntInitialNuclei(initNuclei)
{
  resetSim();
}

solarSystemSim::~solarSystemSim()
{ }

void solarSystemSim::resetSim()
{
  m_bodies.clear();

  m_simTime = 0.0;
  m_timeSinceLastInjection = 0.0;
  m_nextBodyId = 1;
  m_tidallyDisruptedCount = 0;
  m_oligarchicPhase = false;
  
  //m_generateStar();
  //m_disk.init(star, 40);
  
  for (int32_t cnt = 0; cnt < m_cntInitialNuclei; ++cnt) 
  {
      injectNucleus();
  }
  recenterBarycenter();
}

void solarSystemSim::recenterBarycenter()
{
  double totalMass = m_pPrimary->mass();
  Vec3 comPos = m_pPrimary->pos() * m_pPrimary->mass();
  Vec3 comVel = m_pPrimary->vel() * m_pPrimary->mass();

  for (const auto& b : m_bodies) 
  {
    if (!b.m_active) continue;
    totalMass += b.m_mass;
    comPos += b.m_pos * b.m_mass;
    comVel += b.m_vel * b.m_mass;
  }

  comPos = comPos * (1.0 / totalMass);
  comVel = comVel * (1.0 / totalMass);

  m_pPrimary->pos() -= comPos;
  m_pPrimary->vel() -= comVel;
  for (auto& b : m_bodies) 
  {
    if (!b.m_active) continue;
    b.m_pos -= comPos;
    b.m_vel -= comVel;
  }
}

int solarSystemSim::injectNucleus()
{
  if (m_pDisk->getTotalRemainingDust() < 0.5 * M_EARTH || m_bodies.size() > 140) return -1;

  std::uniform_real_distribution<double_t> rDist(m_pDisk->edges().first * 1.4, m_pDisk->edges().second * 0.85);
  std::uniform_real_distribution<double_t> angleDist(0.0, 2.0 * PI);
  std::uniform_real_distribution<double_t> massDist(0.005 * M_EARTH, 0.06 * M_EARTH);
  std::normal_distribution<double_t> incDist(0.0, 0.035);
  std::normal_distribution<double_t> eccPerturb(1.0, 0.03);

  double_t r = rDist(*m_pGen);
  int ringIdx = m_pDisk->getRingIndex(r);
  if (ringIdx < 0) return -2;

  double_t desiredMass = massDist(*m_pGen);
  if (r > m_pPrimary->frostline()) desiredMass *= 2.5;

  double_t actualMass = std::min(desiredMass, m_pDisk->ring(ringIdx).dustMass * 0.5);
  if (actualMass < 0.001 * M_EARTH) return -3;
  //m_disk.ring(ringIdx).dustMass -= actualMass;  
  m_pDisk->ring(ringIdx, m_pDisk->ring(ringIdx).dustMass - actualMass);

  double_t theta = angleDist(*m_pGen);      // randomly choose RAAN angle
  double_t inc = incDist(*m_pGen);          // randomly choosen inclination angle
  
  Body b;
  b.m_id = m_nextBodyId++;
  b.m_mass = actualMass;
  b.m_solidMass = actualMass;
  b.m_gasEnvelope = 0.0;
  b.m_densityCGS = (r > m_pPrimary->frostline()) ? 2.0 : 5.51;
  b.m_radiusAU = 0.0025 * std::cbrt(b.m_mass / M_EARTH);
  // Position relative to the star's current position
  Vec3 relPos = {r * std::cos(theta) * std::cos(inc), r * std::sin(theta) * std::cos(inc), r * std::sin(inc)};
  b.m_pos = m_pPrimary->pos() + relPos;
  // Keplerian reduced-mass circular velocity: sqrt(G * (M_star + m_body) / r)
  double_t vCirc = std::sqrt(G * (m_pPrimary->mass() + b.m_mass) / r) * eccPerturb(*m_pGen);

  Vec3 relVel = {-vCirc * std::sin(theta) * std::cos(inc), vCirc * std::cos(theta) * std::cos(inc), vCirc * std::sin(inc) * 0.5 };
  b.m_vel = m_pPrimary->vel() + relVel;
  // Apply equal-and-opposite momentum recoil to the star so barycenter velocity remains zero!
  m_pPrimary->vel() -= relVel * (b.m_mass / m_pPrimary->mass());
  b.m_color = (r > m_pPrimary->frostline()) ? sf::Color(130, 200, 255) : sf::Color(210, 160, 120);
  m_bodies.push_back(b);

  return b.m_id;
}


void solarSystemSim::computeAccelerations()
{
  const double softeningSq = 1e-6;      // minor fudge factor to avoid division by zero

  m_pPrimary->acc({0.0, 0.0, 0.0});       // Reset primary star acceleration
  
  // 1. Mutual gravitational interaction between the Primary Star and every Body (Newton's 3rd Law)
  for (auto& b : m_bodies) 
   {
    // 0. allow primary to accrete planets that get to close
    double_t minSafeOrbit = std::max<double_t>(m_pPrimary->ISL().second, 0.035);

    if (!b.m_active) continue;
    Vec3 diff = m_pPrimary->pos() - b.m_pos;             // Vector from body to star
    double rSq = diff.magSq() + softeningSq;

    if (rSq < minSafeOrbit)
    {
      double_t newStarMass = m_pPrimary->mass() + b.m_mass;
      m_pPrimary->vel() = (m_pPrimary->vel() * m_pPrimary->mass() + b.m_vel * b.m_mass) * (1.0 / newStarMass);
      b.m_active = false;
      std::cout << "[" << m_simTime << "] body" << b.m_id << " engulfed by primary" << std::endl;
      m_tidallyDisruptedCount++;
      recenterBarycenter();
      continue;
    }


    double invR3 = 1.0 / (rSq * std::sqrt(rSq));
    b.m_acc = diff * (G * m_pPrimary->mass() * invR3);   // Force on body due to star: +G * M_star * diff / r^3
    m_pPrimary->acc() -= diff * (G * b.m_mass * invR3);  // force on star due to body: -G * m_body * diff / r^3
  }
   
  // 2. Pairwise N-body interactions between planetary bodies
  for (size_t i = 0; i < m_bodies.size(); ++i) 
   {
    if (!m_bodies[i].m_active) continue;
    for (size_t j = i + 1; j < m_bodies.size(); ++j) 
    {
      if (!m_bodies[j].m_active) continue;

      double_t minSoft = m_bodies[i].m_radiusAU + m_bodies[j].m_radiusAU;
      Vec3 diff = m_bodies[j].m_pos - m_bodies[i].m_pos;        // vector from i^{th} body to j^{th} body
      double distSq = diff.magSq() + minSoft*minSoft;
      double invDist3 = 1.0 / (distSq * std::sqrt(distSq));
      m_bodies[i].m_acc += diff * (G * m_bodies[j].m_mass * invDist3);   // acceleration on i^{th} body due to j^{th} body
      m_bodies[j].m_acc += diff * (-G * m_bodies[i].m_mass * invDist3);  // acceleration on j^{th} body due to i^{th} body
    }
  }
}


void solarSystemSim::applyDiskPhysicsAndMigration(double_t dt)
{
  // Global gas photoevaporation decay
  double prevDissipation = m_pDisk->gasDissipation();
  m_pDisk->gasDissipation(std::exp(-m_simTime / m_pDisk->lifetime()));
  double decayStep = (prevDissipation > 0.0) ? (m_pDisk->gasDissipation() / prevDissipation) : 0.0;
  for (auto& ring : m_pDisk->rings())
  {
    ring.gasMass *= decayStep;
  }

  if (m_pDisk->gasDissipation() < 0.12 && !m_oligarchicPhase)
  {
    m_oligarchicPhase = true;
  }
  for (auto& b : m_bodies) 
  {
    if (!b.m_active) continue;

    Vec3 relPos = b.m_pos - m_pPrimary->pos();
    Vec3 relVel = b.m_vel - m_pPrimary->vel();

    double r = relPos.mag();

    // Check Inner Roche Limit
    double bodyRocheLimit = 2.44 * m_pPrimary->radius() * std::cbrt(m_pPrimary->density() / b.m_densityCGS);
    if (r < bodyRocheLimit) 
    {
      b.m_active = false;
      std::cout << "[" << m_simTime <<"] removed body: " << b.m_id << " due to tidally disrupted." << std::endl;
      m_tidallyDisruptedCount++;
      continue;
    }
    // Check Outer Curvature / Hill Escape Limit
    double effectiveOuterLimit = std::min(m_pPrimary->OSL().first, m_pPrimary->OSL().second);
    if (r > effectiveOuterLimit) 
    {
      b.m_active = false;
      std::cout << "[" << m_simTime << "] removed body: " << b.m_id << " due to escaping." << std::endl;
      recenterBarycenter();
      continue;
    }

    b.m_inHabitableZone = (r >= m_pPrimary->HZ().first && r <= m_pPrimary->HZ().second);
    int ringIdx = m_pDisk->getRingIndex(r);
    if (ringIdx < 0) continue;
    DiskRing& ring = m_pDisk->ring(ringIdx);
    double sigmaDust = ring.dustMass / ring.areaAU2;
    double sigmaGas = ring.gasMass / ring.areaAU2;
    double omega = std::sqrt(G * m_pPrimary->mass() / (r * r * r));
    double rHill = r * std::cbrt(b.m_mass / (3.0 * m_pPrimary->mass()));

    // 1. Solid (Dust/Pebble) Accretion: consumes local dust reservoir
    if (ring.dustMass > 0.0) 
    {
      double dustAccretion = std::min(ring.dustMass, 0.85 * sigmaDust * rHill * rHill * omega * dt);
      b.m_solidMass += dustAccretion;
      b.m_mass += dustAccretion;
      ring.dustMass -= dustAccretion;
    }

    // 2. Gas Envelope Accretion: caps at ~3 M_Jupiter as planet opens a disk gap
    const double maxGasEnvelope = 3.0 * M_JUPITER;
    if (b.m_solidMass > 10.0 * M_EARTH && ring.gasMass > 0.0 && m_pDisk->gasDissipation() > 0.1 && b.m_gasEnvelope < maxGasEnvelope)
    {
      double gapTaper = 1.0 - (b.m_gasEnvelope / maxGasEnvelope);
      double gasAccretion = std::min({ring.gasMass,maxGasEnvelope - b.m_gasEnvelope,0.04 * sigmaGas * rHill * rHill * omega * dt * gapTaper});
      b.m_gasEnvelope += gasAccretion;
      b.m_mass += gasAccretion;
      ring.gasMass -= gasAccretion;
      if (b.m_gasEnvelope > 5.0 * M_EARTH)
      {
        b.m_isGiant = true;
        b.m_densityCGS = 1.33;
        b.m_color = sf::Color(235, 190, 120);
      }
    }

    b.m_radiusAU = 0.0025 * std::cbrt(b.m_mass / M_EARTH);

    // 3. Type I & Type II Migration (driven strictly by local GAS surface density)
    if (sigmaGas > 0.0 && r > m_pDisk->edges().first*1.15) 
    {
      double massRatio = b.m_mass / m_pPrimary->mass();
      double gasDiskMassRatio = (sigmaGas * r * r) / m_pPrimary->mass();
      double migrationRate = (b.m_mass < 50.0 * M_EARTH)
        ? 2.5 * massRatio * gasDiskMassRatio * omega
        : 0.00015 * omega * m_pDisk->gasDissipation();

      double rXY = std::hypot(relPos.x, relPos.y);
      if (rXY > 1E-8)
      {
        Vec3 rHat = { relPos.x / rXY, relPos.y / rXY, 0.0 };
        Vec3 tHat = { -rHat.y, rHat.x, 0.0 };
        double vCirc = std::sqrt(G * m_pPrimary->mass() / r);
        Vec3 vIdeal = tHat * vCirc;
        Vec3 vDiff = b.m_vel - vIdeal;

        double_t maxStepFrac = std::min(0.01, migrationRate * dt);
        b.m_vel -= tHat * (maxStepFrac * vCirc * dt);
        double damping = 4.0 * gasDiskMassRatio * omega * dt;
        b.m_vel -= vDiff * std::min(0.5, damping);
        b.m_pos.z *= std::max(0.0, 1.0 - damping * 2.0);
      }
    }
  }
}


void solarSystemSim::resolveCollisions()
{
  double collisionBoost = m_oligarchicPhase ? 2.2 : 1.0;
  for (size_t i = 0; i < m_bodies.size(); ++i)
  {
    if (!m_bodies[i].m_active) continue;
    for (size_t j = i + 1; j < m_bodies.size(); ++j)
    {
      if (!m_bodies[j].m_active) continue;
      Vec3 diff = m_bodies[j].m_pos - m_bodies[i].m_pos;
      double dist = diff.mag();
      double minDist = (m_bodies[i].m_radiusAU + m_bodies[j].m_radiusAU) * collisionBoost;

      if (dist < minDist && dist > 1e-12)
      {
        Vec3 normal = diff * (1.0 / dist); // Unit vector from i to j
        Vec3 relVel = m_bodies[j].m_vel - m_bodies[i].m_vel;
        double velAlongNormal = relVel.x * normal.x + relVel.y * normal.y + relVel.z * normal.z;

        // Only process if bodies are moving toward each other
        if (velAlongNormal > 0.0) continue;

        double relSpeed = relVel.mag();
        double sinImpact = (relSpeed > 1e-12) ? std::abs(velAlongNormal) / relSpeed : 1.0; // cos(theta)
        double vEscape = std::sqrt(2.0 * G * (m_bodies[i].m_mass + m_bodies[j].m_mass) / minDist);

        // Glancing blow (impact angle > ~50 deg, i.e., cos(theta) < 0.6) & high relative speed -> Elastic/Restitution Bounce
        if (sinImpact < 0.6 && relSpeed > 1.2 * vEscape)
        {
          std::cout << "[" << m_simTime << "] glancing blow between bodies [" << i << "] and [" << j << "]\n";
          double restitution = 0.85; // 1.0 = pure elastic, <1.0 = slight collisional energy loss
          double invMass1 = 1.0 / m_bodies[i].m_mass;
          double invMass2 = 1.0 / m_bodies[j].m_mass;
          double impulseMag = -(1.0 + restitution) * velAlongNormal / (invMass1 + invMass2);

          Vec3 impulse = normal * impulseMag;
          m_bodies[i].m_vel -= impulse * invMass1;
          m_bodies[j].m_vel += impulse * invMass2;

          // Separate overlapping spheres slightly so they don't re-collide next substep
          double overlap = (minDist - dist) * 0.51;
          m_bodies[i].m_pos -= normal * overlap;
          m_bodies[j].m_pos += normal * overlap;
          continue;
        }

        // Otherwise: Inelastic Accretion Merger
        Body& primary = (m_bodies[i].m_mass >= m_bodies[j].m_mass) ? m_bodies[i] : m_bodies[j];
        Body& secondary = (m_bodies[i].m_mass >= m_bodies[j].m_mass) ? m_bodies[j] : m_bodies[i];
        double primOrigMass = primary.m_mass;
        double totalMass = primary.m_mass + secondary.m_mass;
        primary.m_pos = (primary.m_pos * primary.m_mass + secondary.m_pos * secondary.m_mass) * (1.0 / totalMass);
        primary.m_vel = (primary.m_vel * primary.m_mass + secondary.m_vel * secondary.m_mass) * (1.0 / totalMass);
        primary.m_mass = totalMass;
        primary.m_solidMass += secondary.m_solidMass;
        primary.m_gasEnvelope += secondary.m_gasEnvelope;
        primary.m_radiusAU = 0.0025 * std::cbrt(primary.m_mass / M_EARTH);
        if (primary.m_gasEnvelope > 5.0 * M_EARTH)
        {
          primary.m_isGiant = true;
          primary.m_densityCGS = 1.33;
          primary.m_color = sf::Color(240, 185, 110);
        }
        else if (primary.m_mass > 0.5 * M_EARTH)
        {
          primary.m_color = sf::Color(110, 180, 240);
        }
        secondary.m_active = false;
        std::cout << "[" << m_simTime << "] removed body: " << secondary.m_id << " due to a collision with: " << primary.m_id << std::endl;
      }
    }
  }
 
  
  m_bodies.erase(std::remove_if(m_bodies.begin(), m_bodies.end(), [](const Body& b) { return !b.m_active; }), m_bodies.end());
}


void solarSystemSim::step(double dtYears)
{
  computeAccelerations();

  // integrate both the primary star and all bodies
  m_pPrimary->pos() += m_pPrimary->vel() * dtYears + m_pPrimary->acc() * (0.5 * dtYears * dtYears);
  for (auto& b : m_bodies) 
  {
    b.m_pos += b.m_vel * dtYears + b.m_acc * (0.5 * dtYears * dtYears);
  }
  
  Vec3 oldStarAcc = m_pPrimary->acc();
  std::vector<Vec3> oldAcc(m_bodies.size());
  for (size_t i = 0; i < m_bodies.size(); ++i) 
    oldAcc[i] = m_bodies[i].m_acc;
  computeAccelerations();

  // Velocity Verlet velocity step for both star and bodies
  m_pPrimary->vel() += (oldStarAcc + m_pPrimary->acc()) * (0.5 * dtYears);
  for (size_t i = 0; i < m_bodies.size(); ++i) 
  {
    m_bodies[i].m_vel += (oldAcc[i] + m_bodies[i].m_acc) * (0.5 * dtYears);
  }
  
  applyDiskPhysicsAndMigration(dtYears);
  resolveCollisions();
  
  m_simTime += dtYears;
  m_timeSinceLastInjection += dtYears;
  if (m_timeSinceLastInjection >= 25000.0 && !m_oligarchicPhase)
  {
    const char* reasons[] = { "unknown reason       ", "not enough gas       ", "no ring at choosen AU", "mass to small        " };
  
    int32_t id = injectNucleus();
    if (id < 0)
      std::cout << "[" << m_simTime << "] failed to inject a new nucleus." << reasons[abs(id)] << std::endl;
    else
      std::cout << "[" << m_simTime << "] injecting a new nucleus.  object ID: "<< id << std::endl;
    m_timeSinceLastInjection = 0.0;
  }

  recenterBarycenter();
}

void solarSystemSim::recordTrails()
{
  for (auto& b : m_bodies)
  {
    if (!b.m_active) continue;
    Vec3 relPos = b.m_pos - m_pPrimary->pos();
    double r = relPos.mag();
    double stepThreshold = std::max(0.01, 0.08 * r);

    if (b.m_trail.empty() || (b.m_pos - b.m_trail.back()).mag() >= stepThreshold)
    {
      b.m_trail.push_back(b.m_pos);
      if (b.m_trail.size() > 75)
        b.m_trail.erase(b.m_trail.begin());
    }
  }
}