#ifndef _star_h_
#define _star_h_

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <random>
#include <iostream>

#include "constants.h"
#include "math.h"

struct spectralClass 
{
  std::string type;
  double freq;      // Empirical main-sequence frequency
  double minMass;     // M_sun
  double maxMass;     // M_sun
  double minTemp;
  double maxTemp;
  sf::Color color;
};


class star
{
public:
  star(std::mt19937*, bool earthlike = false);
  ~star();

  double_t mass() const { return m_mass;  }
  double_t radius() const { return m_radius;  }
  double_t temp() const { return m_temp; }
  double_t luminosity() const { return m_luminosity;}
  std::string HRType() const { return m_HRType; }
  std::pair<double_t, double_t> ISL() const { return std::pair<double_t, double_t>(m_ISLRock, m_ISLGas); }
  std::pair<double_t, double_t> HZ() const { return std::pair<double_t, double_t>(m_minHZ, m_maxHZ); }
  double_t frostline() const { return m_snowLine; }
  std::pair<double_t, double_t> OSL() const { return std::pair<double_t, double_t>(m_outerHillLimit, m_curvatureCutoff); }
  double_t density() const { return m_density; }
  sf::Color color() const { return m_color; }
  bool isEarthLike() const { return m_bEarthLike; }

  Vec3& pos() { return m_pos; }
  const Vec3& pos() const { return m_pos; }
  Vec3&  vel() { return m_vel; }
  const Vec3& vel() const { return m_vel; }
  Vec3&  acc() { return m_acc; }
  const Vec3& acc() const { return m_acc; }
  
  void acc(Vec3 v) { m_acc = v; }

  void generateStar();

private:
  std::mt19937*               m_pGen = nullptr;
  bool                        m_bEarthLike = false;
  std::vector<spectralClass>  m_classes = {};

  std::string m_HRType;
  double_t    m_mass;        // relative to the sun
  double_t    m_radius;      // radius of the star in AU
  double_t    m_temp;        // temperature in kelvin
  double_t    m_luminosity;  // relative to the sun
  double_t    m_density;     // density of the star
  double_t    m_snowLine;    // snowline in AU
  double_t    m_minHZ;
  double_t    m_maxHZ;
  double_t    m_ISLRock;     // inner system limit for rocky planets
  double_t    m_ISLGas;      // inner systme limit for gaseous planets
  double_t    m_outerHillLimit;
  double_t    m_curvatureCutoff;
  sf::Color   m_color;

  Vec3        m_pos;
  Vec3        m_vel;
  Vec3        m_acc;



};

std::ostream& operator<<(std::ostream&, star&);


#endif

