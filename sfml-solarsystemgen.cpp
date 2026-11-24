#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <format>

#include "logger.h"
#include "constants.h"
#include "math.h"
#include "star.h"
#include "protoDisk.h"
#include "solarSystemSim.h"

#ifdef __WIN32
    #include "XGetopt.h"
#else
    #include <unistd.h>
#endif

const char* fontPath = { R"(C:\Users\geohu\AppData\Local\Microsoft\Windows\Fonts\intelone-mono-font-family-regular.ttf)" };

void showVersion(const char*);
void showHelp(const char*);
bool isInteger(const char*);

int main(int argc, char** argv) 
{
  uint32_t seed = 0xFFFFFFFF;
  int32_t choice = -1;

  // get current start time and date

  auto nowSec = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
  auto now = std::chrono::zoned_time{ std::chrono::current_zone(), nowSec };
  std::string timeDate = std::format("{:%Y%m%d-%H%M.%S}", now);

  bool earthLike = false;
  uint8_t debugLevel = CLogger::level::WARNING;

  while (-1 != (choice = getopt(argc, argv, "des:vh")))
  {
    switch (choice)
    {
      case 'd':
        debugLevel--;
        break;

      case 'e':
        earthLike = !earthLike;
        break;

      case 's':
        if (isInteger(optarg))
          seed = atoi(optarg);
        break;

      case 'v':
        showVersion(argv[0]);
        return EXIT_SUCCESS;

      case '?':
        std::cout << argv[optind] << " is not a valid command line option" << std::endl;
        [[fallthrough]];

      case 'h':
        showHelp(argv[0]);
        return EXIT_SUCCESS;
    }

  }
 
  std::random_device rd;
  std::mt19937       gen;
  if (seed != 0xFFFFFFFF)    // use provided seed
    gen.seed(seed);
  else
    gen.seed(rd());         // use the random device to get a seed. 
  

  star Primary(&gen, earthLike);                                    // generate the primary star
  protoDisk disk(&Primary, &gen);                         // generate the protoPlanetary disk
  solarSystemSim ssSim(&Primary, &disk, &gen);             // set up the simulation


  sf::RenderWindow window(sf::VideoMode({ 1280, 900 }), "Solar System Simulator");
  window.setFramerateLimit(60);
  
  sf::Font font;
  bool hasFont = font.openFromFile(fontPath);
  if (!hasFont) {
    hasFont = font.openFromFile("C:/Windows/Fonts/consola.ttf");
  }

  // parameters to control the display
  double zoomPixelsPerAU = 22.0;
  double dtPerSubstep = 0.02;        // Years per physics step
  int substepsPerFrame = 80;         // Speed multiplier
  bool paused = false;
  bool showTrails = false;
  
  while (window.isOpen()) 
  {
    while (const std::optional event = window.pollEvent()) 
    {
      if (event->is<sf::Event::Closed>()) { window.close(); }
      else if (const auto* key = event->getIf<sf::Event::KeyPressed>())             // key-press events 
      {
        if (key->code == sf::Keyboard::Key::Escape) { window.close(); }                // close application
        if (key->code == sf::Keyboard::Key::R) {/* sim.reset();*/ }                    // reset simulation
        if (key->code == sf::Keyboard::Key::P) { paused = !paused; }                   // pause simulation
        if (key->code == sf::Keyboard::Key::T) { showTrails = !showTrails; }           // show orbital trails
        else if (key->code == sf::Keyboard::Key::Up)                                   // speed up simulation
        {
          substepsPerFrame = std::min(400, substepsPerFrame + 20);
        }
        else if (key->code == sf::Keyboard::Key::Down)                                 // slow down simulation
        {
          substepsPerFrame = std::max(10, substepsPerFrame - 20);
        }
      }
      else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>())   // mouse wheel events 
      {
        
        if (wheel->delta > 0) 
          zoomPixelsPerAU *= 1.15;
        else 
          zoomPixelsPerAU /= 1.15;
        zoomPixelsPerAU = std::clamp(zoomPixelsPerAU, 0.002, 1500.0);
      }
    }

    if (!paused)
    {
      for (int i = 0; i < substepsPerFrame; ++i)                      // only render results every 80^{th} simulation step
      {
        ssSim.step(dtPerSubstep);
        ssSim.recordTrails();
      }
    }

    

    window.clear(sf::Color(8, 10, 18));
    sf::Vector2f barycenterScreen(window.getSize().x * 0.5f, window.getSize().y * 0.5f);
    //auto& star = ssSim.getStar();
    //auto& disk = ssSim.getDisk();
    sf::Vector2f starScreen(barycenterScreen.x + static_cast<float_t>(Primary.pos().x * zoomPixelsPerAU),
                            barycenterScreen.y + static_cast<float_t>(Primary.pos().y * zoomPixelsPerAU));
    // 0. Draw horizontal axis and tick marks
    std::vector<sf::VertexArray> grid;
    
    sf::VertexArray line(sf::PrimitiveType::Lines, 2);
    sf::Vertex v1 = { { 0.0f, static_cast<float_t>(window.getSize().y * 0.5f) },sf::Color::Yellow };
    sf::Vertex v2 = { {static_cast<float_t>(window.getSize().x), static_cast<float_t>(window.getSize().y * 0.5f)}, sf::Color::Yellow };
    line.append(v1); line.append(v2);
    grid.push_back(line);

    for (int ndx = -200; ndx <= 200; ndx++)
    {
      float_t height = 3.0f;
      if((ndx != 0) && (ndx%5 == 0)) height=5.0f;
      if ((ndx != 0) && (ndx % 10 == 0)) height = 7.0f;
      sf::Vertex vt1 = { {static_cast<float_t>(window.getSize().x * 0.5 + ndx*zoomPixelsPerAU), static_cast<float_t>(window.getSize().y * 0.5f- height)},sf::Color::Yellow};
      sf::Vertex vt2 = { {static_cast<float_t>(window.getSize().x * 0.5 + ndx*zoomPixelsPerAU), static_cast<float_t>(window.getSize().y * 0.5f+ height)}, sf::Color::Yellow };
      line.append(vt1); line.append(vt2);
    }

    for (sf::VertexArray va : grid)
       window.draw(line);
   
    // 1. Render Gas + Dust Annuli centered on the Star
    for (int32_t ndx = disk.ringCnt() - 1; ndx >= 0; --ndx)
    {
      const auto& ring = disk.ring(ndx);
          float rOutPx = static_cast<float_t>(ring.rOuter * zoomPixelsPerAU);
          float rInPx = static_cast<float_t>(ring.rInner * zoomPixelsPerAU);
          if (rOutPx < 1.0f) continue;
          double sigmaGas = ring.gasMass / ring.areaAU2;
          double sigmaDust = ring.dustMass / ring.areaAU2;
          std::uint8_t blueAlpha = static_cast<uint8_t>(std::uint8_t(std::min(45.0, sigmaGas * 1.2e5)));
          std::uint8_t dustAlpha = static_cast<uint8_t>(std::uint8_t(std::min(70.0, sigmaDust * 4.5e6)));
          if (blueAlpha > 0 || dustAlpha > 0) {
            sf::CircleShape annulus(rOutPx);
            annulus.setOrigin({ rOutPx, rOutPx });
            annulus.setPosition(starScreen);
            annulus.setFillColor(sf::Color::Transparent);
            annulus.setOutlineThickness(-(rOutPx - rInPx));
            annulus.setOutlineColor(sf::Color(
              std::min(255, 50 + dustAlpha * 2),
              std::min(255, 95 + dustAlpha),
              std::min(255, 160 + blueAlpha),
              std::max(blueAlpha, dustAlpha)
            ));
            window.draw(annulus);
          }
    }

    // 2. Render Goldilocks (Habitable) Zone Band
    float hzOutPx = static_cast<float_t>(Primary.HZ().second * zoomPixelsPerAU);
    float hzInPx = static_cast<float_t>(Primary.HZ().first * zoomPixelsPerAU);
    if (hzOutPx > 2.0f) 
    {
      sf::CircleShape hzBand(hzOutPx);
      hzBand.setOrigin({ hzOutPx, hzOutPx });
      hzBand.setPosition(starScreen);
      hzBand.setFillColor(sf::Color::Transparent);
      hzBand.setOutlineThickness(-(hzOutPx - hzInPx));
      hzBand.setOutlineColor(sf::Color(60, 210, 110, 45));
      window.draw(hzBand);
    }

    // 3. Render Snow Line Boundary
    sf::CircleShape snowRing(static_cast<float_t>(Primary.frostline() * zoomPixelsPerAU));
    snowRing.setOrigin({ snowRing.getRadius(), snowRing.getRadius() });
    snowRing.setPosition(starScreen);
    snowRing.setFillColor(sf::Color::Transparent);
    snowRing.setOutlineThickness(1.0f);
    snowRing.setOutlineColor(sf::Color(100, 200, 255, 80));
    window.draw(snowRing);
    
    // 4. Render Inner Roche Limit
    float rocheGasPx = static_cast<float_t>(Primary.ISL().first * zoomPixelsPerAU);
    if (rocheGasPx > 1.5f) 
    {
      sf::CircleShape rocheRing(rocheGasPx);
      rocheRing.setOrigin({ rocheGasPx, rocheGasPx });
      rocheRing.setPosition(starScreen);
      rocheRing.setFillColor(sf::Color::Transparent);
      rocheRing.setOutlineThickness(1.2f);
      rocheRing.setOutlineColor(sf::Color(255, 75, 75, 160));
      window.draw(rocheRing);
    }

    // 5. Render Outer Curvature / Hill Limit
    double outerLimitAU = std::min(Primary.OSL().first, Primary.OSL().second);
    float outerPx = static_cast<float_t>(outerLimitAU * zoomPixelsPerAU);
    if (outerPx > 10.0f && outerPx < 5000.0f) 
    {
      sf::CircleShape outerRing(outerPx);
      outerRing.setOrigin({ outerPx, outerPx });
      outerRing.setPosition(starScreen);
      outerRing.setFillColor(sf::Color::Transparent);
      outerRing.setOutlineThickness(1.5f);
      outerRing.setOutlineColor(sf::Color(180, 100, 255, 110));
      window.draw(outerRing);
    }

    // 6. Render Central Star
    float starVisualRadius = std::max(5.0f, static_cast<float_t>(Primary.radius() * zoomPixelsPerAU * 18.0));
    sf::CircleShape starShape(starVisualRadius);
    starShape.setOrigin({ starVisualRadius, starVisualRadius });
    starShape.setPosition(starScreen);
    starShape.setFillColor(Primary.color());
    window.draw(starShape);

    // 7. Render Barycenter Crosshair at (0, 0) so Stellar Reflex Wobble is Visible
    sf::RectangleShape crossH({ 10.f, 1.f });
    crossH.setOrigin({ 5.f, 0.5f });
    crossH.setPosition(barycenterScreen);
    crossH.setFillColor(sf::Color(255, 255, 255, 140));
    sf::RectangleShape crossV({ 1.f, 10.f });
    crossV.setOrigin({ 0.5f, 5.f });
    crossV.setPosition(barycenterScreen);
    crossV.setFillColor(sf::Color(255, 255, 255, 140));
    window.draw(crossH);
    window.draw(crossV);
    
    // 8. Render Planetary Nuclei & Planets
    int habitablePlanets = 0;
    for (const auto& b : ssSim.getBodies()) 
    {
      if (showTrails && b.m_trail.size() > 1)
      {
        sf::VertexArray trailVA(sf::PrimitiveType::LineStrip, b.m_trail.size() + 1);
        for (size_t idx = 0; idx < b.m_trail.size(); ++idx)
        {
          trailVA[idx].position = {
            barycenterScreen.x + static_cast<float_t>(b.m_trail[idx].x * zoomPixelsPerAU),
            barycenterScreen.y + static_cast<float_t>(b.m_trail[idx].y * zoomPixelsPerAU)
          };
          sf::Color c = b.m_color;
          c.a = static_cast<std::uint8_t>(140 * idx / b.m_trail.size());
          trailVA[idx].color = c;
        }
        // Connect final vertex smoothly to the planet's current position
        trailVA[b.m_trail.size()].position = {
          barycenterScreen.x + static_cast<float_t>(b.m_pos.x * zoomPixelsPerAU),
          barycenterScreen.y + static_cast<float_t>(b.m_pos.y * zoomPixelsPerAU)
        };
        sf::Color headCol = b.m_color;
        headCol.a = 160;
        trailVA[b.m_trail.size()].color = headCol;

        window.draw(trailVA);
      }

      double massEarth = b.m_mass / M_EARTH;
      if (b.m_inHabitableZone && !b.m_isGiant && massEarth > 0.2) habitablePlanets++;
      double_t rOrbit = std::max(0.05, (b.m_pos - Primary.pos()).mag());
      double_t sinInc = std::clamp(b.m_pos.z / rOrbit, -0.5, 0.5);
      float visualR = std::clamp(2.0f + static_cast<float_t>(std::log10(massEarth + 1.0) * 3.5), 2.0f, 14.0f);
      visualR *= static_cast<float_t>(1.0 + sinInc*0.4);
      sf::CircleShape planet(visualR);
      planet.setOrigin({ visualR, visualR });
      planet.setPosition({
      barycenterScreen.x + static_cast<float_t>(b.m_pos.x * zoomPixelsPerAU),
      barycenterScreen.y + static_cast<float_t>(b.m_pos.y * zoomPixelsPerAU)});
      planet.setFillColor(b.m_color);
      if (b.m_inHabitableZone && !b.m_isGiant && massEarth > 0.2) {
        planet.setOutlineThickness(1.5f);
        planet.setOutlineColor(sf::Color(80, 255, 130));
      }

      

      window.draw(planet);
    }
    
    // 9. Render Telemetry HUD
    if (hasFont) 
    {
      double remGasEarth = disk.getTotalRemainingGas() / M_EARTH;
      double remDustEarth = disk.getTotalRemainingDust() / M_EARTH;
      double initGasEarth = disk.initialGasMass() / M_EARTH;
      double initDustEarth = disk.initialDustMass() / M_EARTH;
//      double starWobbleAU = star.pos.mag();
      // Convert AU/yr to m/s (1 AU/yr = 4740.47 m/s)
      double starSpeedMS = Primary.vel().mag() * 4740.47;
      std::ostringstream ss;
      ss << std::fixed << std::setprecision(2);
      ss << "Simulation started at: " << timeDate<< " with Zoom factor : " << zoomPixelsPerAU << "\n";
      ss << "Star: " << Primary.HRType() << "-type (" << Primary.mass() << " M_sun, " << static_cast<float_t>(Primary.temp()) << " K)\n";
      ss << (Primary.isEarthLike() ? "  [EARTH-LIKE PRIMARY: YES]" : "  [EARTH-LIKE PRIMARY: NO]") << "\n";
      ss << std::setprecision(5);
//      ss << "Barycenter Offset: " << starWobbleAU << " AU  |  Stellar Reflex Speed: "
//        << std::setprecision(2) << starSpeedMS << " m/s\n";
//      ss << std::setprecision(3);
//      ss << "Roche Limit: " << star.rocheLimitRockAU << " AU (Rock) / " << star.rocheLimitGasAU << " AU (Gas)"
//        << "  |  Goldilocks Zone: " << star.hzInnerAU << " - " << star.hzOuterAU << " AU\n";
//      ss << std::setprecision(1);
      ss << "Gas Reservoir:  " << remGasEarth << " / " << initGasEarth << " M_earth (" << (disk.gasDissipation() * 100.0) << "%)\n";
      ss << "Dust Reservoir: " << remDustEarth << " / " << initDustEarth << " M_earth\n";
      ss << std::setprecision(2);
      ss << "Time: " << (ssSim.getSimTime() / 1e6) << " Myr  |  Phase: "
         << (ssSim.isOligarchic() ? "OLIGARCHIC GIANT IMPACTS" : "GAS/DUST ACCRETION & MIGRATION") << "\n";
      ss << "Active Bodies: " << ssSim.getBodies().size() << "  |  Tidally Shredded: " << ssSim.getTidallyDisruptedCount()
         << "  |  HZ Worlds: " << habitablePlanets << "\n";
//      ss << "[R] New Star   [Scroll] Zoom (Zoom in close to see Star orbit the Barycenter +)   [Up/Down] Speed";
      if (paused)
        ss << "[PAUSED]\n";
      sf::Text hud(font, ss.str(), 14);
      hud.setPosition({ 16.f, 16.f });
      hud.setFillColor(sf::Color(220, 230, 245));
      window.draw(hud);
    }
    window.display();
  }

  return 0;
}

void showVersion(const char* name)
{
  std::cout << name << " solar system generator, version: " << MAJOR << "." << MINOR << "." << PATCH << std::endl;
}


void showHelp(const char* name)
{
  std::cout << name << "solar system generator" << std::endl;
  std::cout << "usage: " << std::endl;
  std::cout << name << " [options]\n" << std::endl;
  std::cout << "options:                                                        " << std::endl;
  std::cout << "d               increate the verbosity of debug messages can be " << std::endl;
  std::cout << "                used multiple times to keep increasing verbosity" << std::endl;
  std::cout << "e               toggles the flag for an earth-like environemnt  " << std::endl;
  std::cout << "                default value is 'false'                        " << std::endl;
  std::cout << "s <num>         use \"num\" as the seed for the pseudo-random   " << std::endl;
  std::cout << "                generator.  default is a system generated value " << std::endl;
  std::cout << "v               shows program version infomation and exits      " << std::endl;
  std::cout << "h               shows a brief help screen (this screen) and exit" << std::endl;
}

bool isInteger(const char* bp)
{
  const char* ep = bp + strlen(bp);
  return ep == std::find_if(bp, ep, [](char ch) {return !std::isdigit(ch); });
}
