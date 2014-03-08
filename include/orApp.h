/*
 * File:   ssrm1app.h
 * Author: s1149322
 *
 * Created on 12 December 2011, 17:11
 */

#ifndef ORBITALSPACEAPP_H
#define	ORBITALSPACEAPP_H

// STL
#include <vector>

// Boost

#include "boost_begin.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include "boost_end.h"

// Eigen

#include <Eigen/Geometry>

// Old style libs

#include "util.h"
#include "rnd.h"
#include "timer.h"

// New style libs

#include "orStd.h"
#include "orMath.h"
#include "orGfx.h"

// Systems

#include "orRender.h"
#include "orPhysics.h"
#include "orCamera.h"
#include "orEntity.h"

// TODO forward decl for SDL_GLContext?

#include "SDL_video.h"

// Code outside the system should refer to the instances only by opaque Id - not iterate over the collection (when needed this should happen internally in the system)

struct SDL_Window;

class orApp
{
public:
  struct Config {
    int windowWidth;
    int windowHeight;
    int renderWidth;
    int renderHeight;
  };

  // Eigen::Vectors are SSE magic.
  // Want those and also some plain old data types.
  struct VectorData2d {
    double x;
    double y;
  }; // TODO replace with appropriate type

  orApp(Config const& config);
  ~orApp();

public:
  void Run();

private:
  void Init();
    void InitRender();
    void InitState();

  void Shutdown();
    void ShutdownState();
    void ShutdownRender();

  void RunOneStep();

  void PollEvents();
    struct Event {}; // TODO replace with appropriate type
    void HandleEvent(Event const& _event);

  void HandleInput();

  void UpdateState();
    void UpdateState_Bodies(double const dt);

  void RenderState();

private:
  Vector3d CalcPlayerThrust(PhysicsSystem::ParticleBody const& playerBody);

private:
  enum AppScreen { Screen_Title, Screen_Level } m_appScreen;

  class TitleScreen {
    void Init();
    void Shutdown();
  } m_titleScreen;

  class MainLevel {
    void Init();
    void Shutdown();
  } m_mainLevel;

  Timer::PerfTime m_lastFrameDuration;
  bool m_running;

  Rnd64 m_rnd;

  double m_simTime;

  // TODO compute coordinates according to JPL formulas
  // TODO convert to SI units
  struct KeplerianElements {
    // AU: astrononmical unit
    // C: century
    // deg: degree
    double semi_major_axis_AU;
    double semi_major_axis_AU_per_C;
    double eccentricity_rad;
    double eccentricity_rad_per_C;
    double inclination_deg;
    double inclination_deg_per_C;
    double mean_longitude_deg;
    double mean_longitude_deg_per_C;
    double longitude_of_perihelion_deg;
    double longitude_of_perihelion_deg_per_C;
    double longitude_of_ascending_node_deg;
    double longitude_of_ascending_node_deg_per_C;

    double error_b;
    double error_c;
    double error_s;
    double error_f;
  };

  struct Ephemeris {
    // TODO
  };

  static double computeEccentricAnomaly(double mean_anomaly_deg, double eccentricity_rad );

  static Eigen::Vector3d ephemerisFromKeplerianElements(KeplerianElements const& elements, boost::posix_time::ptime const& time);

  static double julianDateFromPosixTime(boost::posix_time::ptime const& ptime);
  static boost::posix_time::ptime posixTimeFromSimTime(float simTime);
  static std::string calendarDateFromSimTime(float simTime);

  Config m_config;

  // Simulation options
  bool m_paused;
  bool m_singleStep;

  //// Camera ////

  CameraSystem m_cameraSystem;

  enum CameraMode {
    CameraMode_FirstPerson = 0,
    CameraMode_ThirdPerson = 1
  };
  CameraMode m_camMode;

  int m_cameraId;
  int m_cameraTargetId;

  struct OrbitalCamParams {
    OrbitalCamParams(double _dist) : dist(_dist), theta(0), phi(0) {}
    double dist;
    double theta;
    double phi;
  };

  OrbitalCamParams m_camParams;

  Vector3d CamPosFromCamParams(OrbitalCamParams const& params);

  //// Rendering ////

  RenderSystem m_renderSystem;

  int m_uiTextTopLabel2DId;
  int m_uiTextBottomLabel2DId;

  enum {PALETTE_SIZE = 5};

  // Lazy
  RenderSystem::Colour m_colG[PALETTE_SIZE];
  RenderSystem::Colour m_colR[PALETTE_SIZE];
  RenderSystem::Colour m_colB[PALETTE_SIZE];

  double m_lightDir[3];

  // TODO make into an id-handle thing
  RenderSystem::FrameBuffer m_frameBuffer;

  // Rendering options
  bool m_wireframe;

  //// Physics ////

  PhysicsSystem m_physicsSystem;

  double m_timeScale;
  PhysicsSystem::IntegrationMethod m_integrationMethod;

  //// Entities ////

  EntitySystem m_entitySystem;

  int m_playerShipId;
  int m_suspectShipId;

  int m_earthPlanetId;
  int m_moonMoonId;

  int m_comPoiId;
  int m_lagrangePoiIds[5];

  // Input

  enum InputMode {
    InputMode_Default = 0,
    InputMode_RotateCamera = 1
  };
  InputMode m_inputMode;

  enum Thrusters
  {
    ThrustFwd = 1 << 0,
    ThrustBack = 1 << 1,
    ThrustLeft = 1 << 2,
    ThrustRight = 1 << 3,
    ThrustUp = 1 << 4,
    ThrustDown = 1 << 5
  };

  uint32_t m_thrusters;

  bool m_hasFocus;
  SDL_Window* m_window;
  SDL_GLContext m_gl_context;

  VectorData2d m_savedMousePos;
  struct Music {}; // TODO replace with appropriate type
  Music* m_music;
};

#endif	/* ORBITALSPACEAPP_H */

