#include "orbitalSpaceApp.h"

#include "perftimer.h"

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

OrbitalSpaceApp::OrbitalSpaceApp():
  App(),
  m_rnd(1123LL),
  m_simTime(0.f),
  m_paused(false),
  m_singleStep(true),
  m_useWeights(true),
  m_wireframe(false),
  m_curStep(-1),
  m_camZ(-1000),
  m_camTheta(0.f)
{
}

OrbitalSpaceApp::~OrbitalSpaceApp()
{
}

void OrbitalSpaceApp::Run()
{
  while (m_running && m_curStep < NUM_STEPS)
  {
    Timer::PerfTime const frameStart = Timer::GetPerfTime();
    
    {
      PERFTIMER("PollEvents");
      PollEvents();
    }

    // Input handling
    sf::Mouse::setPosition(sf::Vector2i(m_config.width/2, m_config.height/2), *m_window);
    // TODO get delta
    // TODO hide pointer

    {
      PERFTIMER("UpdateState");
      UpdateState(STEP_SIZE_MS / 1000.f);
    }
    
    if (m_curStep % 10 == 0 || m_paused)
    {
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
    }
    
    m_lastFrameDuration = Timer::GetPerfTime() - frameStart;
  }
}

void OrbitalSpaceApp::InitRender()
{
  App::InitRender();

  // TODO
  m_config.width = 800;
  m_config.height = 600;
  
  sf::ContextSettings settings;
  settings.depthBits         = 24; // Request a 24 bits depth buffer
  settings.stencilBits       = 8;  // Request a 8 bits stencil buffer
  settings.antialiasingLevel = 2;  // Request 2 levels of antialiasing
  m_window = new sf::Window(sf::VideoMode(m_config.width, m_config.height, 32), "SFML OpenGL", sf::Style::Close, settings);
  
  m_window->setMouseCursorVisible(false);

  glViewport(0, 0, m_config.width, m_config.height);

  glClearColor(0, 0, 0, 0);
  glClearDepth(1.0f);
}

void OrbitalSpaceApp::ShutdownRender()
{
  App::ShutdownRender();
}

void OrbitalSpaceApp::InitState()
{
  App::InitState();
  
  m_alice.InitState(&m_rnd, m_world);
#if SSRM_SECTION == 4
  m_bob.InitState(&m_rnd, m_world);
#endif
  
  for (int i = 0; i < SimViz::World::NUM_SENSORS; ++i)
  {
	  m_sensorContact[i] = Vec2(9999.f, 9999.f);
  }
}

void OrbitalSpaceApp::ShutdownState()
{
  App::ShutdownState();
}

void OrbitalSpaceApp::HandleEvent(sf::Event const& _event)
{
  // TODO: individually toggle rendering of avg pose, std dev of pose, real pose, particles
  // TODO: print keys on startup
  /* TODO
  if(_event.type == SDL_QUIT) {
    m_running = false;
  } else if (_event.type == SDL_KEYDOWN && _event.key.keysym.sym == SDLK_p) {
    m_paused = !m_paused;
  } else if (_event.type == SDL_KEYDOWN && _event.key.keysym.sym == SDLK_t) {
    m_singleStep = true;
    m_paused = false;
  } else if (_event.type == SDL_KEYDOWN && _event.key.keysym.sym == SDLK_w) {
    m_wireframe = !m_wireframe;
  } else if (_event.type == SDL_KEYDOWN && _event.key.keysym.sym == SDLK_e) {
    m_useWeights = !m_useWeights;
  } else if (_event.type == SDL_KEYDOWN && _event.key.keysym.sym == SDLK_k) {
    m_alice.Kidnap(&m_rnd, m_world);
  }
 */
  /*
  if (_event.type == sf::Event::Resized)
  {
    glViewport(0, 0, _event.size.width, _event.size.height);
  }
  */

  if (_event.type == sf::Event::MouseWheelMoved)
  {
    m_camZ += 10.f * _event.mouseWheel.delta;
  }

  if (_event.type == sf::Event::KeyPressed)
  {
    if (_event.key.code == sf::Keyboard::Escape)
    {
      m_running = false;
    }
  }

  if (_event.type == sf::Event::Closed)
  {
    m_running = false;
  }
}

