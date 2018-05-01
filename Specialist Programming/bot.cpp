#include <cmath>

#include "bot.h"
#include "dynamicobjects.h"
#include "staticmap.h"
#include "renderer.h"
#include "mydrawengine.h"
#include "navMesh.h"
#include "AllStates.h"
			

float Bot::GetDistance(Vector2D _first, Vector2D _second)
{
	float x, y, answer;
	// Uses pythagorous
	x = _first.XValue - _second.XValue;
	y = _first.YValue - _second.YValue;
	answer = sqrt((x * x) + (y * y));
	return answer;
}

void Bot::Update(float frametime)
{
	if(m_iOwnTeamNumber==0)
		ProcessAI();
	else
		ProcessAIBadly();

	// Check for respawn
	if(this->m_dTimeToRespawn>0)
	{
		m_dTimeToRespawn-=frametime;
		this->m_Velocity.set(0,0);
		if(m_dTimeToRespawn<=0)
		{
			PlaceAt(StaticMap::GetInstance()->GetSpawnPoint(m_iOwnTeamNumber));
      Reload();
		}
	}
	else
	{
		// Apply physics ***************************************************************
		
		// Disable movement for aiming bot
		if(this->m_bAiming == true)
		{
			m_Acceleration = -m_Velocity.unitVector()*MAXIMUMSPEED;
		}

		// Clamp acceleration to maximum
		if(m_Acceleration.magnitude()>MAXIMUMACCELERATION)
		{
			m_Acceleration=m_Acceleration.unitVector()*MAXIMUMACCELERATION;
		}

		// Accelerate
		m_Velocity+=m_Acceleration*frametime;

		// Clamp speed to maximum
		if(m_Velocity.magnitude()>MAXIMUMSPEED)
		{
			m_Velocity=m_Velocity.unitVector()*MAXIMUMSPEED;
		}

		if(m_Velocity.magnitude()>10.0)
		{
			m_dDirection = m_Velocity.angle();
		}

		// Move
		m_Position+=m_Velocity*frametime;

		// Check for collision
		Circle2D pos(m_Position, 8);

		if(StaticMap::GetInstance()->IsInsideBlock(pos))
		{
			// Collided. Move back to where you were.
			m_Position-=m_Velocity*frametime;
			m_Velocity.set(0,0);
		}

    // Check for resupply
    if ((StaticMap::GetInstance()->GetClosestResupplyLocation(m_Position) - m_Position).magnitude()<20.0f)
    {
      if (m_iAmmo <= 0 )
      {
        Renderer::GetInstance()->ShowReload(m_Position);
      }
      Reload();
    }

		// Handle shooting ***********************************************

		if(	m_dTimeToCoolDown>0)
			m_dTimeToCoolDown-=frametime;

		if(m_bAiming == true)
		{
			// If target is not valid
			if(m_iAimingAtTeam<0 ||m_iAimingAtTeam>=NUMTEAMS 
				||m_iAimingAtBot<0 ||m_iAimingAtBot>=NUMBOTSPERTEAM  )
			{
				m_bAiming = false;
				m_dAccuracy =0;
			}
			// else if target is dead
			else if(!DynamicObjects::GetInstance()->GetBot(m_iAimingAtTeam, m_iAimingAtBot).IsAlive())
			{
				m_bAiming = false;
				m_dAccuracy =0;
			}
      else if (m_dTimeToCoolDown>0 || m_iAmmo <= 0)
			{
				// Can't shoot. Waiting to cool down or no ammo
				Bot& targetBot = DynamicObjects::GetInstance()->
												GetBot(m_iAimingAtTeam, m_iAimingAtBot);
				if(m_Velocity.magnitude()<=10.0)
				{
					m_dDirection = (targetBot.m_Position-m_Position).angle();
				}
			}
			else		// Valid target
			{
				Bot& targetBot = DynamicObjects::GetInstance()->
												GetBot(m_iAimingAtTeam, m_iAimingAtBot);
				
				if(m_Velocity.magnitude()<=10.0)
				{
					m_dDirection = (targetBot.m_Position-m_Position).angle();
				}

				// Do we have line of sight?
				if(StaticMap::GetInstance()->IsLineOfSight(this->m_Position, targetBot.m_Position))
				{
					float range = (targetBot.m_Position-m_Position).magnitude();
					
					if(range<1.0)
						range = 1.0;		// Probably aiming at yourself
											// Otherwise suicide causes a divide by zero error.
											// That's the real reason why the church is against it.

					float peakAccuracy = sqrt(ACCURATERANGE / range);
					
					// Only gain accuracy if nearly stopped
					if(m_Velocity.magnitude()<MAXBOTSPEED/3)
					{
						m_dAccuracy += (peakAccuracy - m_dAccuracy)*frametime;
					}

					if(m_bFiring==true && m_dTimeToCoolDown<=0)
					{
						// Take the shot
						m_bFiring = false;
            m_iAmmo--;
						m_dTimeToCoolDown = TIMEBETWEENSHOTS;
						int damage=0;

						while(damage<100 && (rand()%1000) < int(m_dAccuracy*1000))
						{
							damage+=20;
						}

						targetBot.TakeDamage(damage);

						m_dAccuracy/=2.0;

						Vector2D tgt = targetBot.m_Position;
						if(damage==0)	// you missed
						{
							// Make it look like a miss
							tgt+=Vector2D(rand()%30-15.0f, rand()%30-15.0f);
						}
						Renderer::GetInstance()->AddShot(m_Position, tgt);
						Renderer::GetInstance()->AddBloodSpray(targetBot.m_Position, targetBot.m_Position-m_Position, damage/5);
					}

				}
				else		// No line of sight
				{
					m_bAiming = false;
					m_dAccuracy =0;				
				}

			}			// End valid target
		}
		else			// Not aiming
		{
			m_dAccuracy =0;
		}
	}
}

