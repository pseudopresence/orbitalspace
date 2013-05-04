#include "orStd.h"
#include "orPlatform/window.h"

#include "orbitalSpaceApp.h"

#include "orProfile/perftimer.h"
#include "task.h"
#include "taskScheduler.h"
#include "taskSchedulerWorkStealing.h"

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

#include <Eigen/Geometry>

#include "boost_begin.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include "boost_end.h"

#include "constants.h"

OrbitalSpaceApp::OrbitalSpaceApp():
  App(),
  m_rnd(1123LL),
  m_simTime(0.0),
  m_paused(false),
  m_singleStep(false),
  m_wireframe(false),
  m_cameraSystem(),
  m_renderSystem(),
  m_physicsSystem(),
  m_entitySystem(m_cameraSystem, m_renderSystem, m_physicsSystem),
  m_camOrig(true),
  m_camDist(-3.1855e7),
  m_camTheta(0.0),
  m_camPhi(0.0),
  m_cameraId(0),
  m_cameraTargetId(0),
  m_camMode(CameraMode_ThirdPerson),
  m_inputMode(InputMode_Default),
  m_playerShipId(0),
  m_integrationMethod(PhysicsSystem::IntegrationMethod_RK4),
  m_light(1, 1, 0),
  m_thrusters(0),
  m_hasFocus(false),
  m_music(),
  m_timeScale(1.0)
{
  // Create palette
  m_colG[0] = Vector3f(41,42,34)/255;
  m_colG[1] = Vector3f(77,82,50)/255;
  m_colG[2] = Vector3f(99,115,76)/255;
  m_colG[3] = Vector3f(151,168,136)/255;
  m_colG[4] = Vector3f(198,222,172)/255;

  for (int i = 0; i < PALETTE_SIZE; ++i)
  {
    m_colR[i] = Vector3f(m_colG[i].y(), m_colG[i].x(), m_colG[i].z());
    m_colB[i] = Vector3f(m_colG[i].x(), m_colG[i].z(), m_colG[i].y());
  }

  m_light /= m_light.norm();

  // Make camera

  CameraSystem::Camera& camera = m_cameraSystem.getCamera(m_cameraId = m_cameraSystem.makeCamera());
  camera.m_fov = 35.0; // degrees? Seems low...this is the vertical fov though...

  // Make debug text label

  RenderSystem::Label& debugTextLabel = m_renderSystem.getLabel(m_debugTextLabelId = m_renderSystem.makeLabel());
  debugTextLabel.m_pos = Vector3d(8, 8, 0);
  debugTextLabel.m_col = m_colG[4];

  // For now, give the moon a circular orbit

  double const muEarthMoon = (EARTH_MASS + MOON_MASS) * GRAV_CONSTANT;
  double const angularSpeed = MOON_PERIOD / M_TAU;

  double const earthMoonOrbitRadius = pow(muEarthMoon * angularSpeed * angularSpeed, 1.0/3.0); // meters

  // Distances from COM of Earth-Moon system
  double const earthOrbitRadius = earthMoonOrbitRadius * MOON_MASS / (EARTH_MASS + MOON_MASS);
  double const moonOrbitRadius = earthMoonOrbitRadius - earthOrbitRadius;

  Vector3d const earthPos = Vector3d(0.0, 0.0, -earthOrbitRadius);
  Vector3d const moonPos = Vector3d(0.0, 0.0, moonOrbitRadius);

  double const earthSpeed = earthOrbitRadius / angularSpeed;
  double const moonSpeed = moonOrbitRadius / angularSpeed;

  Vector3d const earthVel = Vector3d(-earthSpeed, 0.0, 0.0);
  Vector3d const moonVel = Vector3d(moonSpeed, 0.0, 0.0);

  // Create Earth

  EntitySystem::Planet& earthPlanet = m_entitySystem.getPlanet(m_earthPlanetId = m_entitySystem.makePlanet());

  PhysicsSystem::GravBody& earthGravBody = m_physicsSystem.getGravBody(earthPlanet.m_gravBodyId = m_physicsSystem.makeGravBody());

  earthGravBody.m_mass = EARTH_MASS;
  earthGravBody.m_radius = EARTH_RADIUS;

  earthGravBody.m_pos = earthPos;
  earthGravBody.m_vel = earthVel;

  earthGravBody.m_soiParentBody = earthPlanet.m_gravBodyId;

  RenderSystem::Sphere& earthSphere = m_renderSystem.getSphere(earthPlanet.m_sphereId = m_renderSystem.makeSphere());
  earthSphere.m_radius = earthGravBody.m_radius;
  earthSphere.m_pos = earthGravBody.m_pos;
  earthSphere.m_col = m_colG[1];

  CameraSystem::Target& earthCamTarget = m_cameraSystem.getTarget(earthPlanet.m_cameraTargetId = m_cameraSystem.makeTarget());
  earthCamTarget.m_pos = earthGravBody.m_pos;
  earthCamTarget.m_name = std::string("Earth");

  m_cameraTargetId = earthPlanet.m_cameraTargetId;

  // Create Moon

  EntitySystem::Moon& moonMoon = m_entitySystem.getMoon(m_moonMoonId = m_entitySystem.makeMoon());

  PhysicsSystem::GravBody& moonGravBody = m_physicsSystem.getGravBody(moonMoon.m_gravBodyId = m_physicsSystem.makeGravBody());

  moonGravBody.m_mass = MOON_MASS;
  moonGravBody.m_radius = MOON_RADIUS;

  moonGravBody.m_pos = moonPos;
  moonGravBody.m_vel = moonVel;

  moonGravBody.m_soiParentBody = earthPlanet.m_gravBodyId;

  RenderSystem::Sphere& moonSphere = m_renderSystem.getSphere(moonMoon.m_sphereId = m_renderSystem.makeSphere());
  moonSphere.m_radius = moonGravBody.m_radius;
  moonSphere.m_pos = moonGravBody.m_pos;
  moonSphere.m_col = m_colG[1];

  RenderSystem::Orbit& moonOrbit = m_renderSystem.getOrbit(moonMoon.m_orbitId = m_renderSystem.makeOrbit());
  moonOrbit.m_col = m_colG[1];
  moonOrbit.m_pos = earthGravBody.m_pos;

  RenderSystem::Trail& moonTrail = m_renderSystem.getTrail(moonMoon.m_trailId = m_renderSystem.makeTrail(5000.0, moonGravBody.m_pos, m_cameraSystem.getTarget(m_cameraTargetId).m_pos));
  moonTrail.m_colOld = m_colG[0];
  moonTrail.m_colNew = m_colG[4];

  CameraSystem::Target& moonCamTarget = m_cameraSystem.getTarget(moonMoon.m_cameraTargetId = m_cameraSystem.makeTarget());
  moonCamTarget.m_pos = moonGravBody.m_pos;
  moonCamTarget.m_name = std::string("Moon");

  RenderSystem::Label& moonLabel = m_renderSystem.getLabel(moonMoon.m_labelId = m_renderSystem.makeLabel());
  moonLabel.m_pos = moonGravBody.m_pos;
  moonLabel.m_col = m_colG[4];
  moonLabel.m_text = std::string("Moon");

  // Create Earth-Moon COM

  EntitySystem::Poi& comPoi = m_entitySystem.getPoi(m_comPoiId = m_entitySystem.makePoi());

  RenderSystem::Point& comPoint = m_renderSystem.getPoint(comPoi.m_pointId = m_renderSystem.makePoint());
  comPoint.m_pos = Vector3d(0.0, 0.0, 0.0);
  comPoint.m_col = Vector3f(1.0, 0.0, 0.0);

  CameraSystem::Target& comCamTarget = m_cameraSystem.getTarget(comPoi.m_cameraTargetId = m_cameraSystem.makeTarget());
  comCamTarget.m_pos = comPoint.m_pos;
  comCamTarget.m_name = std::string("Earth-Moon COM");

  for (int i = 0; i < 5; ++i) {
    EntitySystem::Poi& lagrangePoi = m_entitySystem.getPoi(m_lagrangePoiIds[i] = m_entitySystem.makePoi());

    RenderSystem::Point& lagrangePoint = m_renderSystem.getPoint(lagrangePoi.m_pointId = m_renderSystem.makePoint());
    lagrangePoint.m_pos = Vector3d(0.0, 0.0, 0.0);
    lagrangePoint.m_col = Vector3f(1.0, 0.0, 0.0);

    CameraSystem::Target& lagrangeCamTarget = m_cameraSystem.getTarget(lagrangePoi.m_cameraTargetId = m_cameraSystem.makeTarget());
    lagrangeCamTarget.m_pos = lagrangePoint.m_pos;
    std::stringstream builder;
    builder << "Earth-Moon L" << (i + 1);
    lagrangeCamTarget.m_name = builder.str();
  }

  // Create ships
  EntitySystem::Ship& playerShip = m_entitySystem.getShip(m_playerShipId = m_entitySystem.makeShip());

  PhysicsSystem::ParticleBody& playerBody = m_physicsSystem.getParticleBody(playerShip.m_particleBodyId = m_physicsSystem.makeParticleBody());

  playerBody.m_pos = Vector3d(0.0, 0.0, 1.3e7);
  playerBody.m_vel = Vector3d(5e3, 0.0, 0.0);

  RenderSystem::Orbit& playerOrbit = m_renderSystem.getOrbit(playerShip.m_orbitId = m_renderSystem.makeOrbit());
  playerOrbit.m_col = m_colB[2];
  playerOrbit.m_pos = earthGravBody.m_pos;

  RenderSystem::Trail& playerTrail = m_renderSystem.getTrail(playerShip.m_trailId = m_renderSystem.makeTrail(5000.0, playerBody.m_pos, m_cameraSystem.getTarget(m_cameraTargetId).m_pos));
  playerTrail.m_colOld = m_colB[0];
  playerTrail.m_colNew = m_colB[4];

  RenderSystem::Point& playerPoint = m_renderSystem.getPoint(playerShip.m_pointId = m_renderSystem.makePoint());
  playerPoint.m_pos = playerBody.m_pos;
  playerPoint.m_col = m_colB[4];

  CameraSystem::Target& playerCamTarget = m_cameraSystem.getTarget(playerShip.m_cameraTargetId = m_cameraSystem.makeTarget());
  playerCamTarget.m_pos = playerBody.m_pos;
  playerCamTarget.m_name = std::string("Player");

  EntitySystem::Ship& suspectShip = m_entitySystem.getShip(m_suspectShipId = m_entitySystem.makeShip());

  PhysicsSystem::ParticleBody& suspectBody = m_physicsSystem.getParticleBody(suspectShip.m_particleBodyId = m_physicsSystem.makeParticleBody());

  suspectBody.m_pos = Vector3d(0.0, 0.0, 1.3e7);
  suspectBody.m_vel = Vector3d(5e3, 0.0, 0.0);

  RenderSystem::Orbit& suspectOrbit = m_renderSystem.getOrbit(suspectShip.m_orbitId = m_renderSystem.makeOrbit());
  suspectOrbit.m_col = m_colR[2];
  suspectOrbit.m_pos = earthGravBody.m_pos;

  RenderSystem::Trail& suspectTrail = m_renderSystem.getTrail(suspectShip.m_trailId = m_renderSystem.makeTrail(5000.0, suspectBody.m_pos, m_cameraSystem.getTarget(m_cameraTargetId).m_pos));
  suspectTrail.m_colOld = m_colR[0];
  suspectTrail.m_colNew = m_colR[4];

  RenderSystem::Point& suspectPoint = m_renderSystem.getPoint(suspectShip.m_pointId = m_renderSystem.makePoint());
  suspectPoint.m_pos = suspectBody.m_pos;
  suspectPoint.m_col = m_colR[4];

  CameraSystem::Target& suspectCamTarget = m_cameraSystem.getTarget(suspectShip.m_cameraTargetId = m_cameraSystem.makeTarget());
  suspectCamTarget.m_pos = suspectBody.m_pos;
  suspectCamTarget.m_name = std::string("Suspect");

  // Perturb all the ship orbits
  // TODO this should update all the other positions too, or happen earlier!
  // TODO shouldn't iterate through IDs.
  float* rnds = new float[6 * m_entitySystem.numShips()];
  UniformDistribution<float> dist(-1, +1);
  dist.Generate(&m_rnd, 6 * m_entitySystem.numShips(), &rnds[0]);
  for (int i = 0; i < m_entitySystem.numShips(); ++i)
  {
    PhysicsSystem::ParticleBody& shipBody = m_physicsSystem.getParticleBody(m_entitySystem.getShip(i).m_particleBodyId);
    shipBody.m_pos += Vector3d(rnds[6*i  ], rnds[6*i+1], rnds[6*i+2]) * 6e4;
    shipBody.m_vel += Vector3d(rnds[6*i+3], rnds[6*i+4], rnds[6*i+5]) * 1e2;
  }
  delete[] rnds;

  m_music.openFromFile("music/spacething3_mastered_fullq.ogg");
  m_music.setLoop(true);
  // m_music.play();
}

