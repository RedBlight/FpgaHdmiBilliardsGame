#include <stdio.h>
#include "xparameters.h"
#include "platform.h"
#include "xil_printf.h"
#include "xgpio.h"
#include "objectbuffer.h"
#include "xtime_l.h"
#include <math.h>

#include "xparameters.h"
#include "xgpio.h"

////////////////////////////////////////////////////////////////
/////////////////////////-- PILARDO --//////////////////////////
////////////////////////////////////////////////////////////////

struct Ball
{
	double radius;
	double posX;
	double posY;
	double speedX;
	double speedY;
	char colorR;
	char colorG;
	char colorB;
	int isActive;
};

struct Cueball
{
	double radius;
	double posX;
	double posY;
};

struct Hole
{
	double radius;
	double posX;
	double posY;
};

struct InputState
{
	int reset;
	int left;
	int right;
	int cue;
};

struct GameState
{
	XTime clockPrev;
	XTime clockNow;
	double deltaTime;

	double friction;

	double tableMinX;
	double tableMinY;
	double tableMaxX;
	double tableMaxY;

	double tableScreenMinX;
	double tableScreenMinY;
	double tableScreenMaxX;
	double tableScreenMaxY;

	int ballCount;
	struct Ball ball[16];

	int cueballCount;
	struct Cueball cueball[32];
	double cueballPadding;

	int holeCount;
	double holeOffset;
	struct Hole hole[6];

	double cueAngle;
	double cueAngleSpeed;
	double cueEnergy;
	double cueEnergyMin;
	double cueEnergyMax;
	double cueEnergySpeed;
	double cueDist;
	double cueDistMin;
	double cueDistMax;

	int readyToHit;
	int rolling;
	double rollTime;
	int hideCue;

	int fallenCount;
};

static struct InputState inputState;
static struct GameState gameState;
static double pi = 3.14159265358979323846264338327950288;

static XGpio btnArray;

static volatile uint32_t* fpgaPort = (volatile uint32_t*)( XPAR_OBJECTBUFFER_0_S00_AXI_BASEADDR );

static uint16_t ballMemoryOffset = 0;
static uint16_t cueballMemoryOffset = 256;

static uint16_t addressMap[32] = {
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,
	10,
	11,
	12,
	13,
	14,
	15,
	16,
	17,
	18,
	19,
	20,
	21,
	22,
	23,
	24,
	25,
	26,
	27,
	28,
	29,
	30,
	31
};

int PilardoInitFpgaInput();
void PilardoCollectInput();
void PilardoSetup();
void PilardoLoop();
void PilardoReset();
void PilardoEvaluateInput();
void PilardoUpdateGameState();
void PilardoDraw();
void WindowsDrawStart();
void WindowsDrawEnd();
double DeltaTime( XTime clockPrev, XTime clockNow );
void MapToScreen( double xReal, double yReal, uint16_t* xScreen, uint16_t* yScreen );
void SendDataToFpga( uint16_t varAddress, uint16_t varValue );

double DeltaTime( XTime clockPrev, XTime clockNow ) // in sec
{
	return (double)( clockNow - clockPrev ) / (double)( COUNTS_PER_SECOND );
}

void MapToScreen( double xReal, double yReal, uint16_t* xScreen, uint16_t* yScreen )
{
	*xScreen = (uint16_t)( round( xReal ) );
	*yScreen = (uint16_t)( round( yReal ) );
}

void SendDataToFpga( uint16_t varAddress, uint16_t varValue )
{
	fpgaPort[2] = ( varAddress << 16 | varValue );
	for( int i = 0; i<5; ++i );
	/*
	for( int i = 0; i<5; ++i )
	{
		for( int i = 0; i<1; ++i )
		{
			fpgaPort[1] = ( 65535 << 16 | 65535 );
		}
		for( int i = 0; i<1; ++i )
		{
			fpgaPort[1] = ( 0 << 16 | 0 );
		}
	}
	*/
}