void Bot::Reload()
{
  m_iAmmo = MAXAMMO;
}

Vector2D Bot::GetLocation()
{
	return m_Position;
}

float Bot::GetDirection()
{
	return this->m_dDirection;
}

Vector2D Bot::GetVelocity()
{
	return m_Velocity;
}

void Bot::SetOwnNumbers(int teamNo, int botNo)
{
	m_iOwnTeamNumber = teamNo;
	m_iOwnBotNumber = botNo;
}

void Bot::PlaceAt(Vector2D position)
{
	m_Position = position;	// Current world coordinates
	m_Velocity.set(0,0);	// Current velocity
	m_dDirection=0;			// Direction. Mainly useful when stationary
	m_bAiming=false;		// If true, bot is aiming and cannot move
	m_dTimeToCoolDown=0;	// Countdown until the time the bot can shoot again
	m_dTimeToRespawn=0;		// Countdown until the bot can respawn. If zero or below, bot is alive
	m_Acceleration.set(0,0);
	m_bFiring=false;
	m_dAccuracy =0;
	m_iHealth=100;
}

bool Bot::IsAlive()
{
	if(m_dTimeToRespawn<=0)
		return true;
	else
		return false;
}

void Bot::ChangeState(State<Bot>* _state)
{
	if (_state)
	{
		m_myState->Exit(this);
		m_myState = _state;
		m_myState->Enter(this);
	}
}

Bot::Bot()
{
	// I suggest you do nothing here.
	// Remember that the rest of the world may not have been created yet.
	PlaceAt(Vector2D(0,0));		// Places bot at a default location
	m_dTimeToRespawn=1;
}


int Bot::GetHealth()
{
	return m_iHealth;
}

double Bot::GetAccuracy()
{	
	if(m_bAiming== true)
		return m_dAccuracy;
	else
		return 0;
}

void Bot::SetTarget(int targetTeamNo, int targetBotNo)
{

	if(m_iAimingAtTeam!=targetTeamNo || m_iAimingAtBot!=targetBotNo)
	{

		m_iAimingAtTeam = targetTeamNo;

		m_iAimingAtBot = targetBotNo;

		m_dAccuracy=0;
	}

	m_bAiming = true;
}