OrbitalSpaceApp::~OrbitalSpaceApp()
{
}

// TODO put elsewhere
void runTests() {
  // Test FMod
  double const a = Util::FMod(3.0, 2.0);
  if ( a != 1.0 ) {
    DEBUGBREAK;
  }

  // Test Wrap
  double const b = Util::Wrap(3.5, 1.0, 2.0);
  if (b != 1.5) {
    DEBUGBREAK;
  }
}

void OrbitalSpaceApp::Run()
{
  runTests();

  while (m_running)
  {
    Timer::PerfTime const frameStart = Timer::GetPerfTime();

    {
      PERFTIMER("PollEvents");
      PollEvents();
    }

    if (m_hasFocus) {
      // Input handling
      if (m_inputMode == InputMode_RotateCamera) {
        sf::Vector2i const centerPos = sf::Vector2i(m_config.width/2, m_config.height/2);
        sf::Vector2i const mouseDelta = sf::Mouse::getPosition(*m_window) - centerPos;
        sf::Mouse::setPosition(centerPos, *m_window);

        m_camTheta += mouseDelta.x * M_TAU / 300.0;
        m_camTheta = Util::Wrap(m_camTheta, 0.0, M_TAU);
        m_camPhi += mouseDelta.y * M_TAU / 300.0;
        m_camPhi = Util::Clamp(m_camPhi, -.249 * M_TAU, .249 * M_TAU);
      }
    }

    {
      PERFTIMER("UpdateState");
      enum { UPDATES_PER_FRAME_HACK = 1 };
      for (int i = 0; i < UPDATES_PER_FRAME_HACK; ++i) {
        UpdateState(Timer::PerfTimeToMillis(m_lastFrameDuration));
      }
    }

    {
      PERFTIMER("BeginRender");
      BeginRender();
    }

    {
      PERFTIMER("RenderState");
      RenderState();
    }

    {
      PERFTIMER("EndRender");
      EndRender();
    }

    sf::sleep(sf::milliseconds(1)); // TODO sleep according to frame duration

    m_lastFrameDuration = Timer::GetPerfTime() - frameStart;
  }
}