int PilardoInitFpgaInput()
{
	if( XGpio_Initialize( &btnArray, XPAR_AXI_GPIO_0_DEVICE_ID ) != XST_SUCCESS )
	{
		return XST_FAILURE;
	}
	XGpio_SetDataDirection( &btnArray, 1, 0xFF );

	return XST_SUCCESS;
}

void PilardoSetup()
{
	PilardoReset();
}

void PilardoLoop()
{
	PilardoCollectInput();
	PilardoEvaluateInput();
	PilardoUpdateGameState();
	PilardoDraw();
}

void PilardoReset()
{
	inputState.reset = 0;
	inputState.left = 0;
	inputState.right = 0;
	inputState.cue = 0;

	XTime_GetTime( &(gameState.clockNow) );
	//gameState.clockNow = clock();
	gameState.clockPrev = gameState.clockNow;
	gameState.deltaTime = 0.0;

	gameState.friction = 50.0;

	gameState.tableMinX = 40.0;
	gameState.tableMinY = 40.0;
	gameState.tableMaxX = 1240.0;
	gameState.tableMaxY = 640.0;

	gameState.tableScreenMinX = 0.0;
	gameState.tableScreenMinY = 720.0;
	gameState.tableScreenMaxX = 1280.0;
	gameState.tableScreenMaxY = 0.0;

	gameState.ballCount = 16;

	gameState.cueAngle = 0.0;
	gameState.cueAngleSpeed = 2 * pi / 5.0;
	gameState.cueEnergy = 0.0;
	gameState.cueEnergyMin = 0.0;
	gameState.cueEnergyMax = 10000.0;
	gameState.cueEnergySpeed = 1000.0;
	gameState.cueDist = 16.0;
	gameState.cueDistMin = 16.0;
	gameState.cueDistMax = 256.0;

	//srand( time( 0 ) );

	double eps = 0.1;
	double ballRad = 16;
	double diffX = sqrt( 3.0 ) * ballRad + eps;
	double diffY = ballRad + eps;
	double startX = 100.0;
	double startY = gameState.tableMaxY / 2.0 - diffY * 4.0;

	for( int idx = 0; idx < gameState.ballCount; ++idx )
	{
		struct Ball* ball = &( gameState.ball[idx] );
		ball->radius = ballRad;
		ball->speedX = 0;
		ball->speedY = 0;
		ball->colorR = 0;
		ball->colorG = 0;
		ball->colorB = 0;
		ball->isActive = 1;
	}

	gameState.ball[1].posX = startX;
	gameState.ball[2].posX = startX;
	gameState.ball[3].posX = startX;
	gameState.ball[4].posX = startX;
	gameState.ball[5].posX = startX;
	gameState.ball[6].posX = startX + diffX * 1.0;
	gameState.ball[7].posX = startX + diffX * 1.0;
	gameState.ball[8].posX = startX + diffX * 1.0;
	gameState.ball[9].posX = startX + diffX * 1.0;
	gameState.ball[10].posX = startX + diffX * 2.0;
	gameState.ball[11].posX = startX + diffX * 2.0;
	gameState.ball[12].posX = startX + diffX * 2.0;
	gameState.ball[13].posX = startX + diffX * 3.0;
	gameState.ball[14].posX = startX + diffX * 3.0;
	gameState.ball[15].posX = startX + diffX * 4.0;

	gameState.ball[1].posY = startY;
	gameState.ball[2].posY = startY + diffY * 2.0;
	gameState.ball[3].posY = startY + diffY * 4.0;
	gameState.ball[4].posY = startY + diffY * 6.0;
	gameState.ball[5].posY = startY + diffY * 8.0;
	gameState.ball[6].posY = startY + diffY * 1.0;
	gameState.ball[7].posY = startY + diffY * 3.0;
	gameState.ball[8].posY = startY + diffY * 5.0;
	gameState.ball[9].posY = startY + diffY * 7.0;
	gameState.ball[10].posY = startY + diffY * 2.0;
	gameState.ball[11].posY = startY + diffY * 4.0;
	gameState.ball[12].posY = startY + diffY * 6.0;
	gameState.ball[13].posY = startY + diffY * 3.0;
	gameState.ball[14].posY = startY + diffY * 5.0;
	gameState.ball[15].posY = startY + diffY * 4.0;

	gameState.ball[0].posX = gameState.tableMaxX - startX;
	gameState.ball[0].posY = gameState.tableMaxY / 2.0;
	gameState.ball[0].speedX = 0;
	gameState.ball[0].speedY = 0;
	gameState.ball[0].colorR = 255;
	gameState.ball[0].colorG = 255;
	gameState.ball[0].colorB = 255;

	gameState.cueballCount = 32;
	gameState.cueballPadding = 4.0;

	for( int idx = 0; idx < gameState.cueballCount; ++idx )
	{
		struct Ball* ball0 = &( gameState.ball[0] );
		struct Cueball* cueball = &( gameState.cueball[idx] );
		cueball->radius = 4.0;
		double totalDist = cueball->radius + gameState.cueDist + (double)( idx ) * gameState.cueballPadding;
		cueball->posX = cos( gameState.cueAngle ) * totalDist + ball0->posX;
		cueball->posY = sin( gameState.cueAngle ) * totalDist + ball0->posY;
	}

	gameState.readyToHit = 1;
	gameState.rolling = 0;
	gameState.rollTime = 0.0;

	gameState.fallenCount = 0;

	gameState.holeCount = 6;
	for( int idx = 0; idx < gameState.holeCount; ++idx )
	{
		struct Hole* hole = &( gameState.hole[idx] );
		hole->radius = 16;
	}

	gameState.holeOffset = 4.0;

	gameState.hole[0].posX = 40.0 + gameState.holeOffset;
	gameState.hole[1].posX = 640;
	gameState.hole[2].posX = 1240 - gameState.holeOffset;
	gameState.hole[3].posX = 40.0 + gameState.holeOffset;
	gameState.hole[4].posX = 640;
	gameState.hole[5].posX = 1240 - gameState.holeOffset;

	gameState.hole[0].posY = 40.0 + gameState.holeOffset;
	gameState.hole[1].posY = 40.0 + gameState.holeOffset;
	gameState.hole[2].posY = 40.0 + gameState.holeOffset;
	gameState.hole[3].posY = 640.0 - gameState.holeOffset;
	gameState.hole[4].posY = 640.0 - gameState.holeOffset;
	gameState.hole[5].posY = 640.0 - gameState.holeOffset;

	////

	/*
	gameState.ball[0].radius = ballRad;
	gameState.ball[0].posX = gameState.tableMaxX + 100;
	gameState.ball[0].posY = gameState.tableMaxY / 2.0;
	gameState.ball[0].speedX = 0;
	gameState.ball[0].speedY = 0;
	gameState.ball[0].colorR = 255;
	gameState.ball[0].colorG = 255;
	gameState.ball[0].colorB = 255;

	gameState.ball[1].radius = ballRad;
	gameState.ball[1].posX = gameState.tableMinX + 100;
	gameState.ball[1].posY = gameState.tableMaxY / 2.0 - ballRad - eps;
	gameState.ball[1].speedX = 0;
	gameState.ball[1].speedY = 0;
	gameState.ball[1].colorR = 255;
	gameState.ball[1].colorG = 0;
	gameState.ball[1].colorB = 0;

	gameState.ball[2].radius = ballRad;
	gameState.ball[2].posX = gameState.tableMinX + 100;
	gameState.ball[2].posY = gameState.tableMaxY / 2.0 + ballRad + eps;
	gameState.ball[2].speedX = 0;
	gameState.ball[2].speedY = 0;
	gameState.ball[2].colorR = 0;
	gameState.ball[2].colorG = 0;
	gameState.ball[2].colorB = 255;
	*/
}

