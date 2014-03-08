#include "orStd.h"

#include "orApp.h"

#include "orProfile/perftimer.h"

#if 0
#include "task.h"
#include "taskScheduler.h"
#include "taskSchedulerWorkStealing.h"
#endif

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

// SDL
#include <SDL.h>
#include <SDL_log.h>

#include <Eigen/Geometry>

#include "boost_begin.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include "boost_end.h"

#include "constants.h"

#include "util.h"

#include "orGfx.h"

#include "orPlatform/window.h"

orApp::orApp(Config const& config):
  m_appScreen(Screen_Title),
  m_lastFrameDuration(0),
  m_running(true),
  m_rnd(1123LL),
  m_simTime(0.0),
  m_config(config),
  m_paused(false),
  m_singleStep(false),

  m_cameraSystem(),
  m_camMode(CameraMode_ThirdPerson),
  m_cameraId(0),
  m_cameraTargetId(0),
  m_camParams(-3.1855e7),

  m_renderSystem(),
  m_frameBuffer(),
  m_wireframe(false),

  m_physicsSystem(),
  m_timeScale(1.0),
  m_integrationMethod(PhysicsSystem::IntegrationMethod_RK4),

  m_entitySystem(m_cameraSystem, m_renderSystem, m_physicsSystem),
  m_playerShipId(0),

  m_inputMode(InputMode_Default),
  m_thrusters(0),
  m_hasFocus(false),
  m_window(NULL),
  m_music(NULL)
{
  orLog("Starting init\n");

  PerfTimer::StaticInit();

  Init();

#if 0
  m_music = new Music();
  m_music->openFromFile("music/spacething3_mastered_fullq.ogg");
  m_music->setLoop(true);
  m_music->play();
#endif
  SDL_LogWarn(SDL_LOG_CATEGORY_ERROR, "TODO NYI Music");
  orLog("Init complete\n");
}

orApp::~orApp()
{
  orLog("Starting shutdown\n");
  Shutdown();

  delete m_music; m_music = NULL;

  PerfTimer::StaticShutdown(); // TODO terrible code

  orLog("Shutdown complete\n");
}

void orApp::Init()
{
  // TODO init audio
  // TODO init...joystick? gamecontroller? need to look up what each of those do
  if (SDL_Init(SDL_INIT_EVENTS|SDL_INIT_TIMER|SDL_INIT_VIDEO) != 0) {
    fprintf(stderr,
      "\nUnable to initialize SDL:  %s\n",
      SDL_GetError()
    );
  }

  // Show anything with priority of Info and above
  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);

  InitState();
  InitRender();
}