void OrbitalSpaceApp::InitRender()
{
  App::InitRender();

  // TODO
  m_config.width = 1280;
  m_config.height = 768;

  sf::ContextSettings settings;
  settings.depthBits         = 24; // Request a 24 bits depth buffer
  settings.stencilBits       = 8;  // Request a 8 bits stencil buffer
  settings.antialiasingLevel = 2;  // Request 2 levels of antialiasing
  m_window = new sf::RenderWindow(sf::VideoMode(m_config.width, m_config.height, 32), "SFML OpenGL", sf::Style::Close, settings);

  GLenum err = glewInit();
  if (GLEW_OK != err) {
    /* Problem: glewInit failed, something is seriously wrong. */
    fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
  }

  sf::WindowHandle winHandle = m_window->getSystemHandle();
#ifdef WIN32 // TODO linux
  orPlatform::FocusWindow(winHandle);
#endif
  m_hasFocus = true;

}

void OrbitalSpaceApp::ShutdownRender()
{
  App::ShutdownRender();
}

void OrbitalSpaceApp::InitState()
{
  App::InitState();
}

void OrbitalSpaceApp::ShutdownState()
{
  App::ShutdownState();
}

void OrbitalSpaceApp::HandleEvent(sf::Event const& _event)
{
  /* TODO allow?
  if (_event.type == sf::Event::Resized)
  {
    glViewport(0, 0, _event.size.width, _event.size.height);
  }
  */

  if (_event.type == sf::Event::MouseButtonPressed) {
    if (_event.mouseButton.button == sf::Mouse::Right) {
      m_inputMode = InputMode_RotateCamera;

      // When rotating the camera, hide the mouse cursor and center it. We'll then track how far it's moved off center each frame.
      sf::Vector2i const centerPos = sf::Vector2i(m_config.width/2, m_config.height/2);
      m_savedMousePos = sf::Mouse::getPosition(*m_window);
      m_window->setMouseCursorVisible(false);
      sf::Mouse::setPosition(centerPos, *m_window);

    }
  }

  if (_event.type == sf::Event::MouseButtonReleased) {
    if (_event.mouseButton.button == sf::Mouse::Right) {
      // Restore the old position of the cursor.
      m_inputMode = InputMode_Default;
      sf::Mouse::setPosition(m_savedMousePos, *m_window);
      m_window->setMouseCursorVisible(true);
    }
  }

  if (_event.type == sf::Event::MouseWheelMoved)
  {
    m_camDist *= pow(0.9, _event.mouseWheel.delta);
  }

  if (_event.type == sf::Event::KeyPressed)
  {
    if (_event.key.code == sf::Keyboard::Escape)
    {
      m_running = false;
    }

    if (_event.key.code == sf::Keyboard::Tab)
    {
      m_cameraTargetId = m_cameraSystem.nextTarget(m_cameraTargetId);
      // TODO clear all trails
      // TODO make all trails relative to camera target
      // Maybe make a physics frame object, with position + velocity?
      // Make Body the frame?
      // Make frame its own system, have render, physics etc share the frame?
    }

    if (_event.key.code == sf::Keyboard::F1) {
      m_camMode = CameraMode_FirstPerson;
    }

    if (_event.key.code == sf::Keyboard::F2) {
      m_camMode = CameraMode_ThirdPerson;
    }

    if (_event.key.code == sf::Keyboard::PageDown) {
      m_integrationMethod = PhysicsSystem::IntegrationMethod((m_integrationMethod + 1) % PhysicsSystem::IntegrationMethod_Count);
    }

    if (_event.key.code == sf::Keyboard::R)
    {
      m_camOrig = !m_camOrig;
    }

    if (_event.key.code == sf::Keyboard::Add || _event.key.code == sf::Keyboard::Equal)
    {
      m_timeScale *= 2;
    }

    if (_event.key.code == sf::Keyboard::Subtract || _event.key.code == sf::Keyboard::Dash)
    {
      m_timeScale /= 2;
    }

    if (_event.key.code == sf::Keyboard::A)
    {
      m_thrusters |= ThrustLeft;
    }
    if (_event.key.code == sf::Keyboard::D)
    {
      m_thrusters |= ThrustRight;
    }
    if (_event.key.code == sf::Keyboard::W)
    {
      m_thrusters |= ThrustFwd;
    }
    if (_event.key.code == sf::Keyboard::S)
    {
      m_thrusters |= ThrustBack;
    }
    if (_event.key.code == sf::Keyboard::Up)
    {
      m_thrusters |= ThrustUp;
    }
    if (_event.key.code == sf::Keyboard::Down)
    {
      m_thrusters |= ThrustDown;
    }
  }

  if (_event.type == sf::Event::KeyReleased)
  {
    if (_event.key.code == sf::Keyboard::A)
    {
      m_thrusters &= ~ThrustLeft;
    }
    if (_event.key.code == sf::Keyboard::D)
    {
      m_thrusters &= ~ThrustRight;
    }
    if (_event.key.code == sf::Keyboard::W)
    {
      m_thrusters &= ~ThrustFwd;
    }
    if (_event.key.code == sf::Keyboard::S)
    {
      m_thrusters &= ~ThrustBack;
    }
    if (_event.key.code == sf::Keyboard::Up)
    {
      m_thrusters &= ~ThrustUp;
    }
    if (_event.key.code == sf::Keyboard::Down)
    {
      m_thrusters &= ~ThrustDown;
    }
  }

  if (_event.type == sf::Event::Closed)
  {
    m_running = false;
  }

  if (_event.type == sf::Event::LostFocus)
  {
    m_hasFocus = false;
  }

  if (_event.type == sf::Event::GainedFocus)
  {
    m_hasFocus = true;
  }
}