// Stops the bot from aiming, so it can move again
void Bot::StopAiming()
{
	m_bAiming = false;
}

// Call this to set the bot to shoot, if it can.
// Once a shot it taken, the bot will not shoot again unless told to do so.
void Bot::Shoot()
{
	m_bFiring = true;
}

// Returns the number of the team of the bot being aimed at.
// Returns a negative number if no bot is being aimed at.
int Bot::GetTargetTeam()
{
	if(m_bAiming== true)
		return m_iAimingAtTeam;
	else
		return -1;
}

// Returns the number of the bot being aimed at.
// Returns a negative number if no bot is being aimed at.
int Bot::GetTargetBot()
{
	if(m_bAiming== true)
		return m_iAimingAtBot;
	else
		return -1;
}

void Bot::TakeDamage(int amount)
{
	m_iHealth-=amount;

	if(m_iHealth<=0 && m_dTimeToRespawn<=0)
	{
		m_dTimeToRespawn = RESPAWNTIME;
	}

	// Being shot at puts you off your aim. 
	// Even more so if you are hit

	if(amount>0)
		m_dAccuracy=0;
	else
		m_dAccuracy/=2;
}

// ****************************************************************************
// This is your function. Use it to set up any states at the beginning of the game
// and analyse the map.
// Remember that bots have not spawned yet, so will not be in their
// starting positions.
// Eventually, this will contain very little code - it just sets a starting state
// and calls methods to analyse the map
void Bot::StartAI()
{
	// Checks to see if the BOT is users team or not

	// Creates a new behaviour and gives it to the bot
	behaviour* myBehaviour = new behaviour;
	m_behaviour = myBehaviour;
	m_behaviour->SetOwner(this);
	// Lets member variable in behaviour point to this bot so positions can be used
	//m_behaviour->m_bot = this;


	Vector2D start = Vector2D(-1400.0f, 0.0f);
	//Vector2D finish = Vector2D(0.0f, 0.0f);
	Vector2D finish = Graph::GetInstance()->GetRandomNode()->m_position;
	

	m_behaviour->SetPath(Graph::GetInstance()->PathFind(start, finish));
	//m_currentPath = DynamicObjects::GetInstance()->GetGraph().PathFind(start, finish);
	

	//m_myGraph.AnalyseMap();

}

// This is your function. Use it to set the orders for the bot.
// Will be called once each frame from Update
// Remember this will be called even if the bot is currently dead
// Eventually, this will contain very little code - it just runs the state
void Bot::ProcessAI()
{
	// Update bots behaviour variables at the start
	if (IsAlive())
	{
		if (m_behaviour->GetCurrentState() == nullptr)
			m_behaviour->SetCurrentState(Capture::GetInstance());

		m_behaviour->GetCurrentState()->Update(this);
		//m_behaviour->update(this);
		//m_Acceleration += m_behaviour->FollowPath();

	}
	else
		m_behaviour->SetCurrentState(nullptr);


	// Update Feeler (checks for collision) and check for wall collision
	//m_feeler.PlaceAt(m_Position + 10.0f * m_Velocity.unitVector(), 50.0f);
	//if (StaticMap::GetInstance()->IsInsideBlock(m_feeler))
	//m_Acceleration += m_behaviour->AvoidWall(m_feeler);








	if (m_Velocity.magnitude() > MAXBOTSPEED)
	{
		m_Velocity = m_Velocity.unitVector();
		m_Velocity *= MAXBOTSPEED;
	}

	// Check for bot collision
	//m_Acceleration += m_behaviour->AvoidBots(TeamBotLocations);


	// UPDATE VELOCITY AT THE END
	m_Velocity += m_Acceleration;






	// DEBUG DRAWING :: COMMENT OUT WHERE NECASARY
	// DRAW FEELERS
	//MyDrawEngine::GetInstance()->FillCircle(m_feeler.GetCentre(),m_feeler.GetRadius(), MyDrawEngine::WHITE);
	//MyDrawEngine::GetInstance()->WriteInt(Vector2D(0.0f,0.0f), DynamicObjects::GetInstance()->GetGraph().GetDistance(m_Position, DynamicObjects::GetInstance()->GetGraph().m_NodeVector.at(980).m_position),MyDrawEngine::WHITE);
	//if(m_iOwnBotNumber == 0)
	//DynamicObjects::GetInstance()->GetGraph().DrawNodes();
	//DynamicObjects::GetInstance()->GetGraph().DrawEdges();
	//MyDrawEngine::GetInstance()->WriteInt( Vector2D(0, 0), m_currentPath.size(), MyDrawEngine::WHITE);

}