void OrbitalSpaceApp::UpdateState(float const _dt)
{
  if (!m_paused)
  {
    m_simTime += _dt;
	  m_curStep++;
	  SimViz::Motion aliceMotion;
    SimViz::Motion bobMotion;
	  {
      PERFTIMER("UpdateBehaviour");
      m_alice.UpdateMotion(&m_rnd, &aliceMotion);
#if SSRM_SECTION == 4
      m_bob.UpdateMotion(&m_rnd, &bobMotion);
#endif
    }
    {
      PERFTIMER("UpdateWorld");
      m_world.Update(&m_rnd, _dt);
    }
    {
      PERFTIMER("UpdateSim");
      // HACK - should split the update instead so we can use a more recent pose when sensing
#if SSRM_SECTION == 4
      Vec2 bobPos = m_bob.m_model.states.prevPoses[0].pos;
#else
      Vec2 bobPos(99999.f, 9999.f);
#endif
      m_alice.Update(&m_rnd, m_world, bobPos, aliceMotion, _dt);
#if SSRM_SECTION == 4
      m_bob.Update(&m_rnd, m_world, m_alice.m_model.states.prevPoses[0].pos, bobMotion, _dt);
#endif

#if SSRM_SECTION == 4
      // HACK
      Circle2D a;
      a.center = m_alice.m_model.states.currPoses[0].pos;
      a.radius = 25.f;
      Circle2D b;
      b.center = m_bob.m_model.states.currPoses[0].pos;
      b.radius = 25.f;
      CircleTestResult result;
      TestCircleCircle(a, b, &result);
      if (result.col)
      {
        // Resolve intersection
        Vec2 shift = (result.dist - 26.f) * result.colNormal;
        m_alice.m_model.states.currPoses[0].pos = m_alice.m_model.states.currPoses[0].pos + shift;
        m_bob.m_model.states.currPoses[0].pos = m_bob.m_model.states.currPoses[0].pos - shift;

        // Avoidance
        m_alice.m_behaviour = SimViz::RobotAgent::Behaviour_Turn;
        m_bob.m_behaviour = SimViz::RobotAgent::Behaviour_Turn;
      }
#endif
    }
    {
      PERFTIMER("UpdateModel");
    }

    {
      PERFTIMER("CalcStats");
	    
      m_poseHist[m_curStep] = m_alice.m_model.states.currPoses[0];

      for (int i = 0; i < SimViz::World::NUM_SENSORS; ++i)
      {
        m_sensorHist[m_curStep][i] = m_alice.m_model.readings.sensorDist[i];
      }
    }
  }

  if (m_singleStep)
  {
    m_singleStep = false;
    m_paused = true;
  }
}

void OrbitalSpaceApp::DrawWireSphere(float const radius, int const slices, int const stacks)
{
    int curStack, curSlice;

    /* Adjust z and radius as stacks and slices are drawn. */

    double r;
    double x,y,z;

    double const sliceInc = 2*M_PI / (-slices);
    double const stackInc = 2*M_PI / (2*stacks);
    /* Draw a line loop for each stack */

    for (curStack = 1; curStack < stacks; curStack++)
    {
        y = cos( curStack * stackInc );
        r = sin( curStack * stackInc );

        glBegin(GL_LINE_LOOP);

            for(curSlice = 0; curSlice <= slices; curSlice++)
            {
                x = cos( curSlice * sliceInc );
                z = sin( curSlice * sliceInc );

                glNormal3d(x,y,z);
                glVertex3d(x*r*radius,y*radius,z*r*radius);
            }

        glEnd();
    }

    /* Draw a line loop for each slice */
    for (curSlice = 0; curSlice < slices; curSlice++)
    {
        glBegin(GL_LINE_STRIP);

            for (curStack = 1; curStack < stacks; curStack++)
            {
                x = cos( curSlice * sliceInc ) * sin( curStack * stackInc );
                z = sin( curSlice * sliceInc ) * sin( curStack * stackInc );
                y = cos( curStack * stackInc );

                glNormal3d(x,y,z);
                glVertex3d(x*radius,y*radius,z*radius);
            }

        glEnd();
    }
}