void PilardoCollectInput()
{
	int btnStatus = XGpio_DiscreteRead( &btnArray, 1 );
	inputState.reset = btnStatus == 1;
	inputState.left = btnStatus == 2;
	inputState.right = btnStatus == 4;
	inputState.cue = btnStatus == 8;
}

void PilardoEvaluateInput()
{
	if( inputState.reset )
	{
		PilardoReset();
	}

	if( gameState.readyToHit )
	{
		if( inputState.cue )
		{
			gameState.cueEnergy += gameState.deltaTime * gameState.cueEnergySpeed;
			if( gameState.cueEnergy > gameState.cueEnergyMax )
			{
				gameState.cueEnergy = gameState.cueEnergyMax;
			}
			gameState.cueDist = gameState.cueDistMin + ( gameState.cueDistMax - gameState.cueDistMin )
				* ( ( gameState.cueEnergy - gameState.cueEnergyMin ) / ( gameState.cueEnergyMax - gameState.cueEnergyMin  ) );
		}
		else
		{
			gameState.ball[0].speedX -= gameState.cueEnergy * cos( gameState.cueAngle );
			gameState.ball[0].speedY -= gameState.cueEnergy * sin( gameState.cueAngle );
			gameState.cueEnergy = gameState.cueEnergyMin;
			gameState.cueDist = gameState.cueDistMin + ( gameState.cueDistMax - gameState.cueDistMin )
				* ( ( gameState.cueEnergy - gameState.cueEnergyMin ) / ( gameState.cueEnergyMax - gameState.cueEnergyMin  ) );
			gameState.readyToHit = 0;
		}
	}

	if( inputState.left )
	{
		gameState.cueAngle += gameState.deltaTime * gameState.cueAngleSpeed;
	}

	if( inputState.right )
	{
		gameState.cueAngle -= gameState.deltaTime * gameState.cueAngleSpeed;
	}
}