void Bot::ProcessAIBadly()
{
	// This is all placeholder code.
	// Delete it all and write your own

	DominationPoint targetDP = DynamicObjects::GetInstance()
		->GetDominationPoint(m_iOwnBotNumber%3);
	
	// Find closest enemy
	int closestEnemy = 0;
	double range = (DynamicObjects::GetInstance()->GetBot
		(1-m_iOwnTeamNumber, 0).m_Position - m_Position).magnitude();
  if (DynamicObjects::GetInstance()->GetBot(1 - m_iOwnTeamNumber, 0).m_iHealth < 0)
    range += 1000;

	for(int i=1;i<NUMBOTSPERTEAM;i++)
	{
		double nextRange = (DynamicObjects::GetInstance()->GetBot
			(1-m_iOwnTeamNumber, i).m_Position - m_Position).magnitude();
    if (DynamicObjects::GetInstance()->GetBot(1 - m_iOwnTeamNumber, 0).m_iHealth < 0)
      nextRange += 1000;
		if(nextRange<range)
		{
			closestEnemy = i;
			range=nextRange;
		}
	}

	Bot& targetEnemy = DynamicObjects::GetInstance()->
		GetBot(1-m_iOwnTeamNumber, closestEnemy);

  Circle2D loc;
  loc.PlaceAt(m_Position, 30);
  
  // Out of ammo?
  if (m_iAmmo<=0)
  {
    // Find closest resupply
    Vector2D closestSupply = StaticMap::GetInstance()->GetClosestResupplyLocation(m_Position);

    if (StaticMap::GetInstance()->IsLineOfSight(m_Position, closestSupply))
    {
      targetPoint = closestSupply;
    }
    else
    {
      // Pick a random point
      Vector2D randomPoint(rand() % 4000 - 2000.0f, rand() % 4000 - 2000.0f);

      // Is it better than current target point?
      double currentValue = (m_Position - targetPoint).magnitude()
        + (closestSupply - targetPoint).magnitude()*1.2;
      if (!StaticMap::GetInstance()->IsLineOfSight(m_Position, targetPoint))
      {
        currentValue += 2500;
      }
      if (!StaticMap::GetInstance()->IsLineOfSight(closestSupply, targetPoint))
      {
        currentValue += 1000;
      }
      // Is it too close to a block?
      Circle2D loc;
      loc.PlaceAt(randomPoint, 20);
      if (StaticMap::GetInstance()->IsInsideBlock(loc))
      {
        currentValue += 500;
      }

      double randomValue = (m_Position - randomPoint).magnitude()
        + (closestSupply - randomPoint).magnitude()*1.2;
      if (!StaticMap::GetInstance()->IsLineOfSight(m_Position, randomPoint))
      {
        randomValue += 1500;
      }
      if (!StaticMap::GetInstance()->IsLineOfSight(closestSupply, randomPoint))
      {
        randomValue += 1000;
      }

      if (randomValue < currentValue)
        // Set as target point
      {
        targetPoint = randomPoint;
      }
    }

    Vector2D desiredVelocity = (targetPoint - m_Position).unitVector() * MAXBOTSPEED;
    m_Acceleration = (desiredVelocity - m_Velocity);
//   	Renderer::GetInstance()->DrawDot(targetPoint);
    StopAiming();
    // Bounce off walls

    Circle2D bounds(m_Position, 50);

    if (StaticMap::GetInstance()->IsInsideBlock(bounds))
    {
      m_Acceleration = StaticMap::GetInstance()->GetNormalToSurface(bounds)*MAXIMUMACCELERATION;
    }
  }
	// Closest enemy within range?
	else if( StaticMap::GetInstance()->IsLineOfSight( m_Position, targetEnemy.m_Position)
		&& targetEnemy.IsAlive() && range<400)
	{
		// Kill it
		SetTarget(1-m_iOwnTeamNumber, closestEnemy);

    if (m_dAccuracy>0.7 || (targetEnemy.m_bAiming && targetEnemy.m_dTimeToCoolDown<0.1 &&m_dAccuracy>0.3))
		{
			Shoot();
		}
	}
  else if (StaticMap::GetInstance()->IsInsideBlock(loc))    // Too close to a wall
  {
    loc.PlaceAt(targetPoint, 30);
    if ((m_Position - targetPoint).magnitude()<45
      || StaticMap::GetInstance()->IsInsideBlock(loc))
    {
      targetPoint = m_Position + Vector2D(rand() % 60 - 30.0f, rand() % 60 - 30.0f);
    }
  }
	else    // Go to domination point
	{
    // If dom point is visible, within 400 units and owned
    if (StaticMap::GetInstance()->IsLineOfSight(m_Position, targetDP.m_Location)
      && (m_Position-targetDP.m_Location).magnitude()<400 &&
      targetDP.m_OwnerTeamNumber == m_iOwnTeamNumber)
    {
      // Stand and kill nearest target
      m_Acceleration = -m_Velocity;
      SetTarget(1 - m_iOwnTeamNumber, closestEnemy);
      if (m_dAccuracy>0.5)
      {
        Shoot();
      }
    }
    // If dom point is visible
		// Dock
		else if(StaticMap::GetInstance()->IsLineOfSight( m_Position, targetDP.m_Location))
		{
			// Target is dom point
      targetPoint = targetDP.m_Location;
      StopAiming();
		}
    else // Navigate to it
		{
			// Pick a random point
			Vector2D randomPoint(rand()%4000-2000.0f, rand()%4000-2000.0f);

			// Is it better than current target point?
			double currentValue = (m_Position - targetPoint).magnitude()
				+ (targetDP.m_Location - targetPoint).magnitude()*1.2;
			if(!StaticMap::GetInstance()->IsLineOfSight(m_Position, targetPoint))
			{
				currentValue+=2500;
			}
			if(!StaticMap::GetInstance()->IsLineOfSight(targetDP.m_Location, targetPoint))
			{
				currentValue+=1000;
			}
      // Is it too close to a block?
      Circle2D loc;
      loc.PlaceAt(randomPoint, 20);
      if (StaticMap::GetInstance()->IsInsideBlock(loc))
      {
        currentValue += 500;
      }

			double randomValue = (m_Position - randomPoint).magnitude()
					+ (targetDP.m_Location - randomPoint).magnitude()*1.2;
			if(!StaticMap::GetInstance()->IsLineOfSight(m_Position, randomPoint))
			{
				randomValue+=1500;
			}
			if(!StaticMap::GetInstance()->IsLineOfSight(targetDP.m_Location, randomPoint))
			{
				randomValue+=1000;
			}

			if(randomValue< currentValue)
			// Set as target point
			{
				targetPoint = randomPoint;
			}
      StopAiming();
		}

		// Head for target point
	//	Renderer::GetInstance()->DrawDot(targetPoint);
		Vector2D desiredVelocity = (targetPoint-m_Position).unitVector() * MAXBOTSPEED;
		m_Acceleration = (desiredVelocity-m_Velocity);

		// Bounce off walls
		Circle2D bounds(m_Position, 50);
		if(StaticMap::GetInstance()->IsInsideBlock(bounds))
		{
			m_Acceleration= StaticMap::GetInstance()->GetNormalToSurface(bounds)*MAXIMUMACCELERATION;
		}

	}
}