Vector3d OrbitalSpaceApp::CalcPlayerThrust(PhysicsSystem::ParticleBody const& playerBody)
{
  Vector3d origin = m_physicsSystem.findSOIGravBody(playerBody).m_pos;

  // Calc acceleration due to gravity
  Vector3d const r = (origin - playerBody.m_pos);
  double const r_mag = r.norm();

  Vector3d const r_dir = r / r_mag;

  // Calc acceleration due to thrust
  double const thrustAccel = 10.0; // meters per second squared - TODO what is a realistic value?

  Vector3d thrustVec(0.0,0.0,0.0);

  Vector3d const fwd = playerBody.m_vel.normalized(); // Prograde
  Vector3d const left = fwd.cross(r_dir); // name? (and is the order right?)
  Vector3d const dwn = left.cross(fwd); // name? (and is the order right?)

  if (m_thrusters & ThrustFwd)  { thrustVec += fwd; }
  if (m_thrusters & ThrustBack) { thrustVec -= fwd; }
  if (m_thrusters & ThrustDown)  { thrustVec += dwn; }
  if (m_thrusters & ThrustUp) { thrustVec -= dwn; }
  if (m_thrusters & ThrustLeft)  { thrustVec += left; }
  if (m_thrusters & ThrustRight) { thrustVec -= left; }

  Vector3d a_thrust = thrustAccel * thrustVec;

  return a_thrust;
}