void orApp::Shutdown()
{
  ShutdownRender();
  ShutdownState();
  SDL_Quit();
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

void orApp::PollEvents()
{
#if 0
  sf::Event event;
  while (m_window->pollEvent(event))
  {
    HandleEvent(event);
  }
#endif
  SDL_LogWarn(SDL_LOG_CATEGORY_ERROR, "TODO NYI PollEvents()");
}

void orApp::Run()
{
  runTests();

  while (m_running)
  {
    Timer::PerfTime const frameStart = Timer::GetPerfTime();

    RunOneStep();

    SDL_Delay(1); // milliseconds

    m_lastFrameDuration = Timer::GetPerfTime() - frameStart;
  }
}

void orApp::RunOneStep()
{
  {
    PERFTIMER("PollEvents");
    PollEvents();
  }

  {
    PERFTIMER("HandleInput");
    HandleInput();
  }

  {
    PERFTIMER("UpdateState");
    UpdateState();
  }

  {
    PERFTIMER("RenderState");
    RenderState();
  }
}

void orApp::HandleInput()
{
  if (!m_hasFocus) {
    return;
  }

  // Input handling
  if (m_inputMode == InputMode_RotateCamera) {
    SDL_LogWarn(SDL_LOG_CATEGORY_ERROR, "TODO NYI Camera Rotation");
#if 0
    sf::Vector2i const centerPos = sf::Vector2i(m_config.windowWidth/2, m_config.windowHeight/2);
    sf::Vector2i const mouseDelta = sf::Mouse::getPosition(*m_window) - centerPos;
    sf::Mouse::setPosition(centerPos, *m_window);

    double const dx = mouseDelta.x * M_TAU / 300.0;
    m_camParams.theta = Util::Wrap(m_camParams.theta + dx, 0.0, M_TAU);
    double const dy = mouseDelta.y * M_TAU / 300.0;
    m_camParams.phi = Util::Clamp(m_camParams.phi + dy, -.249 * M_TAU, .249 * M_TAU);
#endif
  }
}

// TODO cleanup
char* file_contents(const char* filename, GLint* length)
{
    FILE* f = fopen(filename, "r");
    char* buffer;

    if (!f) {
        fprintf(stderr, "Unable to open %s for reading\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *length = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = (char*)malloc(*length+1);
    *length = fread(buffer, 1, *length, f);
    fclose(f);
    buffer[*length] = '\0';

    return buffer;
}

// TODO cleanup
static void show_info_log(
    GLuint object,
    PFNGLGETSHADERIVPROC glGet__iv,
    PFNGLGETSHADERINFOLOGPROC glGet__InfoLog
)
{
    GLint log_length;
    char* log;

    glGet__iv(object, GL_INFO_LOG_LENGTH, &log_length);
    log = (char*)malloc(log_length);
    glGet__InfoLog(object, log_length, NULL, log);
    fprintf(stderr, "%s", log);
    free(log);
}

// TODO cleanup
static GLuint make_shader(GLenum type, const char* filename)
{
    GLint length;
    GLchar* source = file_contents(filename, &length);
    GLuint shader;
    GLint shader_ok;

    if (!source)
        return 0;

    shader = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar**)&source, &length);
    free(source);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_ok);
    if (!shader_ok) {
        fprintf(stderr, "Failed to compile %s:\n", filename);
        show_info_log(shader, glGetShaderiv, glGetShaderInfoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// TODO cleanup
static GLuint make_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLint program_ok;

    GLuint program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &program_ok);
    if (!program_ok) {
        fprintf(stderr, "Failed to link shader program:\n");
        show_info_log(program, glGetProgramiv, glGetProgramInfoLog);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

void orApp::InitRender()
{
  // TODO move to render system

  // TODO z-ordering seems bad on text labels (Moon label appears in front of everything else - not sure if this is what I want, would be good to have the option)
  // TODO 3d labels show when behind the camera as well as when they are in front.
  // TODO trails are bugged when switching camera targets.

  // TODO SDL logging not flushing?
#if 0
  sf::ContextSettings settings;
  settings.depthBits         = 24; // Request a 24 bits depth buffer
  settings.stencilBits       = 8;  // Request a 8 bits stencil buffer
  settings.antialiasingLevel = 2;  // Request 2 levels of antialiasing
#endif
  // TODO backbuffer settings as above?

  // Window mode MUST include SDL_WINDOW_OPENGL for use with OpenGL.
  m_window = SDL_CreateWindow(
    "Orbital Space", 0, 0, m_config.windowWidth, m_config.windowHeight,
    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

  // Create an OpenGL context associated with the window.
  m_gl_context = SDL_GL_CreateContext(m_window);

  GLenum err = glewInit();
  if (GLEW_OK != err) {
    /* Problem: glewInit failed, something is seriously wrong. */
    fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
  }

  m_frameBuffer = m_renderSystem.makeFrameBuffer(m_config.renderWidth, m_config.renderHeight);

  m_renderSystem.initRender();

#ifdef WIN32 // TODO linux
  sf::WindowHandle winHandle = m_window->getSystemHandle();
  orPlatform::FocusWindow(winHandle);
#endif
  m_hasFocus = true;
}

void orApp::ShutdownRender()
{
  m_renderSystem.shutdownRender();

  // TODO free opengl resources

  // Once finished with OpenGL functions, the SDL_GLContext can be deleted.
  SDL_GL_DeleteContext(m_gl_context);

  SDL_DestroyWindow(m_window);
  m_window = NULL;
}

void orApp::InitState()
{
  // Create NES-ish palette (selected colour sets from the NES palettes, taken from Wikipedia)
  m_colR[0] = RenderSystem::Colour(0,0,0)/255.f;
  m_colR[1] = RenderSystem::Colour(136,20,0)/255.f;
  m_colR[2] = RenderSystem::Colour(228,92,16)/255.f;
  m_colR[3] = RenderSystem::Colour(252,160,68)/255.f;
  m_colR[4] = RenderSystem::Colour(252,224,168)/255.f;

  m_colG[0] = RenderSystem::Colour(0,0,0)/255.f;
  m_colG[1] = RenderSystem::Colour(0,120,0)/255.f;
  m_colG[2] = RenderSystem::Colour(0,184,0)/255.f;
  m_colG[3] = RenderSystem::Colour(184,248,24)/255.f;
  m_colG[4] = RenderSystem::Colour(216,248,120)/255.f;

  m_colB[0] = RenderSystem::Colour(0,0,0)/255.f;
  m_colB[1] = RenderSystem::Colour(0,0,252)/255.f;
  m_colB[2] = RenderSystem::Colour(0,120,248)/255.f;
  m_colB[3] = RenderSystem::Colour(60,188,252)/255.f;
  m_colB[4] = RenderSystem::Colour(164,228,252)/255.f;

  Vector3d const lightDir = Vector3d(1.0, 1.0, 0.0).normalized();

  const double* const lightDirData = lightDir.data();

  m_lightDir[0] = lightDirData[0];
  m_lightDir[1] = lightDirData[1];
  m_lightDir[2] = lightDirData[2];

  // Make camera

  CameraSystem::Camera& camera = m_cameraSystem.getCamera(m_cameraId = m_cameraSystem.makeCamera());
  camera.m_fov = 35.0; // degrees? Seems low...this is the vertical fov though...

  // Make debug text label3D

  // RenderSystem::Label2D& debugTextLabel2D = m_renderSystem.getLabel2D(m_debugTextLabel2DId = m_renderSystem.makeLabel2D());
  m_uiTextTopLabel2DId = m_renderSystem.makeLabel2D();
  RenderSystem::Label2D& m_uiTextTopLabel2D = m_renderSystem.getLabel2D(m_uiTextTopLabel2DId);

  m_uiTextTopLabel2D.m_pos[0] = 0;
  m_uiTextTopLabel2D.m_pos[1] = 0;

  m_uiTextTopLabel2D.m_col[0] =  m_colG[4].x();
  m_uiTextTopLabel2D.m_col[1] =  m_colG[4].y();
  m_uiTextTopLabel2D.m_col[2] =  m_colG[4].z();

  m_uiTextBottomLabel2DId = m_renderSystem.makeLabel2D();
  RenderSystem::Label2D& m_uiTextBottomLabel2D = m_renderSystem.getLabel2D(m_uiTextBottomLabel2DId);

  m_uiTextBottomLabel2D.m_pos[0] = 0;
  m_uiTextBottomLabel2D.m_pos[1] = m_config.renderHeight - 8; // TODO HAX - 8 is the bitmap font height, shouldn't be hardcoded

  m_uiTextBottomLabel2D.m_col[0] =  m_colG[4].x();
  m_uiTextBottomLabel2D.m_col[1] =  m_colG[4].y();
  m_uiTextBottomLabel2D.m_col[2] =  m_colG[4].z();

  // Load in the json data for the solar system
  // TODO noticed that the data I'm using only has partial information and
  // not very well explained units/reference frames
  // In particular, the orbits aren't drawn from the json but parsed from a
  // binary JPL database of polynomial fits
#if 0
  namespace ptree = boost::property_tree;
  ptree::ptree data_root;
  ptree::json_parser::read_json("data/solarsys.json", data_root);
  printf("%s\n", data_root.get<std::string>("name").c_str());
  BOOST_FOREACH(ptree::ptree::value_type &v,
            data_root.get_child("require"))
  {
    std::string file_name = v.second.get_value<std::string>();
    printf("%s\n", file_name.c_str());
  }
#endif

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

  double const* const earthPosData = earthPos.data();
  double const* const earthVelData = earthVel.data();

  double const* const moonPosData = moonPos.data();
  double const* const moonVelData = moonVel.data();

  // Create Earth

  EntitySystem::Planet& earthPlanet = m_entitySystem.getPlanet(m_earthPlanetId = m_entitySystem.makePlanet());

  PhysicsSystem::GravBody& earthGravBody = m_physicsSystem.getGravBody(earthPlanet.m_gravBodyId = m_physicsSystem.makeGravBody());
  {
    earthGravBody.m_mass = EARTH_MASS;
    earthGravBody.m_radius = EARTH_RADIUS;

    earthGravBody.m_pos[0] = earthPosData[0];
    earthGravBody.m_pos[1] = earthPosData[1];
    earthGravBody.m_pos[2] = earthPosData[2];

    earthGravBody.m_vel[0] = earthVelData[0];
    earthGravBody.m_vel[1] = earthVelData[1];
    earthGravBody.m_vel[2] = earthVelData[2];

    earthGravBody.m_soiParentBody = earthPlanet.m_gravBodyId;
  }

  RenderSystem::Sphere& earthSphere = m_renderSystem.getSphere(earthPlanet.m_sphereId = m_renderSystem.makeSphere());
  {
    earthSphere.m_radius = earthGravBody.m_radius;

    earthSphere.m_pos[0] = earthPosData[0];
    earthSphere.m_pos[1] = earthPosData[1];
    earthSphere.m_pos[2] = earthPosData[2];

    earthSphere.m_col[0] = m_colG[1].x();
    earthSphere.m_col[1] = m_colG[1].y();
    earthSphere.m_col[2] = m_colG[1].z();
  }

  CameraSystem::Target& earthCamTarget = m_cameraSystem.getTarget(earthPlanet.m_cameraTargetId = m_cameraSystem.makeTarget());
  {
    earthCamTarget.m_pos[0] = earthPosData[0];
    earthCamTarget.m_pos[1] = earthPosData[1];
    earthCamTarget.m_pos[2] = earthPosData[2];

    earthCamTarget.m_name = std::string("Earth");
  }

  m_cameraTargetId = earthPlanet.m_cameraTargetId;

  // Create Moon

  EntitySystem::Moon& moonMoon = m_entitySystem.getMoon(m_moonMoonId = m_entitySystem.makeMoon());

  PhysicsSystem::GravBody& moonGravBody = m_physicsSystem.getGravBody(moonMoon.m_gravBodyId = m_physicsSystem.makeGravBody());
  {
    moonGravBody.m_mass = MOON_MASS;
    moonGravBody.m_radius = MOON_RADIUS;

    moonGravBody.m_pos[0] = moonPosData[0];
    moonGravBody.m_pos[1] = moonPosData[1];
    moonGravBody.m_pos[2] = moonPosData[2];

    moonGravBody.m_vel[0] = moonVelData[0];
    moonGravBody.m_vel[1] = moonVelData[1];
    moonGravBody.m_vel[2] = moonVelData[2];

    moonGravBody.m_soiParentBody = earthPlanet.m_gravBodyId;
  }

  RenderSystem::Sphere& moonSphere = m_renderSystem.getSphere(moonMoon.m_sphereId = m_renderSystem.makeSphere());
  {
    moonSphere.m_radius = moonGravBody.m_radius;

    moonSphere.m_pos[0] = moonPosData[0];
    moonSphere.m_pos[1] = moonPosData[1];
    moonSphere.m_pos[2] = moonPosData[2];

    moonSphere.m_col[0] = m_colG[1].x();
    moonSphere.m_col[1] = m_colG[1].y();
    moonSphere.m_col[2] = m_colG[1].z();
  }

  RenderSystem::Orbit& moonOrbit = m_renderSystem.getOrbit(moonMoon.m_orbitId = m_renderSystem.makeOrbit());
  {
    // Orbit pos is pos of parent body

    moonOrbit.m_pos[0] = earthPosData[0];
    moonOrbit.m_pos[1] = earthPosData[1];
    moonOrbit.m_pos[2] = earthPosData[2];

    moonOrbit.m_col[0] = m_colG[1].x();
    moonOrbit.m_col[1] = m_colG[1].y();
    moonOrbit.m_col[2] = m_colG[1].z();
  }

  RenderSystem::Trail& moonTrail = m_renderSystem.getTrail(moonMoon.m_trailId = m_renderSystem.makeTrail(5000.0, moonGravBody.m_pos, m_cameraSystem.getTarget(m_cameraTargetId).m_pos));
  {
    moonTrail.m_colOld[0] = m_colG[0].x();
    moonTrail.m_colOld[1] = m_colG[0].y();
    moonTrail.m_colOld[2] = m_colG[0].z();

    moonTrail.m_colNew[0] = m_colG[4].x();
    moonTrail.m_colNew[1] = m_colG[4].y();
    moonTrail.m_colNew[2] = m_colG[4].z();
  }

  CameraSystem::Target& moonCamTarget = m_cameraSystem.getTarget(moonMoon.m_cameraTargetId = m_cameraSystem.makeTarget());
  {
    moonCamTarget.m_pos[0] = moonPosData[0];
    moonCamTarget.m_pos[1] = moonPosData[1];
    moonCamTarget.m_pos[2] = moonPosData[2];

    moonCamTarget.m_name = std::string("Moon");
  }

  RenderSystem::Label3D& moonLabel3D = m_renderSystem.getLabel3D(moonMoon.m_label3DId = m_renderSystem.makeLabel3D());
  {
    moonLabel3D.m_pos[0] = moonPosData[0];
    moonLabel3D.m_pos[1] = moonPosData[1];
    moonLabel3D.m_pos[2] = moonPosData[2];

    moonLabel3D.m_col[0] = m_colG[4].x();
    moonLabel3D.m_col[1] = m_colG[4].y();
    moonLabel3D.m_col[2] = m_colG[4].z();

    moonLabel3D.m_text = std::string("Moon");
  }

  // Create Earth-Moon COM

  EntitySystem::Poi& comPoi = m_entitySystem.getPoi(m_comPoiId = m_entitySystem.makePoi());

  RenderSystem::Point& comPoint = m_renderSystem.getPoint(comPoi.m_pointId = m_renderSystem.makePoint());
  {
    comPoint.m_pos[0] = 0.0;
    comPoint.m_pos[1] = 0.0;
    comPoint.m_pos[2] = 0.0;

    comPoint.m_col[0] = 1.0f;
    comPoint.m_col[1] = 0.0f;
    comPoint.m_col[2] = 0.0f;
  }

  CameraSystem::Target& comCamTarget = m_cameraSystem.getTarget(comPoi.m_cameraTargetId = m_cameraSystem.makeTarget());
  {
    comCamTarget.m_pos[0] = comPoint.m_pos[0];
    comCamTarget.m_pos[1] = comPoint.m_pos[1];
    comCamTarget.m_pos[2] = comPoint.m_pos[2];

    comCamTarget.m_name = std::string("Earth-Moon COM");
  }

  for (int i = 0; i < 5; ++i) {
    EntitySystem::Poi& lagrangePoi = m_entitySystem.getPoi(m_lagrangePoiIds[i] = m_entitySystem.makePoi());

    RenderSystem::Point& lagrangePoint = m_renderSystem.getPoint(lagrangePoi.m_pointId = m_renderSystem.makePoint());
    {
      lagrangePoint.m_pos[0] = 0.0;
      lagrangePoint.m_pos[1] = 0.0;
      lagrangePoint.m_pos[2] = 0.0;

      lagrangePoint.m_col[0] = 1.0f;
      lagrangePoint.m_col[1] = 0.0f;
      lagrangePoint.m_col[2] = 0.0f;
    }

    CameraSystem::Target& lagrangeCamTarget = m_cameraSystem.getTarget(lagrangePoi.m_cameraTargetId = m_cameraSystem.makeTarget());

    lagrangeCamTarget.m_pos[0] = lagrangePoint.m_pos[0];
    lagrangeCamTarget.m_pos[1] = lagrangePoint.m_pos[1];
    lagrangeCamTarget.m_pos[2] = lagrangePoint.m_pos[2];

    std::stringstream builder;
    builder << "Earth-Moon L" << (i + 1);
    lagrangeCamTarget.m_name = builder.str();
  }

  // Create ships
  EntitySystem::Ship& playerShip = m_entitySystem.getShip(m_playerShipId = m_entitySystem.makeShip());

  PhysicsSystem::ParticleBody& playerBody = m_physicsSystem.getParticleBody(playerShip.m_particleBodyId = m_physicsSystem.makeParticleBody());
  {
    playerBody.m_pos[0] = 0.0;
    playerBody.m_pos[1] = 0.0;
    playerBody.m_pos[2] = 1.3e7;

    playerBody.m_vel[0] = 5e3;
    playerBody.m_vel[1] = 0.0;
    playerBody.m_vel[2] = 0.0;

    playerBody.m_userAcc[0] = 0.0;
    playerBody.m_userAcc[1] = 0.0;
    playerBody.m_userAcc[2] = 0.0;
  }

  RenderSystem::Orbit& playerOrbit = m_renderSystem.getOrbit(playerShip.m_orbitId = m_renderSystem.makeOrbit());
  {
    playerOrbit.m_pos[0] = earthPosData[0];
    playerOrbit.m_pos[1] = earthPosData[1];
    playerOrbit.m_pos[2] = earthPosData[2];

    playerOrbit.m_col[0] = m_colB[2].x();
    playerOrbit.m_col[1] = m_colB[2].y();
    playerOrbit.m_col[2] = m_colB[2].z();
  }

  RenderSystem::Trail& playerTrail = m_renderSystem.getTrail(playerShip.m_trailId = m_renderSystem.makeTrail(5000.0, playerBody.m_pos, m_cameraSystem.getTarget(m_cameraTargetId).m_pos));
  {
    playerTrail.m_colOld[0] = m_colB[0].x();
    playerTrail.m_colOld[1] = m_colB[0].y();
    playerTrail.m_colOld[2] = m_colB[0].z();

    playerTrail.m_colNew[0] = m_colB[4].x();
    playerTrail.m_colNew[1] = m_colB[4].y();
    playerTrail.m_colNew[2] = m_colB[4].z();
  }

  RenderSystem::Point& playerPoint = m_renderSystem.getPoint(playerShip.m_pointId = m_renderSystem.makePoint());
  {
    playerPoint.m_pos[0] = playerBody.m_pos[0];
    playerPoint.m_pos[1] = playerBody.m_pos[1];
    playerPoint.m_pos[2] = playerBody.m_pos[2];

    playerPoint.m_col[0] = m_colB[4].x();
    playerPoint.m_col[1] = m_colB[4].y();
    playerPoint.m_col[2] = m_colB[4].z();
  }

  CameraSystem::Target& playerCamTarget = m_cameraSystem.getTarget(playerShip.m_cameraTargetId = m_cameraSystem.makeTarget());
  {
    playerCamTarget.m_pos[0] = playerBody.m_pos[0];
    playerCamTarget.m_pos[1] = playerBody.m_pos[1];
    playerCamTarget.m_pos[2] = playerBody.m_pos[2];

    playerCamTarget.m_name = std::string("Player");
  }

  EntitySystem::Ship& suspectShip = m_entitySystem.getShip(m_suspectShipId = m_entitySystem.makeShip());

  PhysicsSystem::ParticleBody& suspectBody = m_physicsSystem.getParticleBody(suspectShip.m_particleBodyId = m_physicsSystem.makeParticleBody());
  {
    suspectBody.m_pos[0] = 0.0;
    suspectBody.m_pos[1] = 0.0;
    suspectBody.m_pos[2] = 1.3e7;

    suspectBody.m_vel[0] = 5e3;
    suspectBody.m_vel[1] = 0.0;
    suspectBody.m_vel[2] = 0.0;

    suspectBody.m_userAcc[0] = 0.0;
    suspectBody.m_userAcc[1] = 0.0;
    suspectBody.m_userAcc[2] = 0.0;
  }

  RenderSystem::Orbit& suspectOrbit = m_renderSystem.getOrbit(suspectShip.m_orbitId = m_renderSystem.makeOrbit());
  {
    suspectOrbit.m_pos[0] = earthPosData[0];
    suspectOrbit.m_pos[1] = earthPosData[1];
    suspectOrbit.m_pos[2] = earthPosData[2];

    suspectOrbit.m_col[0] = m_colR[2].x();
    suspectOrbit.m_col[1] = m_colR[2].y();
    suspectOrbit.m_col[2] = m_colR[2].z();
  }

  RenderSystem::Trail& suspectTrail = m_renderSystem.getTrail(suspectShip.m_trailId = m_renderSystem.makeTrail(5000.0, suspectBody.m_pos, m_cameraSystem.getTarget(m_cameraTargetId).m_pos));
  {
    suspectTrail.m_colOld[0] = m_colR[0].x();
    suspectTrail.m_colOld[1] = m_colR[0].y();
    suspectTrail.m_colOld[2] = m_colR[0].z();

    suspectTrail.m_colNew[0] = m_colR[4].x();
    suspectTrail.m_colNew[1] = m_colR[4].y();
    suspectTrail.m_colNew[2] = m_colR[4].z();
  }

  RenderSystem::Point& suspectPoint = m_renderSystem.getPoint(suspectShip.m_pointId = m_renderSystem.makePoint());
  {
    suspectPoint.m_pos[0] = suspectBody.m_pos[0];
    suspectPoint.m_pos[1] = suspectBody.m_pos[1];
    suspectPoint.m_pos[2] = suspectBody.m_pos[2];

    suspectPoint.m_col[0] = m_colR[4].x();
    suspectPoint.m_col[1] = m_colR[4].y();
    suspectPoint.m_col[2] = m_colR[4].z();
  }

  CameraSystem::Target& suspectCamTarget = m_cameraSystem.getTarget(suspectShip.m_cameraTargetId = m_cameraSystem.makeTarget());
  {
    suspectCamTarget.m_pos[0] = suspectBody.m_pos[0];
    suspectCamTarget.m_pos[1] = suspectBody.m_pos[1];
    suspectCamTarget.m_pos[2] = suspectBody.m_pos[2];

    suspectCamTarget.m_name = std::string("Suspect");
  }

  // Perturb all the ship orbits
  // TODO this should update all the other positions too, or happen earlier!
  // TODO shouldn't iterate through IDs outside a system.
#if 0
  float* rnds = new float[6 * m_entitySystem.numShips()];
  UniformDistribution<float> dist(-1, +1);
  dist.Generate(&m_rnd, 6 * m_entitySystem.numShips(), &rnds[0]);
  for (int i = 0; i < m_entitySystem.numShips(); ++i)
  {
    PhysicsSystem::ParticleBody& shipBody = m_physicsSystem.getParticleBody(m_entitySystem.getShip(i).m_particleBodyId);
    // could round-trip to vector, or do it in components...
    shipBody.m_pos[0] += 6e4 * rnds[6*i  ];
    shipBody.m_pos[1] += 6e4 * rnds[6*i+1];
    shipBody.m_pos[2] += 6e4 * rnds[6*i+2];

    shipBody.m_vel[0] += 1e2 * rnds[6*i+3];
    shipBody.m_vel[1] += 1e2 * rnds[6*i+4];
    shipBody.m_vel[2] += 1e2 * rnds[6*i+5];
  }
  delete[] rnds;
#endif
}


void orApp::ShutdownState()
{
}

void orApp::HandleEvent(Event const& _event)
{
  /* TODO allow?
  if (_event.type == sf::Event::Resized)
  {
    glViewport(0, 0, _event.size.width, _event.size.height);
  }
  */

  SDL_LogWarn(SDL_LOG_CATEGORY_ERROR, "TODO NYI Event handling");

#if 0
  if (_event.type == sf::Event::MouseButtonPressed) {
    if (_event.mouseButton.button == sf::Mouse::Right) {
      m_inputMode = InputMode_RotateCamera;

      // When rotating the camera, hide the mouse cursor and center it. We'll then track how far it's moved off center each frame.
      sf::Vector2i const centerPos = sf::Vector2i(m_config.windowWidth/2, m_config.windowHeight/2);
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
    m_camParams.dist *= pow(0.9, _event.mouseWheel.delta);
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

    if (_event.key.code == sf::Keyboard::Add || _event.key.code == sf::Keyboard::Equal)
    {
      m_timeScale *= 2;
      if (m_timeScale > 1<<16) {
        m_timeScale = 1<<16;
      }
    }

    if (_event.key.code == sf::Keyboard::Subtract || _event.key.code == sf::Keyboard::Dash)
    {
      m_timeScale /= 2;
      if (m_timeScale < 1) {
        m_timeScale = 1;
      }
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
#endif
}

Vector3d orApp::CalcPlayerThrust(PhysicsSystem::ParticleBody const& playerBody)
{
  Vector3d const origin(m_physicsSystem.findSOIGravBody(playerBody).m_pos);
  Vector3d const playerPos(playerBody.m_pos);
  Vector3d const playerVel(playerBody.m_vel);

  // Calc acceleration due to gravity
  Vector3d const r = origin - playerPos;
  double const r_mag = r.norm();

  Vector3d const r_dir = r / r_mag;

  // Calc acceleration due to thrust
  double const thrustAccel = 10.0; // meters per second squared - TODO what is a realistic value?

  Vector3d thrustVec(0.0,0.0,0.0);

  Vector3d const fwd = playerVel.normalized(); // Prograde
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

void orApp::UpdateState_Bodies(double const dt)
{
  // Update player thrust
  PhysicsSystem::ParticleBody& playerShipBody = m_physicsSystem.getParticleBody(m_entitySystem.getShip(m_playerShipId).m_particleBodyId);
  Vector3d const userAcc = CalcPlayerThrust(playerShipBody);

  const double* const userAccData = userAcc.data();

  playerShipBody.m_userAcc[0] = userAccData[0];
  playerShipBody.m_userAcc[1] = userAccData[1];
  playerShipBody.m_userAcc[2] = userAccData[2];

  m_physicsSystem.update(m_integrationMethod, dt);

  m_entitySystem.update(dt, m_cameraSystem.getTarget(m_cameraTargetId).m_pos);

  // TODO eaghghgh not clear where these should live

  PhysicsSystem::GravBody& earthBody = m_physicsSystem.getGravBody(m_entitySystem.getPlanet(m_earthPlanetId).m_gravBodyId);
  EntitySystem::Moon& moon = m_entitySystem.getMoon(m_moonMoonId);
  PhysicsSystem::GravBody& moonBody = m_physicsSystem.getGravBody(moon.m_gravBodyId);

  Vector3d const earthPos(earthBody.m_pos);
  Vector3d const earthVel(earthBody.m_vel);
  Vector3d const moonPos(moonBody.m_pos);
  Vector3d const moonVel(moonBody.m_vel);

  const double* const moonPosData = moonPos.data();

  // Update the moon's label
  RenderSystem::Label3D& moonLabel3D = m_renderSystem.getLabel3D(moon.m_label3DId);
  moonLabel3D.m_pos[0] = moonPosData[0];
  moonLabel3D.m_pos[1] = moonPosData[1];
  moonLabel3D.m_pos[2] = moonPosData[2];

  // Update the earth-moon COM
  double const totalMass = earthBody.m_mass + moonBody.m_mass;

  Vector3d const comPos = (earthPos * earthBody.m_mass / totalMass) + (moonPos * moonBody.m_mass / totalMass);
  const double* const comPosData = comPos.data();

  EntitySystem::Poi& comPoi = m_entitySystem.getPoi(m_comPoiId);

  RenderSystem::Point& comPoint = m_renderSystem.getPoint(comPoi.m_pointId);
  {
    comPoint.m_pos[0] = comPosData[0];
    comPoint.m_pos[1] = comPosData[1];
    comPoint.m_pos[2] = comPosData[2];
  }

  CameraSystem::Target& comTarget = m_cameraSystem.getTarget(comPoi.m_cameraTargetId);
  {
    comTarget.m_pos[0] = comPosData[0];
    comTarget.m_pos[1] = comPosData[1];
    comTarget.m_pos[2] = comPosData[2];
  }

  // Update the earth-moon Lagrange points
  Vector3d const earthMoonVector = moonPos - earthPos;
  double const earthMoonOrbitRadius = earthMoonVector.norm();
  Vector3d const earthMoonDir = earthMoonVector / earthMoonOrbitRadius;
  double const massRatio = MOON_MASS / EARTH_MASS;
  double const r1 = earthMoonOrbitRadius * pow(massRatio / 3.0, 1.0/3.0);
  double const r3 = earthMoonOrbitRadius * (1.0 + (7.0/12.0) * massRatio); // extra 1.0 to make r3 a distand from Earth position rather than an offset from earthMoonOrbitRadius

  Vector3d lagrangePos[5];
  // Lagrange point 1
  lagrangePos[0] = moonPos - earthMoonDir * r1;
  // Lagrange point 2
  lagrangePos[1] = moonPos + earthMoonDir * r1;
  // Lagrange point 3
  lagrangePos[2] = earthPos - earthMoonDir * r3;

  // L4 and L5 are on the Moon's orbit, 60 degrees ahead and 60 degrees behind.
  Vector3d orbitAxis = moonVel.normalized().cross(earthMoonVector.normalized());
  Eigen::AngleAxisd rotation(M_TAU / 6.0, orbitAxis);
  // Lagrange point 4
  lagrangePos[3] = rotation           * earthMoonVector;
  // Lagrange point 5
  lagrangePos[4] = rotation.inverse() * earthMoonVector;

  for (int i = 0; i < 5; ++i) {
    EntitySystem::Poi& lagrangePoi = m_entitySystem.getPoi(m_lagrangePoiIds[i]);
    RenderSystem::Point& lagrangePoint = m_renderSystem.getPoint(lagrangePoi.m_pointId);
    CameraSystem::Target& lagrangeTarget = m_cameraSystem.getTarget(lagrangePoi.m_cameraTargetId);

    const double* const lagrangePos_data = lagrangePos[i].data();

    lagrangePoint.m_pos[0] = lagrangePos_data[0];
    lagrangePoint.m_pos[1] = lagrangePos_data[1];
    lagrangePoint.m_pos[2] = lagrangePos_data[2];

    lagrangeTarget.m_pos[0] = lagrangePos_data[0];
    lagrangeTarget.m_pos[1] = lagrangePos_data[1];
    lagrangeTarget.m_pos[2] = lagrangePos_data[2];
  }
}

Vector3d orApp::CamPosFromCamParams(OrbitalCamParams const& params)
{
  Eigen::AngleAxisd thetaRot(params.theta, Vector3d(0.0, 1.0, 0.0));
  Eigen::AngleAxisd phiRot(params.phi, Vector3d(1.0, 0.0, 0.0));

  Eigen::Affine3d mat;
  mat.setIdentity();
  mat.rotate(thetaRot).rotate(phiRot);

  return mat * Vector3d(0.0, 0.0, params.dist);
}

void orApp::UpdateState()
{
  double const dt = m_timeScale * Util::Min((double)Timer::PerfTimeToMillis(m_lastFrameDuration), 100.0) / 1000.0; // seconds

  if (!m_paused) {
    UpdateState_Bodies(dt);

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
    Vector3d const camTargetPos(camTarget.m_pos);

    Vector3d camPos;

    if (m_camMode == CameraMode_FirstPerson) {
      camPos = Vector3d(m_physicsSystem.getParticleBody(m_entitySystem.getShip(m_playerShipId).m_particleBodyId).m_pos);
    } else if (m_camMode == CameraMode_ThirdPerson) {
      camPos = CamPosFromCamParams(m_camParams) + camTargetPos;
    } else {
      assert(false);
    }

    CameraSystem::Camera& camera = m_cameraSystem.getCamera(m_cameraId);

    const double* const camPosData = camPos.data();

    camera.m_pos[0] = camPosData[0];
    camera.m_pos[1] = camPosData[1];
    camera.m_pos[2] = camPosData[2];
  }


  // Update debug text
  {
    PERFTIMER("DebugText");

    // Top of screen
    {
      std::ostringstream str;
      str.precision(3);
      str.flags(std::ios::right | std::ios::fixed);

      str << calendarDateFromSimTime(m_simTime) << "\n";

      str << "Time Scale: " << (int)m_timeScale << "\n";

      // str << "FPS: " << (int)(1000.0 / Timer::PerfTimeToMillis(m_lastFrameDuration)) << "\n";
      // str << "Cam Dist: " << m_camDist << "\n";
      // str << "Cam Theta:" << m_camTheta << "\n";
      // str << "Cam Phi:" << m_camPhi << "\n";
      // double const shipDist = (m_ships[0].m_physics.m_pos - m_ships[1].m_physics.m_pos).norm();
      // str << "Intership Distance:" << shipDist << "\n";
      // str << "Intership Distance: TODO\n";
      // str << "Integration Method: " << m_integrationMethod << "\n";

      // TODO: better double value text formatting
      // TODO: small visualisations for the angle etc values

      RenderSystem::Label2D& m_uiTextTopLabel2D = m_renderSystem.getLabel2D(m_uiTextTopLabel2DId);
      m_uiTextTopLabel2D.m_text = str.str();
    }

    // Bottom of screen
    {
      std::ostringstream str;
      str.precision(3);
      str.flags(std::ios::right | std::ios::fixed);

      CameraSystem::Target& camTarget = m_cameraSystem.getTarget(m_cameraTargetId);
      str << "Cam Target: " << camTarget.m_name << "\n";

      RenderSystem::Label2D& m_uiTextBottomLabel2D = m_renderSystem.getLabel2D(m_uiTextBottomLabel2DId);
      m_uiTextBottomLabel2D.m_text = str.str();
    }
  }
}

Vector3d lerp(Vector3d const& _x0, Vector3d const& _x1, double const _a) {
    return _x0 * (1 - _a) + _x1 * _a;
}

boost::posix_time::ptime orApp::posixTimeFromSimTime(float simTime) {
  using namespace boost::posix_time;
  using namespace boost::gregorian;
  typedef boost::posix_time::ptime posix_time;

  // Astronomical Epoch: 1200 hours, 1 January 2000
  posix_time epoch(date(2000, Jan, 1), hours(12));
  // Game start date: 1753 hours, Mar 15 2025 (Did I pick this for any reason?)
  posix_time gameStart(date(2025, Mar, 15), hours(1753));
  // Note: the (long) here limits us to ~68 years game time.
  // Should be enough, otherwise just need to keep adding seconds to the
  // dateTime to match the simTime.
  posix_time curDateTime = gameStart + seconds((long)simTime);
  return curDateTime;
}

std::string orApp::calendarDateFromSimTime(float simTime) {
  using namespace boost::posix_time;
  return to_simple_string(posixTimeFromSimTime(simTime));
}

double orApp::julianDateFromPosixTime(
  boost::posix_time::ptime const& ptime
) {
  using namespace boost::posix_time;
  using namespace boost::gregorian;
  typedef boost::posix_time::ptime posix_time;
  // wikipedia: posix_time = (julian_date - 2440587.5) * 86400
  // => (posix_time / 86400.0) + 2440587.5 = julian_date
  posix_time posix_epoch(date(1970, Jan, 1), hours(0));
  boost::posix_time::time_duration d = (ptime - posix_epoch);
  double posix_time_s = (double)d.ticks() / (double)d.ticks_per_second();
  return (posix_time_s / 86400.0) + 2440587.5;
}

// returns eccentric anomaly
// from http://ssd.jpl.nasa.gov/txt/aprx_pos_planets.pdf

// "If this iteration formula won't converge, the eccentricity is probably too close to one. Then you should instead use the formulae for near-parabolic or parabolic orbits."
// http://astro.if.ufrgs.br/trigesf/position.html
double orApp::computeEccentricAnomaly(
  double mean_anomaly_deg,
  double eccentricity_rad
) {
  double tolerance_deg = 10e-6;
  double eccentricity_deg = eccentricity_rad * (360.0 / M_TAU);
  double eccentric_anomaly_deg = mean_anomaly_deg + eccentricity_deg * sin(mean_anomaly_deg);
  double delta_mean_anomaly_deg = 0;
  double delta_eccentric_anomaly_deg = 0;
  do
  {
    double eccentric_anomaly_rad = eccentric_anomaly_deg * (M_TAU / 360.0);
    delta_mean_anomaly_deg = mean_anomaly_deg - (eccentric_anomaly_deg - eccentricity_deg * sin(eccentric_anomaly_rad));
    delta_eccentric_anomaly_deg = delta_mean_anomaly_deg / (1 - eccentricity_rad * cos(eccentric_anomaly_rad));
    eccentric_anomaly_deg += delta_eccentric_anomaly_deg;
  } while (delta_eccentric_anomaly_deg > tolerance_deg);

  return eccentric_anomaly_deg;
}

Eigen::Vector3d orApp::ephemerisFromKeplerianElements(
  KeplerianElements const& elements_t0,
  boost::posix_time::ptime const& ptime
) {
  // Compute time in centuries since J2000
  double julian_date = julianDateFromPosixTime(ptime);
  double t_C = (julian_date - 2451545.0) / 36525;

  // Update elements for ephemerides
  KeplerianElements e(elements_t0);
  e.semi_major_axis_AU += e.semi_major_axis_AU_per_C * t_C;
  e.eccentricity_rad += e.eccentricity_rad_per_C * t_C;
  e.inclination_deg += e.inclination_deg_per_C * t_C;
  e.mean_longitude_deg += e.mean_longitude_deg_per_C * t_C;
  e.longitude_of_perihelion_deg += e.longitude_of_perihelion_deg_per_C * t_C;
  e.longitude_of_ascending_node_deg += e.longitude_of_ascending_node_deg_per_C * t_C;

  // arg: argument
  double arg_of_perihelion_deg = e.longitude_of_perihelion_deg - e.longitude_of_ascending_node_deg;

  // NOTE assuming error_f needs deg->rad conversion, since all other angles in the paper needed it
  double error_f = e.error_f * t_C * (M_TAU / 360.0);

  double mean_anomaly_deg = e.mean_longitude_deg - e.longitude_of_perihelion_deg
    + e.error_b * t_C * t_C
    + e.error_c * cos(error_f)
    + e.error_s * sin(error_f);

  mean_anomaly_deg = Util::Wrap(mean_anomaly_deg, -180.0, +180.0);

  double eccentric_anomaly_deg = computeEccentricAnomaly(mean_anomaly_deg, e.eccentricity_rad);
  double eccentric_anomaly_rad = eccentric_anomaly_deg * (M_TAU / 360.0);

  double x_orbital = e.semi_major_axis_AU * (cos(eccentric_anomaly_rad) - e.eccentricity_rad);
  double y_orbital = e.semi_major_axis_AU * sqrt(1 - e.eccentricity_rad * e.eccentricity_rad) * sin(eccentric_anomaly_rad);
  double z_orbital = 0;

  Eigen::Vector3d r_orbital(x_orbital, y_orbital, z_orbital);

  Eigen::Matrix3d m;
  m = Eigen::AngleAxisd(-e.longitude_of_ascending_node_deg * (M_TAU / 360.0), Eigen::Vector3d::UnitX())
    * Eigen::AngleAxisd(-e.inclination_deg * (M_TAU / 360.0), Eigen::Vector3d::UnitY())
    * Eigen::AngleAxisd(-arg_of_perihelion_deg * (M_TAU / 360.0), Eigen::Vector3d::UnitZ());

  return m * r_orbital;
}

void orApp::RenderState()
{
  PERFTIMER("RenderState");

  // Projection matrix (GL_PROJECTION)
  double const minZ = 1e6; // meters
  double const maxZ = 1e11; // meters

  double const aspect = m_config.windowWidth / (double)m_config.windowHeight;
  Eigen::Matrix4d const projMatrix = m_cameraSystem.calcProjMatrix(m_cameraId, m_config.renderWidth, m_config.renderHeight, minZ, maxZ, aspect);

  // Camera matrix (GL_MODELVIEW)
  Vector3d up(0.0, 1.0, 0.0);
  Eigen::Matrix4d const camMatrix = m_cameraSystem.calcCameraMatrix(m_cameraId, m_cameraTargetId, up);

  // Used to translate a 3d position into a 2d framebuffer position
  Eigen::Matrix4d const screenMatrix = m_cameraSystem.calcScreenMatrix(m_config.renderWidth, m_config.renderHeight);

  RenderSystem::Colour clearCol = m_colG[0];

  m_renderSystem.render(m_frameBuffer, clearCol, minZ, screenMatrix, projMatrix, camMatrix);

  {
    PERFTIMER("PostEffect");

    // Render from 2D framebuffer to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_config.windowWidth, m_config.windowHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    float const scale = 1.0;
    float const uv_scale = 1.0;

    glBindTexture(GL_TEXTURE_2D, m_frameBuffer.colorTextureId);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-scale, -scale, 0.0);
    glTexCoord2f(uv_scale, 0.0);
    glVertex3f(+scale, -scale, 0.0);
    glTexCoord2f(uv_scale, uv_scale);
    glVertex3f(+scale, +scale, 0.0);
    glTexCoord2f(0.0, uv_scale);
    glVertex3f(-scale, +scale, 0.0);
    glEnd();

    glDisable(GL_TEXTURE_2D);
  }

  SDL_GL_SwapWindow(m_window);

  // printf("Frame Time: %04.1f ms Total Sim Time: %04.1f s \n", Timer::PerfTimeToMillis(m_lastFrameDuration), m_simTime / 1000);
}