void OrbitalSpaceApp::RenderState()
{
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  // Units: cm
  float const aspect = m_config.width / (float)m_config.height;
  float const height = 500.f; // cm
  float const width = height * aspect;
  Vec2 size(width, height);
  Vec2 tl = -.5f * size;
  Vec2 br = .5f * size;

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  float const fov = 35.f;
  gluPerspective(fov, aspect, 1.0f, 10000.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  Vec3 eye(0.0, 0.0, m_camZ);
  Vec3 focus(0.0, 0.0, 0.0);
  Vec3 up(0.0, 1.0, 0.0);

  glRotatef(m_camTheta, 0.0f, 1.0f, 0.0f);

  gluLookAt(eye.m_x, eye.m_y, eye.m_z,
            focus.m_x, focus.m_y, focus.m_z,
            up.m_x, up.m_y, up.m_z);

  glEnable(GL_TEXTURE_2D);
  
  glLineWidth(1);

  if (m_wireframe)
  {
    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
  }
  else
  {
    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
  }
  
  float const realW = 1.f;
  Vec3 const realCol(1.f, 1.f, 0.f);
  Vec3 const obstacleCol(0.f, 1.f, 0.f);
  Vec3 const avgCol(0.f, 1.f, 1.f);
  {
    PERFTIMER("DrawRobots");
  
    Vec3 const frontCol(1.f, 0.f, 0.f);
    Vec3 const backCol(0.f, 0.f, 1.f);
    Vec3 const lineCol(0.f, 1.f, 0.f);
    
    DrawRobots(1, &m_alice.m_model.states.currPoses[0], &realW, realCol, realCol);

#if SSRM_SECTION == 4
    DrawRobots(1, &m_bob.m_model.states.currPoses[0], &realW, obstacleCol, obstacleCol);
#endif
    
    // ~TODO~ skeletons :V
    for (int i = 0; i < SimViz::World::NUM_SENSORS; ++i)
    {
      DrawSensors(1, &m_alice.m_model.states.currPoses[0], &realW, &m_world.m_sensorPose[i], realCol);
    }
  }
  
  {
    PERFTIMER("DrawWalls");
  
    DrawWalls(SimViz::World::NUM_WALLS, &m_world.m_wall[0]);
  }
  
  {
    PERFTIMER("UpdateContacts")
    // TODO needs robot sensor poses
    for (int i = 0; i < SimViz::World::NUM_SENSORS; ++i)
    {
      UpdateContacts(1, m_world.m_sensorPose[i], &m_alice.m_model.states.currPoses[0], &m_alice.m_model.readings.sensorDist[i], &m_sensorContact[i]);
    }
  }

  {
    PERFTIMER("DrawPoints");

    Vec3 const pointCol(1.f, 0.f, 1.f);
    
    for (int i = 0; i < SimViz::World::NUM_SENSORS; ++i)
    {
      DrawPoints(1, &m_sensorContact[i], &realW, pointCol);
    }
  }

  {
    PERFTIMER("DrawTrail");

    Vec3 const trailCol(1.f, 0.f, 1.f);
    
    DrawTrail(m_curStep, &m_poseHist[0], trailCol);
  }
  
  DrawWireSphere(100.0, 32, 32);

  printf("Frame Time: %04.1f ms Total Sim Time: %04.1f s Tick: %04d \n", Timer::PerfTimeToMillis(m_lastFrameDuration), m_simTime, m_curStep);
}

void OrbitalSpaceApp::UpdateContacts(int const _n, Pose const& _sensorPose, Pose const* const _poses, float const* const _dists, Vec2* const o_points)
{
  for (int i = 0; i < _n; ++i)
  {
    Pose const sensorPose = _poses[i].WorldFromLocal(_sensorPose);
    o_points[i] = sensorPose.pos + Vec2::FromDirLen(sensorPose.dir, _dists[i]);
  }
}

float OrbitalSpaceApp::AlphaFromProb(float const _p)
{
  if (m_useWeights)
  {
    return .97f * logf(_p + 1.f)/logf(2.f) + .03f;
  }
  else
  {
    return 1.f;
  }
}

void OrbitalSpaceApp::SetDrawColour(Vec3 const _c)
{
  glColor3f(_c.m_x, _c.m_y, _c.m_z);
}

void OrbitalSpaceApp::DrawRobots(int const _n, Pose const* const _poses, float const* const _weights, Vec3 const& _frontCol, Vec3 const& _backCol)
{
  float const radius = 25.f;
  enum { SEGMENTS = 32 };

  for (int i = 0; i < _n; ++i)
  {
    glPushMatrix();
    glTranslatef( _poses[i].pos.m_x, _poses[i].pos.m_y, 0.f );
    glRotatef( _poses[i].dir * 180.f / (float)M_PI, 0.f, 0.f, 1.f );
    
    glBegin(GL_LINE_LOOP);
      
    float const weight = AlphaFromProb(_weights[i]);
      
    SetDrawColour(weight * _frontCol);
      
    Vec2 radiusV(radius, 0.f);
    for (int i = 0; i < SEGMENTS; ++i)
    {
      Vec2 p = radiusV.RotatedBy(i * 2.f * (float)M_PI / SEGMENTS);

      glVertex3f(p.m_x, p.m_y, 0);
    }
         
    glEnd();
      
    glPopMatrix();
  }
}

void OrbitalSpaceApp::DrawSensors(int const _n, Pose const* const _robotPoses, float const* const _weights, Pose const* const _sensorPose, Vec3 const& _lineCol)
{
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE); // Additive blending

  glBegin(GL_LINES);
    for (int i = 0; i < _n; ++i)
    {
      // glPushMatrix();
      // glTranslatef( _poses[i].pos.m_x, _poses[i].pos.m_y, 0 );
      // glRotatef( _poses[i].dir * 180 / M_PI, 0, 0, 1 );
      float const weight = AlphaFromProb(_weights[i]);
      
      {
        Pose sensorPose = _robotPoses[i].WorldFromLocal(*_sensorPose);

        Vec2 const s = sensorPose.pos;
        Vec2 const e = s + Vec2::FromDirLen(sensorPose.dir, 100);

        SetDrawColour(weight * _lineCol);
        glVertex3f(s.m_x, s.m_y, 0);
        glVertex3f(e.m_x, e.m_y, 0);
      }
      // glPopMatrix();
    }
  glEnd();

  glDisable(GL_BLEND);
}