void OrbitalSpaceApp::UpdateState(double const _dt)
{
  if (!m_paused) {
    // Update Simulation

    double dt = m_timeScale * Util::Min(_dt, 100.0) / 1000.0; // seconds

    // Update player thrust
    PhysicsSystem::ParticleBody& playerShipBody = m_physicsSystem.getParticleBody(m_entitySystem.getShip(m_playerShipId).m_particleBodyId);
    playerShipBody.m_userAcc = CalcPlayerThrust(playerShipBody);

    m_physicsSystem.update(m_integrationMethod, dt);

    m_entitySystem.update(_dt, m_cameraSystem.getTarget(m_cameraTargetId).m_pos);

    // TODO eaghghgh not clear where these should live

    // Update the earth-moon COM
    PhysicsSystem::GravBody& earthBody = m_physicsSystem.getGravBody(m_entitySystem.getPlanet(m_earthPlanetId).m_gravBodyId);
    PhysicsSystem::GravBody& moonBody = m_physicsSystem.getGravBody(m_entitySystem.getMoon(m_moonMoonId).m_gravBodyId);
    double const totalMass = earthBody.m_mass + moonBody.m_mass;
    Vector3d comPos = (earthBody.m_pos * earthBody.m_mass / totalMass) + (moonBody.m_pos * moonBody.m_mass / totalMass);

    EntitySystem::Poi& comPoi = m_entitySystem.getPoi(m_comPoiId);
    RenderSystem::Point& comPoint = m_renderSystem.getPoint(comPoi.m_pointId);
    comPoint.m_pos = comPos;
    CameraSystem::Target& comTarget = m_cameraSystem.getTarget(comPoi.m_cameraTargetId);
    comTarget.m_pos = comPos;

    // Update the earth-moon Lagrange points
    Vector3d const earthMoonVector = moonBody.m_pos - earthBody.m_pos;
    double const earthMoonOrbitRadius = earthMoonVector.norm();
    Vector3d const earthMoonDir = earthMoonVector / earthMoonOrbitRadius;
    double const massRatio = MOON_MASS / EARTH_MASS;
    double const r1 = earthMoonOrbitRadius * pow(massRatio / 3.0, 1.0/3.0);
    double const r3 = earthMoonOrbitRadius * (1.0 + (7.0/12.0) * massRatio); // extra 1.0 to make r3 a distand from Earth position rather than an offset from earthMoonOrbitRadius

    Vector3d lagrangePos[5];
    // Lagrange point 1
    lagrangePos[0] = moonBody.m_pos - earthMoonDir * r1;
    // Lagrange point 2
    lagrangePos[1] = moonBody.m_pos + earthMoonDir * r1;
    // Lagrange point 3
    lagrangePos[2] = earthBody.m_pos - earthMoonDir * r3;

    // L4 and L5 are on the Moon's orbit, 60 degrees ahead and 60 degrees behind.
    Vector3d orbitAxis = moonBody.m_vel.normalized().cross(earthMoonVector.normalized());
    Eigen::AngleAxisd rotation(M_TAU / 6.0, orbitAxis);
    // Lagrange point 4
    lagrangePos[3] = rotation           * earthMoonVector;
    // Lagrange point 5
    lagrangePos[4] = rotation.inverse() * earthMoonVector;

    for (int i = 0; i < 5; ++i) {
      EntitySystem::Poi& lagrangePoi = m_entitySystem.getPoi(m_lagrangePoiIds[i]);
      RenderSystem::Point& lagrangePoint = m_renderSystem.getPoint(lagrangePoi.m_pointId);
      lagrangePoint.m_pos = lagrangePos[i];
      CameraSystem::Target& lagrangeTarget = m_cameraSystem.getTarget(lagrangePoi.m_cameraTargetId);
      lagrangeTarget.m_pos = lagrangePos[i];
    }

    m_simTime += dt;
  }

  if (m_singleStep) {
    // Pause simulation after this step

    m_singleStep = false;
    m_paused = true;
  }

  {
    // Update camera

    CameraSystem::Target& camTarget = m_cameraSystem.getTarget(m_cameraTargetId);
    Vector3d const camTargetPos = camTarget.m_pos;

    Vector3d camPos;

    if (m_camMode == CameraMode_FirstPerson) {
      camPos = m_physicsSystem.getParticleBody(m_entitySystem.getShip(m_playerShipId).m_particleBodyId).m_pos;
    } else if (m_camMode == CameraMode_ThirdPerson) {
      camPos = Vector3d(0.0, 0.0, m_camDist);

      Eigen::AngleAxisd thetaRot(m_camTheta, Vector3d(0.0, 1.0, 0.0));
      Eigen::AngleAxisd phiRot(m_camPhi, Vector3d(1.0, 0.0, 0.0));

      Eigen::Affine3d camMat1;
      camMat1.setIdentity();
      camMat1.rotate(thetaRot).rotate(phiRot);

      camPos = camMat1 * camPos;
      camPos += camTargetPos;
    } else {
      assert(false);
    }

    CameraSystem::Camera& camera = m_cameraSystem.getCamera(m_cameraId);
    camera.m_pos = camPos;
  }
}

