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
#include <fstream>
#include <bit>

#include "logger.h"
#include "constants.h"
#include "math.h"
#include "star.h"
#include "protoDisk.h"
#include "solarSystemSim.h"
#include "ephemeris.h"

#ifdef __WIN32
    #include "XGetopt.h"
#else
    #include <unistd.h>
#endif

struct PlanetTelemetry {
    double_t a = 0.0;              // semi-major axis, AU
    double_t e = 0.0;              // orbital eccentricity
    double_t i = 0.0;              // orbital inclination, degrees
    double_t t = 0.0;              // planetary temperature, kelvin degrees
};

const char* fontPath = { R"(C:\Users\geohu\AppData\Local\Microsoft\Windows\Fonts\intelone-mono-font-family-regular.ttf)" };

void showVersion(const char*);
void showHelp(const char*);
bool isInteger(const char*);
PlanetTelemetry computeTelemetry(const Body& b, const star& primary);
PlanetEphemeris computePlanetEphemeris(const Body& b, const star& primary);
bool writeBinaryEphemeris(const std::string& filename, const std::string& timeDate, bool earthLike, uint32_t seed, const star& primary, const solarSystemSim& sim);

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
  if (seed == 0xFFFFFFFF)    // use random seed
    seed = rd();
  
  gen.seed(seed);            // seed the random-number generator
  

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
  int32_t selectedBodyId = -1;
  bool paused = false;
  bool showTrails = false;
  bool sideView = false;
  
  while (window.isOpen()) 
  {
    while (const std::optional event = window.pollEvent()) 
    {
      if (event->is<sf::Event::Closed>()) { window.close(); }
      else if (const auto* key = event->getIf<sf::Event::KeyPressed>())             // key-press events 
      {
        if (key->code == sf::Keyboard::Key::Escape) { window.close(); }                // close application
        if (key->code == sf::Keyboard::Key::R)                                      // reset simulation
        {
            seed = rd();
            gen.seed(seed);

            Primary.generateStar();
            disk.genDisk();
            ssSim.resetSim();

            selectedBodyId = -1;
        }                    
        if (key->code == sf::Keyboard::Key::P) { paused = !paused; }                   // pause simulation
        if (key->code == sf::Keyboard::Key::T) { showTrails = !showTrails; }           // show orbital trails
        if (key->code == sf::Keyboard::Key::V) { sideView = !sideView; }
        else if (key->code == sf::Keyboard::Key::Up)                                   // speed up simulation
        {
          substepsPerFrame = std::min(/*400*/800, substepsPerFrame + 20);
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
      else if (const auto* resized = event->getIf<sf::Event::Resized>())            // window resize events
      {
          sf::FloatRect visibleArea({0.f,0.f}, {static_cast<float_t>(resized->size.x), static_cast<float_t>(resized->size.y) });
          window.setView(sf::View(visibleArea));
      }
      else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>())   // mouse click events
      {
        if (mouse->button == sf::Mouse::Button::Left)                                   // lett mouse-button clicks
        {
            sf::Vector2f clickPos(static_cast<float>(mouse->position.x), static_cast<float>(mouse->position.y));
            sf::Vector2f barycenter(window.getSize().x * 0.5f, window.getSize().y * 0.5f);

            int32_t closestId = -1;
            float bestDistPx = 18.0f; // Click snap radius in pixels

            for (const auto& b : ssSim.getBodies())
            {
                if (!b.m_active) continue;
                double_t worldY = sideView ? b.m_pos.z : b.m_pos.y;
                sf::Vector2f pScreen(
                    barycenter.x + static_cast<float>(b.m_pos.x * zoomPixelsPerAU),
                    barycenter.y + static_cast<float>(worldY * zoomPixelsPerAU)
                );
                float dx = clickPos.x - pScreen.x;
                float dy = clickPos.y - pScreen.y;
                float dist = std::hypot(dx, dy);

                if (dist < bestDistPx)
                {
                    bestDistPx = dist;
                    closestId = b.m_id;
                }
            }
            selectedBodyId = closestId; // Clicking empty space clears selection (-1)
        }
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
    sf::Vector2f starScreen(barycenterScreen.x + static_cast<float_t>(Primary.pos().x * zoomPixelsPerAU),
                            barycenterScreen.y + static_cast<float_t>((sideView ? Primary.pos().z :Primary.pos().y) * zoomPixelsPerAU));
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
    if (!sideView)                                                       // top-down rendering 
    {
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
    }
    else                                                                 // edge-on side view rendering 
    {
      // Helper lambda to draw symmetric left/right horizontal bands in the X-Z plane
      auto drawSideBand = [&](float rInPx, float rOutPx, float halfHeightPx, sf::Color col) {
        float width = rOutPx - rInPx;
        if (width <= 0.5f) return;
      
        sf::RectangleShape rightBand({ width, halfHeightPx * 2.0f });
        rightBand.setPosition({ starScreen.x + rInPx, starScreen.y - halfHeightPx });
        rightBand.setFillColor(col);
        window.draw(rightBand);
      
        sf::RectangleShape leftBand({ width, halfHeightPx * 2.0f });
        leftBand.setPosition({ starScreen.x - rOutPx, starScreen.y - halfHeightPx });
        leftBand.setFillColor(col);
        window.draw(leftBand);
        };
      
      // Helper lambda to draw symmetric left/right vertical boundary markers
      auto drawSideMarker = [&](float rPx, float halfHeightPx, float thickness, sf::Color col) {
        if (rPx < 1.5f || rPx > 5000.0f) return;
        for (float sign : { -1.0f, 1.0f }) {
          sf::RectangleShape marker({ thickness, halfHeightPx * 2.0f });
          marker.setOrigin({ thickness * 0.5f, halfHeightPx });
          marker.setPosition({ starScreen.x + sign * rPx, starScreen.y });
          marker.setFillColor(col);
          window.draw(marker);
        }
        };
      
      // 1. Gas + Dust Disk Cross-Section (flared scale height h/r ~ 0.05)
      for (int32_t ndx = disk.ringCnt() - 1; ndx >= 0; --ndx)
      {
        const auto& ring = disk.ring(ndx);
        float rOutPx = static_cast<float>(ring.rOuter * zoomPixelsPerAU);
        float rInPx = static_cast<float>(ring.rInner * zoomPixelsPerAU);
        if (rOutPx < 1.0f) continue;
      
        double sigmaGas = ring.gasMass / ring.areaAU2;
        double sigmaDust = ring.dustMass / ring.areaAU2;
        std::uint8_t blueAlpha = static_cast<std::uint8_t>(std::min(45.0, sigmaGas * 1.2e5));
        std::uint8_t dustAlpha = static_cast<std::uint8_t>(std::min(70.0, sigmaDust * 4.5e6));
      
        if (blueAlpha > 0 || dustAlpha > 0) {
          float halfThick = std::max(2.0f, rOutPx * 0.05f);
          sf::Color col(
            std::min(255, 50 + dustAlpha * 2),
            std::min(255, 95 + dustAlpha),
            std::min(255, 160 + blueAlpha),
            std::max(blueAlpha, dustAlpha)
          );
          drawSideBand(rInPx, rOutPx, halfThick, col);
        }
      }
      
      // 2. Goldilocks (Habitable) Zone Band
      float hzOutPx = static_cast<float>(Primary.HZ().second * zoomPixelsPerAU);
      float hzInPx = static_cast<float>(Primary.HZ().first * zoomPixelsPerAU);
      if (hzOutPx > 2.0f) {
        drawSideBand(hzInPx, hzOutPx, 24.0f, sf::Color(60, 210, 110, 45));
      }
      
      // 3. Snow Line Boundary
      drawSideMarker(static_cast<float>(Primary.frostline() * zoomPixelsPerAU), 28.0f, 1.5f, sf::Color(100, 200, 255, 110));
      
      // 4. Inner Roche Limit
      drawSideMarker(static_cast<float>(Primary.ISL().first * zoomPixelsPerAU), 20.0f, 1.5f, sf::Color(255, 75, 75, 170));
      
      // 5. Outer Curvature / Hill Limit
      double outerLimitAU = std::min(Primary.OSL().first, Primary.OSL().second);
      drawSideMarker(static_cast<float>(outerLimitAU * zoomPixelsPerAU), 35.0f, 1.5f, sf::Color(180, 100, 255, 120));
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
          double trialY = sideView ? b.m_trail[idx].z : b.m_trail[idx].y;
          trailVA[idx].position = {
            barycenterScreen.x + static_cast<float_t>(b.m_trail[idx].x * zoomPixelsPerAU),
            barycenterScreen.y + static_cast<float_t>(trialY * zoomPixelsPerAU)
          };

          sf::Color c = b.m_color;
          c.a = static_cast<std::uint8_t>(140 * idx / b.m_trail.size());
          trailVA[idx].color = c;
        }
        // Connect final vertex smoothly to the planet's current position
        double headY = sideView ? b.m_pos.z : b.m_pos.y;
        trailVA[b.m_trail.size()].position = {
          barycenterScreen.x + static_cast<float>(b.m_pos.x * zoomPixelsPerAU),
          barycenterScreen.y + static_cast<float>(headY * zoomPixelsPerAU)
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
      double headY = sideView ? b.m_pos.z : b.m_pos.y;
      double depthCoord = sideView ? b.m_pos.y : b.m_pos.z;
      double sinDepth = std::clamp(depthCoord / rOrbit, -0.5, 0.5);
      float visualR = std::clamp(2.0f + static_cast<float>(std::log10(massEarth + 1.0) * 3.5), 2.0f, 14.0f);
      visualR *= static_cast<float>(1.0 + sinDepth * 0.4);
      sf::CircleShape planet(visualR);
      planet.setOrigin({ visualR, visualR });
      planet.setPosition({
      barycenterScreen.x + static_cast<float_t>(b.m_pos.x * zoomPixelsPerAU),
      barycenterScreen.y + static_cast<float_t>(headY * zoomPixelsPerAU)});

      planet.setFillColor(b.m_color);
      if (b.m_inHabitableZone && !b.m_isGiant && massEarth > 0.2) {
        planet.setOutlineThickness(1.5f);
        planet.setOutlineColor(sf::Color(80, 255, 130));
      }
      if (b.m_id == selectedBodyId)                         // draw highlight around selected planet
      {
        sf::CircleShape selRing(visualR + 4.0f);
        selRing.setOrigin({visualR + 4.0f, visualR + 4.0f});
        selRing.setPosition(planet.getPosition());
        selRing.setFillColor(sf::Color::Transparent);
        selRing.setOutlineThickness(1.5f);
        selRing.setOutlineColor(sf::Color::Yellow);
        window.draw(selRing);
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
      ss << std::fixed << std::setprecision(4);
      ss << "Simulation started at: " << timeDate<< " with Zoom factor : " << zoomPixelsPerAU << "\n";
      ss << "Simulation speed: " << substepsPerFrame << " (simulation steps per frame)\n";
      ss << "View: " << (sideView ? "SIDE (X-Z) Inclination" : "TOP-DOWN (X-Y Orbial plane)\n");
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
      ss << std::setprecision(4);
      ss << "Time: " << (ssSim.getSimTime() / 1e6) << " Myr  |  Phase: "
         << (ssSim.isOligarchic() ? "OLIGARCHIC GIANT IMPACTS" : "GAS/DUST ACCRETION & MIGRATION") << "\n";
      ss << "Active Bodies: " << ssSim.getBodies().size() << "  |  Tidally Shredded: " << ssSim.getTidallyDisruptedCount()
         << "  |  HZ Worlds: " << habitablePlanets << "\n";

      if (selectedBodyId != -1)
      {
          const auto& bodies = ssSim.getBodies();
          auto it = std::find_if(bodies.begin(), bodies.end(),
              [selectedBodyId](const Body& b) { return b.m_id == selectedBodyId && b.m_active; });

          if (it != bodies.end())
          {
              PlanetTelemetry tel = computeTelemetry(*it, Primary);
              double mEarth = it->m_mass / M_EARTH;
              double mJup = it->m_mass / M_JUPITER;

              ss << "\n--- Selected Body #" << it->m_id << (it->m_isGiant ? " [Gas Giant]" : " [Rocky/Ice]") << " ---\n";
              ss << std::fixed << std::setprecision(3);
              ss << "Mass: " << mEarth << " M_earth (" << mJup << " M_jup)\n";
              ss << "Orbit: a = " << tel.a << " AU  |  e = " << tel.e << "  |  i = " << tel.i << " deg\n";
              ss << std::setprecision(1);
              ss << "Eq. Temp: " << tel.t << " K" << (it->m_inHabitableZone ? "  [IN HZ]" : "") << "\n";
          }
          else
          {
              selectedBodyId = -1; // Body merged or disrupted
          }

      }
      ss << "\n[R] New Star   [Scroll] Zoom    [Up/Down] Speed  [P] Pause  [T] trails  [V] Profile View";
      if (paused)
        ss << "[PAUSED]\n";

      sf::Text hud(font, ss.str(), 14);
      hud.setPosition({ 16.f, 16.f });
      hud.setFillColor(sf::Color(220, 230, 245));
      window.draw(hud);
    }
    window.display();
  }

  std::string outFileName = "ephemeris_" + timeDate + ".bin";
  if (writeBinaryEphemeris(outFileName, timeDate, earthLike, seed, Primary, ssSim))
  {
      std::cout << "Wrote binary ephemeris (" << ssSim.getBodies().size() << " bodies) to " << outFileName << std::endl;
  }
  else
  {
      std::cout << "Failed to write ephemeris to " << outFileName << std::endl;
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

PlanetTelemetry computeTelemetry(const Body& b, const star& primary)
{
    PlanetTelemetry tel;

    Vec3  rVec = b.m_pos - primary.pos();
    Vec3  vVec = b.m_vel - primary.vel();

    double_t r = rVec.mag();
    double_t vSq = vVec.magSq();
    double_t mu = G*(primary.mass() + b.m_mass);

    // Specific orbital energy ->semi-major axis: 1/a = 2/r - v^2/mu
    double_t invA = (2.0/r) - (vSq/mu);
    tel.a = (std::abs(invA) > 1E12) ? (1.0/invA): r;

    // Eccentricity vector: e_vec = ((v^2 - mu/r)*r_vec - (r_vec . v_vec)*v_vec) / mu
    Vec3 eVec = rVec*(vSq - mu/r) - vVec*rVec.dot(vVec)*(1.0/mu);
    tel.e = eVec.mag();

    // Specific angular momentum -> inclination cos(i) = h_z/|h|
    Vec3 hVec = rVec.cross(vVec);
    double_t hMag = hVec.mag();

    if (hMag > 1E-12)
    {
        double_t cosI = std::clamp(hVec.z / hMag, -1.0, 1.0);
        tel.i = std::acos(cosI)*(180.0/PI);
    }

    // equilibrium temperature, assuming Bond albedo A ~0.3, no greenhouse effect.
    // for earth T_eq ~ 255 K at 1 AU and 1 L_sum
    double_t distAU = std::max(r, 1E-4);
    double_t empK = 255.0 * std::pow(primary.luminosity(), 0.25) / std::sqrt(distAU);
    tel.t = empK;

    return tel;
}


PlanetEphemeris computePlanetEphemeris(const Body& b, const star& primary)
{
    PlanetEphemeris ep{};

    ep.id = b.m_id;
    ep.isGiant = b.m_isGiant ? 1 : 0;
    ep.inHabitableZone = b.m_inHabitableZone ? 1 : 0;
    ep.massEarth = b.m_mass / M_EARTH;
    
    Vec3 rVec = b.m_pos - primary.pos();
    Vec3 vVec = b.m_vel - primary.vel();
    double r = rVec.mag();
    double vSq = vVec.magSq();
    double mu = G * (primary.mass() + b.m_mass);
    
    // 1. Semi-major axis (a) & Period (T)
    double invA = (2.0 / r) - (vSq / mu);
    ep.semiMajorAxisAU = (std::abs(invA) > 1e-12) ? (1.0 / invA) : r;
    ep.periodYr = (ep.semiMajorAxisAU > 0.0)
        ? (2.0 * PI * std::sqrt(std::pow(ep.semiMajorAxisAU, 3.0) / mu))
        : 0.0; // Hyperbolic/unbound escape trajectory
    
    // 2. Angular momentum vector (h = r x v) & Inclination (i)
    Vec3 hVec = rVec.cross(vVec);
    double hMag = hVec.mag();
    if (hMag > 1e-12)
    {
        ep.inclinationDeg = std::acos(std::clamp(hVec.z / hMag, -1.0, 1.0)) * (180.0 / PI);
    }
    
    // 3. Node vector (n = k_hat x h = (-h_y, h_x, 0)) & RAAN (Omega)
    Vec3 nVec = { -hVec.y, hVec.x, 0.0 };
    double nMag = nVec.mag();
    if (nMag > 1e-12)
    {
        double raanRad = std::acos(std::clamp(nVec.x / nMag, -1.0, 1.0));
        if (nVec.y < 0.0) raanRad = 2.0 * PI - raanRad;
        ep.raanDeg = raanRad * (180.0 / PI);
    }
    
    // 4. Eccentricity vector (e_vec) & Argument of Periapsis (omega)
    Vec3 eVec = (rVec * (vSq - mu / r) - vVec * rVec.dot(vVec)) * (1.0 / mu);
    ep.eccentricity = eVec.mag();
    
    if (nMag > 1e-12 && ep.eccentricity > 1e-12)
    {
        double omegaRad = std::acos(std::clamp(nVec.dot(eVec) / (nMag * ep.eccentricity), -1.0, 1.0));
        if (eVec.z < 0.0) omegaRad = 2.0 * PI - omegaRad;
        ep.argPeriapsisDeg = omegaRad * (180.0 / PI);
    }
    else if (ep.eccentricity > 1e-12)
    {
        // Equatorial fallback (i ~ 0): measure longitude of periapsis from +X axis
        double omegaRad = std::atan2(eVec.y, eVec.x);
        if (omegaRad < 0.0) omegaRad += 2.0 * PI;
        ep.argPeriapsisDeg = omegaRad * (180.0 / PI);
    }
    
    return ep;
}

 bool writeBinaryEphemeris(const std::string & filename,
     const std::string & timeDate,
     bool earthLike,
     uint32_t seed,
     const star & primary,
     const solarSystemSim & sim)
 {
     std::vector<PlanetEphemeris> records;
     for (const auto& b : sim.getBodies())
     {
         if (!b.m_active) continue;
         records.push_back(computePlanetEphemeris(b, primary));
     }

     EphemerisHeader header{};
     header.endianness = (std::endian::native == std::endian::little) ? 1 : 2;
     std::snprintf(header.runDateTime, sizeof(header.runDateTime), "%s", timeDate.c_str());
     header.earthLike = earthLike ? 1 : 0;
     header.seed = seed;
     header.simTimeYr = sim.getSimTime();
     header.starMass = primary.mass();
     header.bodyCount = static_cast<uint32_t>(records.size());

     std::ofstream out(filename, std::ios::binary);
     if (!out) return false;

     out.write(reinterpret_cast<const char*>(&header), sizeof(EphemerisHeader));
     if (!records.empty())
     {
         out.write(reinterpret_cast<const char*>(records.data()),
             records.size() * sizeof(PlanetEphemeris));
     }
     return out.good();
 }