void OrbitalSpaceApp::DrawWalls(int const _n, SimViz::Wall const* const _walls)
{
  glBegin(GL_QUADS);
  glColor3f(0, 1, 0);

  for (int i = 0; i < _n; ++i)
  {
    SimViz::Wall wall = _walls[i];
    Vec2 const p1 = wall.start;
    Vec2 const p2 = wall.end;
    Vec2 major = p2 - p1;
    Vec2 minor = Vec2(major.m_y, -major.m_x);
    minor /= minor.GetLength();
    minor *= 5.f;

    Vec2 const mid = .5f * (p1 + p2);

    Vec2 tl = mid + .5f * major + .5f * minor;
    Vec2 bl = mid + .5f * major - .5f * minor;
    Vec2 tr = mid - .5f * major + .5f * minor;
    Vec2 br = mid - .5f * major - .5f * minor;

    glVertex3f(tl.m_x, tl.m_y, 0);
    glVertex3f(tr.m_x, tr.m_y, 0);
    glVertex3f(br.m_x, br.m_y, 0);
    glVertex3f(bl.m_x, bl.m_y, 0);
  }
  glEnd();
}

void OrbitalSpaceApp::DrawPoints(int const _n, Vec2 const* const _points, float const* const _weights, Vec3 const _col)
{
#if 0
  // Dots
  glPointSize(3.0f);
  glBegin(GL_POINTS);
    glColor3f(1, 0, 1);
    glVertex3f(_p.m_x, _p.m_y, 0);
  glEnd();
#else
  // Crosses
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE); // Additive blending

  glBegin(GL_LINES);
    for (int i = 0; i < _n; ++i)
    {
      float const weight = AlphaFromProb(_weights[i]);
      Vec2 p = _points[i];
      
      SetDrawColour(weight * _col);
      glVertex3f(p.m_x - 3.f, p.m_y, 0);
      glVertex3f(p.m_x + 3.f, p.m_y, 0);

      glVertex3f(p.m_x, p.m_y - 3.f, 0);
      glVertex3f(p.m_x, p.m_y + 3.f, 0);
    }
  glEnd();

  glDisable(GL_BLEND);
#endif
}

void OrbitalSpaceApp::DrawTrail(int const _n, Pose const* const _poses, Vec3 const _col)
{
  glBegin(GL_LINE_STRIP);
    SetDrawColour(_col);
    for (int i = 0; i < _n; ++i)
    {
      Pose p = _poses[i];
       
      glVertex3f(p.pos.m_x, p.pos.m_y, 0);
    }
  glEnd();
}