void PilardoUpdateGameState()
{
	XTime_GetTime( &(gameState.clockNow) );
	//gameState.clockNow = clock();
	gameState.deltaTime = DeltaTime( gameState.clockPrev, gameState.clockNow );
	gameState.clockPrev = gameState.clockNow;

	for( int idA = 0; idA < gameState.ballCount; ++idA )
	{
		struct Ball* ballA = &( gameState.ball[idA] );

		if( !ballA->isActive ) continue;

		ballA->posX += ballA->speedX * gameState.deltaTime;
		ballA->posY += ballA->speedY * gameState.deltaTime;

		if( ballA->posX + ballA->radius > gameState.tableMaxX )
		{
			ballA->posX -= 2.0 * ( ( ballA->posX + ballA->radius ) - gameState.tableMaxX );
			ballA->speedX = -ballA->speedX;
		}

		if( ballA->posY + ballA->radius > gameState.tableMaxY )
		{
			ballA->posY -= 2.0 * ( ( ballA->posY + ballA->radius ) - gameState.tableMaxY );
			ballA->speedY = -ballA->speedY;
		}

		if( ballA->posX - ballA->radius < gameState.tableMinX )
		{
			ballA->posX += 2.0 * ( gameState.tableMinX - ( ballA->posX - ballA->radius ) );
			ballA->speedX = -ballA->speedX;
		}

		if( ballA->posY - ballA->radius < gameState.tableMinY )
		{
			ballA->posY += 2.0 * ( gameState.tableMinY - ( ballA->posY - ballA->radius ) );
			ballA->speedY = -ballA->speedY;
		}

		for( int idB = 0; idB < gameState.ballCount; ++idB )
		{
			if( idB == idA ) continue;

			struct Ball* ballB = &( gameState.ball[idB] );

			if( !ballB->isActive ) continue;

			double dy = ballA->posY - ballB->posY;
			double dx = ballA->posX - ballB->posX;
			double dr = ballA->radius + ballB->radius;
			double dist = sqrt( dy * dy + dx * dx );

			if( dist <= dr )
			{
				double vi = sqrt( ballA->speedX * ballA->speedX + ballA->speedY * ballA->speedY );
				double vj = sqrt( ballB->speedX * ballB->speedX + ballB->speedY * ballB->speedY );

				double avi = atan2( ballA->speedY, ballA->speedX );
				double avj = atan2( ballB->speedY, ballB->speedX );

				double avc = atan2( dy , dx );

				double vxin = vj * cos( avj - avc ) * cos( avc ) + vi * sin( avi - avc ) * cos( avc + pi / 2.0 );
				double vyin = vj * cos( avj - avc ) * sin( avc ) + vi * sin( avi - avc ) * sin( avc + pi / 2.0 );

				double vxjn = vi * cos( avi - avc ) * cos( avc ) + vj * sin( avj - avc ) * cos( avc + pi / 2.0 );
				double vyjn = vi * cos( avi - avc ) * sin( avc ) + vj * sin( avj - avc ) * sin( avc + pi / 2.0 );

				double colltime = ( dr - dist ) / ( vi + vj + 0.001 );

				ballA->posX -= ( ballA->speedX * colltime );
				ballA->posY -= ( ballA->speedY * colltime );
				ballB->posX -= ( ballB->speedX * colltime );
				ballB->posY -= ( ballB->speedY * colltime );

				ballA->speedX = vxin;
				ballA->speedY = vyin;
				ballB->speedX = vxjn;
				ballB->speedY = vyjn;

				ballA->posX += ( ballA->speedX * colltime );
				ballA->posY += ( ballA->speedY * colltime );
				ballB->posX += ( ballB->speedX * colltime );
				ballB->posY += ( ballB->speedY * colltime );

			}

		}
	}

	for( int idA = 0; idA < gameState.ballCount; ++idA )
	{
		struct Ball* ballA = &( gameState.ball[idA] );

		if( !ballA->isActive ) continue;

		double totalSpeed = sqrt( ballA->speedX * ballA->speedX + ballA->speedY * ballA->speedY );
		double totalSpeedNew = totalSpeed - gameState.friction * gameState.deltaTime;
		totalSpeedNew = totalSpeedNew <= 0.0 ? 0.0 : totalSpeedNew;
		double speedFactor = totalSpeed < 0.00000001 ? 0.0 : totalSpeedNew / totalSpeed;
		ballA->speedX *= speedFactor;
		ballA->speedY *= speedFactor;
	}

	for( int idx = 0; idx < gameState.cueballCount; ++idx )
	{
		struct Ball* ball0 = &( gameState.ball[0] );
		struct Cueball* cueball = &( gameState.cueball[idx] );
		double totalDist = cueball->radius + gameState.cueDist + (double)( idx ) * gameState.cueballPadding;
		cueball->posX = cos( gameState.cueAngle ) * totalDist + ball0->posX;
		cueball->posY = sin( gameState.cueAngle ) * totalDist + ball0->posY;
	}

	gameState.rolling = 0;
	for( int idA = 0; idA < gameState.ballCount; ++idA )
	{
		struct Ball* ballA = &( gameState.ball[idA] );

		if( !ballA->isActive ) continue;

		if( ballA->speedX != 0.0 || ballA->speedY != 0.0 )
		{
			gameState.rolling = 1;
		}
	}

	if( gameState.rolling )
	{
		gameState.rollTime += gameState.deltaTime;
	}
	else
	{
		gameState.rollTime = 0.0;
	}

	if( gameState.rollTime > 0.1 )
	{
		gameState.hideCue = 1;
	}
	else
	{
		gameState.hideCue = 0;
	}

	if( !gameState.rolling )
	{
		gameState.readyToHit = 1;
	}

	for( int idA = 1; idA < gameState.ballCount; ++idA )
	{
		struct Ball* ballA = &( gameState.ball[idA] );

		if( !ballA->isActive ) continue;

		for( int idB = 0; idB < gameState.holeCount; ++idB )
		{
			struct Hole* ballB = &( gameState.hole[idB] );
			double dy = ballA->posY - ballB->posY;
			double dx = ballA->posX - ballB->posX;
			double dr = ballA->radius + ballB->radius;
			double dist = sqrt( dy * dy + dx * dx );
			if( dist < dr - 4.0 )
			{
				ballA->posX = 20.0 + (double)(gameState.fallenCount) * 40.0;
				ballA->posY = 700.0;
				ballA->speedX = 0.0;
				ballA->speedY = 0.0;
				ballA->isActive = 0;
				++( gameState.fallenCount );
			}
		}
	}

}