Vector3d lerp(Vector3d const& _x0, Vector3d const& _x1, double const _a) {
    return _x0 * (1 - _a) + _x1 * _a;
}

void OrbitalSpaceApp::RenderState()
{
  Eigen::Matrix4d screenMatrix = m_cameraSystem.calcScreenMatrix( m_config.width, m_config.height );
  // Projection matrix (GL_PROJECTION)
  // Simplified for symmetric case
  double const minZ = 1.0; // meters
  double const maxZ = 1e11; // meters

  Eigen::Matrix4d projMatrix = m_cameraSystem.calcProjMatrix(m_cameraId, m_config.width, m_config.height, minZ, maxZ );

  // Camera matrix (GL_MODELVIEW)
  Vector3d up(0.0, 1.0, 0.0);
  Eigen::Affine3d camMatrix = m_cameraSystem.calcCameraMatrix(m_cameraId, m_cameraTargetId, up);

  m_window->resetGLStates();

  // Render debug text
  {
    std::ostringstream str;
    str.precision(3);
    str.width(7);
    str.flags(std::ios::right | std::ios::fixed);

    str << "Time Scale: " << m_timeScale << "\n";

    {
      using namespace boost::posix_time;
      using namespace boost::gregorian;

      // Astronomical Epoch: 1200 hours, 1 January 2000
      // Game start date:
      ptime epoch(date(2000, Jan, 1), hours(12));
      ptime gameStart(date(2025, Mar, 15), hours(1753));
      // Note: the (long) here limits us to ~68 years game time. Should be enough, otherwise just need to keep adding seconds to the dateTime to match the simTime
      ptime curDateTime = gameStart + seconds((long)m_simTime);
      str << "UTC DateTime: " << to_simple_string(curDateTime) << "\n";
    }

    CameraSystem::Target& camTarget = m_cameraSystem.getTarget(m_cameraTargetId);
    str << "Cam Target: " << camTarget.m_name << "\n";
    str << "Cam Dist: " << m_camDist << "\n";
    str << "Cam Theta:" << m_camTheta << "\n";
    str << "Cam Phi:" << m_camPhi << "\n";
    // double const shipDist = (m_ships[0].m_physics.m_pos - m_ships[1].m_physics.m_pos).norm();
    // str << "Intership Distance:" << shipDist << "\n";
    str << "Intership Distance: TODO\n";
    str << "Integration Method: " << m_integrationMethod << "\n";

    // TODO: better double value text formatting
    // TODO: small visualisations for the angle etc values

    RenderSystem::Label& debugTextLabel = m_renderSystem.getLabel(m_debugTextLabelId);
    debugTextLabel.m_text = str.str();
  }

  m_renderSystem.render2D(m_window, (screenMatrix * projMatrix * camMatrix).matrix());
  // m_renderSystem.render2D(m_window, (projMatrix * camMatrix.inverse()).matrix());

  m_window->resetGLStates();

  glViewport(0, 0, m_config.width, m_config.height);

  Vector3f clearCol = m_colG[0];
  glClearColor(clearCol.x(), clearCol.y(), clearCol.z(), 0);
  glClearDepth(minZ);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMultMatrix( projMatrix );

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glMultMatrix( camMatrix );

  glEnable(GL_TEXTURE_2D);

  glLineWidth(1);
  glEnable(GL_POINT_SMOOTH);
  glEnable(GL_LINE_SMOOTH);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // TODO clean up
  if (m_wireframe) {
    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
  } else {
    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
  }

  m_renderSystem.render3D(m_window);

  printf("Frame Time: %04.1f ms Total Sim Time: %04.1f s \n", Timer::PerfTimeToMillis(m_lastFrameDuration), m_simTime / 1000);
}