void PilardoDraw()
{
	//WindowsDrawStart();
	//SelectObject( hdcMem, GetStockObject( DC_PEN ) );
 //   SelectObject( hdcMem, GetStockObject( DC_BRUSH ) );

	//COLORREF tableFillColor = RGB( 0, 128, 0 );
	//COLORREF ballColor = RGB( 255, 255, 255 );

	//SetDCBrushColor( hdcMem, tableFillColor );
	//Rectangle( hdcMem, 0, 0, 800, 600 );

	//for( int idx = 0; idx < 16; ++idx  )
	//{
	//	int ballLeft = (int)( round( gameState.ball[idx].posX - gameState.ball[idx].radius ) );
	//	int ballRight = (int)( round( gameState.ball[idx].posX + gameState.ball[idx].radius ) );
	//	int ballTop = (int)( round( ( gameState.tableMaxY - gameState.ball[idx].posY ) - gameState.ball[idx].radius ) );
	//	int ballBottom = (int)( round( ( gameState.tableMaxY - gameState.ball[idx].posY ) + gameState.ball[idx].radius ) );
	//
	//	ballColor = RGB( gameState.ball[idx].colorR, gameState.ball[idx].colorG, gameState.ball[idx].colorB );
	//	SetDCBrushColor( hdcMem, ballColor );
	//	Ellipse( hdcMem, ballLeft, ballTop, ballRight, ballBottom );
	//}
	//
	//WindowsDrawEnd();

	uint16_t iPosX;
	uint16_t iPosY;

	uint16_t varAddress = 0;

	for( int idA = 0; idA < gameState.ballCount; ++idA )
	{
		struct Ball* ballA = &( gameState.ball[idA] );
		MapToScreen( ballA->posX, ballA->posY, &iPosX, &iPosY );

		SendDataToFpga( 9999, 9999 );
		SendDataToFpga( 9999, iPosX - ballA->radius );
		SendDataToFpga( varAddress + ballMemoryOffset, iPosX - ballA->radius );
		SendDataToFpga( 9999, iPosX - ballA->radius );
		SendDataToFpga( 9999, 9999 );
		varAddress++;

		SendDataToFpga( 9999, 9999 );
		SendDataToFpga( 9999, iPosY - ballA->radius );
		SendDataToFpga( varAddress + ballMemoryOffset, iPosY - ballA->radius );
		SendDataToFpga( 9999, iPosY - ballA->radius );
		SendDataToFpga( 9999, 9999 );
		varAddress++;
	}

	varAddress = 0;

	for( int idA = 0; idA < gameState.cueballCount; ++idA )
	{
		struct Cueball* cueball = &( gameState.cueball[idA] );

		if( gameState.hideCue )
		{
			MapToScreen( 9999.9, 9999.9, &iPosX, &iPosY );
		}
		else
		{
			MapToScreen( cueball->posX, cueball->posY, &iPosX, &iPosY );
		}


		SendDataToFpga( 9999, 9999 );
		SendDataToFpga( 9999, iPosX - cueball->radius );
		SendDataToFpga( varAddress + cueballMemoryOffset, iPosX - cueball->radius );
		SendDataToFpga( 9999, iPosX - cueball->radius );
		SendDataToFpga( 9999, 9999 );
		varAddress++;

		SendDataToFpga( 9999, 9999 );
		SendDataToFpga( 9999, iPosY - cueball->radius );
		SendDataToFpga( varAddress + cueballMemoryOffset, iPosY - cueball->radius );
		SendDataToFpga( 9999, iPosY - cueball->radius );
		SendDataToFpga( 9999, 9999 );
		varAddress++;
	}

}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

int main()
{
	int status;

	init_platform();

	status = PilardoInitFpgaInput();
	if( status != XST_SUCCESS )
	{
		print("ERROR @ INPUT INIT!\n");
		return XST_FAILURE;
	}
	print("INPUT INIT SUCCESS!\n");


	fpgaPort[0] = ( 0 << 16 | 0 );
	fpgaPort[1] = ( 0 << 16 | 0 );


	PilardoSetup();
	while( 1 )
	{
		PilardoLoop();
	}


	cleanup_platform();
	return 0;
}
