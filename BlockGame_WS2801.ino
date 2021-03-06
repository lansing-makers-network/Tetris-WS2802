/*
\file  BlockGame_WS2801.ino
\brief  Arduino sketch to run a Block Playing Game using a string of WS2801 RGB leds.
\remarks  comments are implemented with Doxygen Markdown format

\section  Contributors Contributors
\author  Michael Flaga, www.flaga.net
\author  Brian Adams, www.lansingmakersnetwork.org
\author  Bill Porter, www.billporter.info
\author  Mofidul Jamal

\licence  This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License. To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or send a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.

\section  References References
\see
- Software
 - <A HREF = "https://github.com/adafruit/Adafruit-WS2801-Library"> WS2801 Library</A>.
 - <A HREF = "https://github.com/adafruit/Adafruit_NeoPixel"> Adafruit_NeoPixel Library</A>.
 - <A HREF = "http://playground.arduino.cc//Main/WiiChuckClass?action=sourceblock&num=3"> Wii Nunchuck Library</A>.
- Hardware
 - a Arduino
 - a lot of either WS2801 or WS2811/WS2812s
 - Wii Chuck
 - <A HREF = https://www.sparkfun.com/products/9281"> WiiChuck Adapter - DEV-09281</A>.
 
\Note
 - due to the larger number of the strings length the WS2801 is preferable over that of the WS2811, in that the WS2801 has the clock.
   and WS2801 can be purchased to run off of 12 Volts, lowering the supply amperage.
 - Note at this time the WiiChuckClass will only compile on an Arduino UNO or Mega, but not Micro/Leonardo.
*/

// Hardware Specific Constants

// Type of RGB:
#define NEOPIXEL // comment out if WS2801
//#define LEDCLKPIN 13 // if WS2801 (not NeoPixel) and commented out then Hardware SPI will be used.
#define LEDDATAPIN 11 // Pin of RGB data if not SPI, otherwise dont care.

// Panel Description:
//#define BOTTOM_LEFT // comment if first pixel is bottom left, otherwise it is bottom right.
//#define VERT_STRIPS // comment out if Horiztonal Rows otherwise Veritical Colomuns.
#define PROGRESSIVE // Comment out if stripes are zigzag .
#define FIELD_WIDTH 8 // Horizontal Width.
#define FIELD_HEIGHT 24 // Veritical Heigth.
#define LEDS FIELD_HEIGHT * FIELD_WIDTH

// Operational Constants:
#define FULL_INTENSITY 16 // 0 to 255 where lower values are dimmed to run off of smaller power supplies, good for testing.
#define MAX_IDLEBRICKS 4 //Number of Moves without user input, before Idle
#define TICK_DELAY 400 //game speed, millseconds between screen updates.
#define BOUNCE_DELAY 50 //weight given to the highest column for ai
#define HIGH_COLUMN_WEIGHT 5 //weight given to the number of holes for ai
#define HOLE_WEIGHT 3
#define ADD_EXTRA_BRICK_COUNT 20 // limit before adding last mystry brick

#ifdef NEOPIXEL
#include <Adafruit_NeoPixel.h>
#else // NEOPIXEL
#include <SPI.h>
#include <Adafruit_WS2801.h>
#endif // NEOPIXEL

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <Wire.h>
#include <WiiChuck.h>

//Interpreted WII Chuck response values
#define COUNTERCLOCKWISE  0
#define DOWN  1
#define RIGHT  2
#define LEFT  3
#define CLOCKWISE 4
#define NUMBEROFMOVES 5 // Idle/No input from WII Chuck

unsigned long  next_tick = 0;
unsigned long bounce_tick = 0;

// Block Descriptions, Each brick consists of 4 patterns one for of each rotation, 
// where each uint16_t pattern is 4 nibbles that stack to create the 4x4 block.
const PROGMEM uint16_t bricks[][4] = {
  {
    0b0100010001000100,      //1x4 cyan
    0b0000000011110000,
    0b0100010001000100,
    0b0000000011110000
  },
  {
    0b0000010011100000,      //T  purple
    0b0000010001100100,
    0b0000000011100100,
    0b0000010011000100
  },
  {
    0b0000011001100000,      //2x2 yellow
    0b0000011001100000,
    0b0000011001100000,
    0b0000011001100000
  },
  {
    0b0000000011100010,      //L orange
    0b0000010001001100,
    0b0000100011100000,
    0b0000011001000100
  },
  {
    0b0000000011101000,      //inverse L blue
    0b0000110001000100,
    0b0000001011100000,
    0b0000010001000110
  },
  {
    0b0000100011000100,      //S green
    0b0000011011000000,
    0b0000100011000100,
    0b0000011011000000
  },
  {
    0b0000010011001000,      //Z red
    0b0000110001100000,
    0b0000010011001000,
    0b0000110001100000
  },
#define EXTRABRICKS 2 // The Two Non-Cannon Bricks are easter egg or deterents. 
  {
    0b0000111010101110,      // doughnut
    0b0000111010101110,
    0b0000111010101110,
    0b0000111010101110
  },
  {
    0b0000101001001010,      // X
    0b0000101001001010,
    0b0000101001001010,
    0b0000101001001010,
  }
};
uint8_t brick_count = sizeof(bricks)/sizeof(bricks[0]);


//8 bit RGB colors of blocks
//RRRBBBGG
const PROGMEM uint8_t brick_colors[]={
  0b00011111, //cyan
  0b10010000, //purple
  0b11100011, //yellow
  0b11100001, //orange?
  0b00011100, //blue
  0b00000011, //green
  0b11100000, //red
  0b11100111, //pale yellow
  0b01011001, //pale purple
};

//You will need to modify this to translate fro x,y to a pixel number.
uint16_t computeAddress(int row, int col){
	uint16_t reversed = 0;
#ifdef VERT_STRIPS
#ifndef PROGRESSIVE
	if (col%2 == 0) {
		reversed = 1;
	}
#endif // PROGRESSIVE
uint16_t base = (col)*FIELD_HEIGHT;
	if (reversed) {
		base += FIELD_HEIGHT - 1;
	}
	uint16_t final = reverse == 1? base - row: base + row;
	}
#else // VERT_STRIPS
#ifndef PROGRESSIVE
	if (row%2 == 0){
		reversed = 1;
	}
#endif // PROGRESSIVE
	uint16_t base = (row)*FIELD_WIDTH;
	if (reversed) {
		base += FIELD_WIDTH -1;
	}
	uint16_t final = reversed == 1 ? base - col: base + col;
#endif // VERT_STRIPS
	return final;
}

byte wall[FIELD_WIDTH][FIELD_HEIGHT];
//The 'wall' is the 2D array that holds all bricks that have already 'fallen' into place

bool aiCalculatedAlready = false;
bool useAi = true;
struct TAiMoveInfo{
  byte rotation;
  int positionX, positionY;
  int weight;
} aiCurrentMove;

struct TBrick{
  byte type; //This is the current brick shape. 
  byte rotation; //active brick rotation
  byte color; //active brick color
  int positionX, positionY; //active brick position
  byte pattern[4][4]; //2D array of active brick shape, used for drawing and collosion detection

} currentBrick;


//unsigned short  level        = 0;
//unsigned long  score        = 0;
//unsigned long  score_lines      = 0;

//WiiChuck

WiiChuck chuck = WiiChuck();


// Define the RGB pixel array and controller functions, 
#ifdef NEOPIXEL
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDS, LEDDATAPIN, NEO_GRB + NEO_KHZ800);
#else // NEOPIXEL
#ifdef LEDCLKPIN
Adafruit_WS2801 strip = Adafruit_WS2801(LEDS, LEDDATAPIN, LEDCLKPIN);
#else // LEDCLKPIN
Adafruit_WS2801 strip = Adafruit_WS2801(LEDS);
#endif // LEDCLKPIN
#endif // NEOPIXEL

int idleBricks = 0;

int numberOfBricksSinceAI = 0;
int numberOfBricksInGame = 0;

void setup(){

  Serial.begin(115200); 
  Serial.println(F("Starting Arduino BlockGame"));
  Serial.print(F("Numbers of Possible Bricks = "));
  Serial.println(sizeof(bricks)/sizeof(bricks[0]));
  brick_count = (sizeof(bricks)/sizeof(bricks[0])) - EXTRABRICKS; 

  Serial.print(F("Numbers of Bricks in Play = "));
  Serial.println(brick_count);

  strip.begin();

  //Pre-Operating Self Test of LED grid.
  Serial.println(F("Starting Pre Operating Self Test"));
  Serial.println(F("fade to Red"));
  fadeGrid(Color(0,0,0), Color(FULL_INTENSITY,0,0), 8, 50);   // fade from off to Red
  Serial.println(F("fade to Green"));
  fadeGrid(Color(FULL_INTENSITY,0,0), Color(0,FULL_INTENSITY,0), 8, 50); // fade from Red to Green
  Serial.println(F("fade to Blue"));
  fadeGrid(Color(0,FULL_INTENSITY,0), Color(0,0,FULL_INTENSITY), 8, 50); // fade from Green to Blue
  Serial.println(F("fade to off"));
  fadeGrid(Color(0,0,FULL_INTENSITY), Color(0,0,0), 8, 50);   // fade from Blue to Off
  Serial.println(F("Pre Operating Self Test Finished"));

  Serial.print(F("useAI mode = ")); 
  Serial.println(useAi); 
  
  chuck.begin();
  chuck.update();
  newGame();

}

void loop(){

  //screenTest();
  play();
  
}

//tests pixels
void screenTest(){
  for( int i = 0; i < FIELD_WIDTH; i++ )
  {
    for( int k = 0; k < FIELD_HEIGHT; k++ )
    {
      wall[i][k] = 7;
      drawGame();
      delay(500);
    }
  }
}

//plays the game!
// globals, sorry (@RCK)
int ct = 0;
void play(){
	ct++; // increment our tick counter
	if(aiCalculatedAlready == false) {
		performAI();
	}


	if (millis() > bounce_tick) {
		byte command = getCommand();

		if ( command != 4 ) {
			bounce_tick = millis() + BOUNCE_DELAY;
		}
		/* To account for an oversensitive thumbstick,
		   we want to introduce a timer that prevents
		   commands from being processed too frequently. @RCK */

		// if we're not on the AI, and there is a real command
		// but < 7 loops have gone by, pretend it didn't happen.
		if (!useAi && command < 4 && ct < 7) {
			Serial.print(F("SKIPPED"));
			Serial.println(ct);
		} else { // ok, we can process this command.

			// reset the tick counter if it's a command
			if (command < 4)
				ct = 0;

			// process the command
			if ( command == COUNTERCLOCKWISE ) {
  			Serial.println(F("ROTATE 90"));
				bounce_tick = millis() + BOUNCE_DELAY*5;
				if ( checkRotate( 1 ) == true ) {
					rotate( 1 );
				}
			} else if ( command == CLOCKWISE ) {
  			Serial.println(F("ROTATE -90"));
				bounce_tick = millis() + BOUNCE_DELAY*5;
				if ( checkRotate( 0 ) == true ) {
					rotate( 0 );
				}
			} else if ( command == RIGHT ) {
				if ( checkShift( -1, 0 ) == true ) {
					Serial.println(F("SHIFT RIGHT"));
					shift( -1, 0 );
				}
			} else if ( command == LEFT ) {
				if ( checkShift( 1, 0 ) == true ) {
					Serial.println(F("SHIFT LEFT"));
					shift( 1, 0 );
				}
			} else if ( command == DOWN ) {
  			Serial.print(F("D"));
				moveDown();
			}
		}
	}
	if ( millis() > next_tick ) {
		next_tick = millis()+TICK_DELAY;
		moveDown();
	}
	drawGame();
}


//performs AI player calculations. 
void performAI(){
  Serial.println(F("AI performed"));
  struct TBrick initialBrick;
  //save position of the brick in its raw state
  memcpy((void*)&initialBrick, (void*)&currentBrick, sizeof(TBrick));
  //stores our 20 possible AI moves
  struct TAiMoveInfo aiMoves[4 * FIELD_WIDTH];
  //counter keeps track of the current index into our aimoves array
  byte aiMoveCounter = 0;
  //save position of the the brick at the left most rotated position
  struct TBrick aiLeftRotatedBrick;
  //save position of the brick at the rotated position
  struct TBrick aiRotatedBrick;

  //first check the rotations(initial, rotated once, twice, thrice)
  for(int aiRotation = 0; aiRotation < 4; aiRotation++ )
  {
    //rotate if possible
    if(checkRotate(1) == true)
      rotate(1);
    //save the rotated brick
    memcpy((void*)&aiRotatedBrick, (void*)&currentBrick, sizeof(TBrick));
    //shift as far left as possible
    while(checkShift(-1,0) == true)
      shift(-1, 0);
    //save this leftmost rotated position
    memcpy((void*)&aiLeftRotatedBrick, (void*)&currentBrick, sizeof(TBrick));

    //now check each possible position of X
    for(int aiPositionX = 0; aiPositionX < FIELD_WIDTH; aiPositionX++)
    {
      //next move down until we can't
      while(checkGround() == false )
      {
        shift(0,1);
      }
      //calculate ai weight of this particular final position
      int aiMoveWeight = aiCalculateWeight();
      //save the weight, positions and rotations for this ai move
      aiMoves[aiMoveCounter].weight = aiMoveWeight;
      aiMoves[aiMoveCounter].rotation = currentBrick.rotation;
      aiMoves[aiMoveCounter].positionX = currentBrick.positionX;
      aiMoves[aiMoveCounter].positionY = currentBrick.positionY;
      //move our index up for the next position to save to
      aiMoveCounter++;
      //drawGame();
      //Serial.println(aiMoveWeight);
      //delay(500);

      //now restore the previous position and shift it right by the column # we are checking
      memcpy((void*)&currentBrick, (void*)&aiLeftRotatedBrick, sizeof(TBrick));
      if(checkShift(aiPositionX+1,0) == true)
        shift(aiPositionX+1,0);
    }

    //reload rotated start position
    memcpy((void*)&currentBrick, (void*)&aiRotatedBrick, sizeof(TBrick));
  }
  
  //at this point we have calculated all the weights of every possible position and rotation of the brick

  //find move with lowest weight 
  int lowestWeight = aiMoves[0].weight;
  int lowestWeightIndex = 0;
  for(int i = 1; i < aiMoveCounter; i++)
  {
    if(aiMoves[i].weight <= lowestWeight)
    {
      lowestWeight = aiMoves[i].weight;
      lowestWeightIndex = i;
    }
  }
  //save this AI move as the current move
  memcpy((void*)&aiCurrentMove, (void*)&aiMoves[lowestWeightIndex], sizeof(TAiMoveInfo));
  //restore original brick that we started with
  memcpy((void*)&currentBrick, (void*)&initialBrick, sizeof(TBrick));
  //update the brick, set the ai flag so we know that we dont need to recalculate
  updateBrickArray();
  aiCalculatedAlready = true;
}

//calculates the ai weight
//when this function is called, the currentBrick is moved into a final legal position at the bottom of the wall
//which is why we add it to the wall first and then remove it at the end
int aiCalculateWeight(){
  int weights = 0;
  //add to wall first before calculating ai stuffs
  addToWall(); 
  //get the two weights
  int highestColumn = getHighestColumn();
  int holeCount = getHoleCount();

  //if this position will yield a full completed row then its weight is 0, which is the lowest possible
  //remember the the lowest weight will be the best move to make
  if(getFullLinePossible() == true)
  {
    weights = 0;
  }
  else
  {
      weights = (HIGH_COLUMN_WEIGHT * highestColumn) + (HOLE_WEIGHT * holeCount);
  }
  removeFromWall(); //undo the wall addition when done
  return weights;
}


//returns how high the wall goes 
int getHighestColumn(){
  int columnHeight = 0;
  //count
  int maxColumnHeight = 0;
  for(int j = 0; j < FIELD_WIDTH; j++)
  {
    columnHeight = 0;
    for(int k = FIELD_HEIGHT-1; k!=0; k--)
    {
      if(wall[j][k] != 0)
      {
        columnHeight = FIELD_HEIGHT - k;
        //Serial.print(k);
        //Serial.println(F(" is k"));
        //delay(100);
      }
    }
    if(columnHeight > maxColumnHeight)
      maxColumnHeight = columnHeight;
  }
  return maxColumnHeight;
}

//counts the number of given holes for the ai calculation
int getHoleCount(){
  int holeCount = 0;
  for(int j = 0; j < FIELD_WIDTH; j++)
  {
    for(int k = currentBrick.positionY + 2; k < FIELD_HEIGHT; k++)
    {
      if(wall[j][k] == 0)
        holeCount++;
    }
  }
  return holeCount;
}

//determines if a full line is possible given the current wall (for ai)
bool getFullLinePossible()
{
  int lineCheck;
  for(byte i = 0; i < FIELD_HEIGHT; i++)
  {
    lineCheck = 0;
    for(byte k = 0; k < FIELD_WIDTH; k++)
    {
      if( wall[k][i] != 0)  
        lineCheck++;
    }
    
    if(lineCheck == FIELD_WIDTH)
    {
      return true;
    }
  }
  return false;
}
//gets commands according to ai state
byte getCommand(){
  /*
  if(currentBrick.rotation != aiCurrentMove.rotation)
    return COUNTERCLOCKWISE;
  if(currentBrick.positionX > aiCurrentMove.positionX)
    return LEFT;
  if(currentBrick.positionX < aiCurrentMove.positionX)
    return RIGHT;
  if(currentBrick.positionX == aiCurrentMove.positionX)
    return DOWN;
    */
    
  byte playerMove = NUMBEROFMOVES;
  chuck.update(); 

  int x = chuck.readJoyX();
  int y = chuck.readJoyY();

  if (chuck.buttonC && chuck.buttonZ) {
    Serial.println(F("Both Button C & Z pushed."));
     useAi = !useAi;
     Serial.println(F("Toggling useAI mode"));
     if (useAi) {
    	 colorGrid(Color(FULL_INTENSITY, 0, 0));
       Serial.println(F("useAI mode enabled"));
     } else {
    	 colorGrid(Color(0, FULL_INTENSITY, 0));
       Serial.println(F("useAI mode disabled"));
     }
     strip.show();
     delay(250);
  } else if (chuck.buttonZ){
    Serial.println(F("Button Z pushed."));
    playerMove = COUNTERCLOCKWISE;
  } else if (chuck.buttonC){
    Serial.println(F("Button C pushed."));
    playerMove = CLOCKWISE;
  } else if ((x != -DEFAULT_ZERO_JOY_X) && (y != -DEFAULT_ZERO_JOY_Y)) {
    if (x > 75){
      Serial.print(F("RIGHT: Joy X > 75.("));
      Serial.print(x);
      Serial.println(F(")"));
#ifdef BOTTOM_LEFT
      playerMove = LEFT;
#else // BOTTOM_LEFT
      playerMove = RIGHT;
#endif // BOTTOM_LEFT
    } else if (x < -75){
      Serial.print(F("LEFT: Joy X < -75.("));
      Serial.print(x);
      Serial.println(F(")"));
#ifdef BOTTOM_LEFT
      playerMove = RIGHT;
#else // BOTTOM_LEFT
      playerMove = LEFT;
#endif // BOTTOM_LEFT
    } else if ( y < -75 ){
      Serial.print(F("DOWN: Joy Y < -75.("));
      Serial.print(y);
      Serial.println(F(")"));
      playerMove = DOWN;
    }
  }

  if (playerMove < NUMBEROFMOVES) {
    idleBricks = 0;
    Serial.println(F("resetting the idle manual brick count to zero"));
  }
  
  if (useAi){
    if (playerMove < NUMBEROFMOVES) {
      useAi = !useAi;
      Serial.println(F("Toggling useAI mode OFF!"));
      idleBricks = 0;
    } else {
//      Serial.print(currentBrick.rotation); Serial.print(F("|"));
//      Serial.print(aiCurrentMove.rotation); Serial.print(F("|"));
//      Serial.print(currentBrick.positionX); Serial.print(F("|"));
//      Serial.print(aiCurrentMove.positionX); Serial.print(F(" "));
      if(currentBrick.rotation != aiCurrentMove.rotation)
        playerMove = COUNTERCLOCKWISE;
      if(currentBrick.positionX > aiCurrentMove.positionX)
        playerMove = RIGHT;
      if(currentBrick.positionX < aiCurrentMove.positionX)
        playerMove = LEFT;
      if(currentBrick.positionX == aiCurrentMove.positionX)
        playerMove = DOWN;
    }
//  } else {
//    Serial.println("Problem in getcommand useAi");
  }




  chuck.update();
  return playerMove;
    
}

//checks if the next rotation is possible or not.
bool checkRotate( bool direction )
{
  rotate( direction );
  bool result = !checkCollision();
  rotate( !direction );

  return result;
}

//checks if the current block can be moved by comparing it with the wall
bool checkShift(short right, short down)
{
  shift( right, down );
  bool result = !checkCollision();
  shift( -right, -down );

  return result;
}

// checks if the block would crash if it were to move down another step
// i.e. returns true if the eagle has landed.
bool checkGround()
{
  shift( 0, 1 );
  bool result = checkCollision();
  shift( 0, -1 );
  return result;
}

// checks if the block's highest point has hit the ceiling (true)
// this is only useful if we have determined that the block has been
// dropped onto the wall before!
bool checkCeiling()
{
  for( int i = 0; i < 4; i++ )
  {
    for( int k = 0; k < 4; k++ )
    {
      if(currentBrick.pattern[i][k] != 0)
      {
        if( ( currentBrick.positionY + k ) < 0 )
        {
          return true;
        }
      }
    }
  }
  return false;
}

//checks if the proposed movement puts the current block into the wall.
bool checkCollision()
{
  int x = 0;
  int y =0;

  for( byte i = 0; i < 4; i++ )
  {
    for( byte k = 0; k < 4; k++ )
    {
      if( currentBrick.pattern[i][k] != 0 )
      {
        x = currentBrick.positionX + i;
        y = currentBrick.positionY + k;

        if(x >= 0 && y >= 0 && wall[x][y] != 0)
        {
          //this is another brick IN the wall!
          return true;
        }
        else if( x < 0 || x >= FIELD_WIDTH )
        {
          //out to the left or right
          return true;
        }
        else if( y >= FIELD_HEIGHT )
        {
          //below sea level
          return true;
        }
      }
    }
  }
  return false; //since we didn't return true yet, no collision was found
}

//updates the position variable according to the parameters
void shift(short right, short down)
{
  currentBrick.positionX += right;
  currentBrick.positionY += down;
}

// updates the rotation variable, wraps around and calls updateBrickArray().
// direction: 1 for clockwise (default), 0 to revert.
void rotate( bool direction )
{
  if( direction == 1 )
  {
    if(currentBrick.rotation == 0)
    {
      currentBrick.rotation = 3;
    }
    else
    {
      currentBrick.rotation--;
    }
  }
  else
  {
    if(currentBrick.rotation == 3)
    {
      currentBrick.rotation = 0;
    }
    else
    {
      currentBrick.rotation++;
    }
  }
  updateBrickArray();
}

void moveDown(){
  Serial.print(F("."));
  if( checkGround() )
  {
    #ifdef DEGUB
  	Serial.print(F("checkGround()"));
  	Serial.print(F("bounce_tick = "));
  	Serial.print(bounce_tick);
  	Serial.print(F(" millis() = "));
  	Serial.println(millis());
  	#endif

    addToWall();
    #ifdef DEGUB
  	Serial.print(F("addToWall()"));
  	Serial.print(F("bounce_tick = "));
  	Serial.print(bounce_tick);
  	Serial.print(F(" millis() = "));
  	Serial.println(millis());
  	#endif

    drawGame();
    #ifdef DEGUB
  	Serial.print(F("drawGame()"));
  	Serial.print(F("bounce_tick = "));
  	Serial.print(bounce_tick);
  	Serial.print(F(" millis() = "));
  	Serial.println(millis());
  	#endif
    
    if( checkCeiling() )
    {
    #ifdef DEGUB
  	Serial.print(F("Ceiling Found"));
  	Serial.print(F("bounce_tick = "));
  	Serial.print(bounce_tick);
  	Serial.print(F(" millis() = "));
  	Serial.println(millis());
  	#endif
      gameOver();
    }
    else
    {
      while( clearLine() )
      {
        //scoreOneUpLine();
      }
      nextBrick();
      //scoreOneUpBrick();
    }
  }
  else
  {
    //grounding not imminent
    shift( 0, 1 );
  }
  //scoreAdjustLevel();
  //ticks = 0;
}

//put the brick in the wall after the eagle has landed.
void addToWall()
{
  for( byte i = 0; i < 4; i++ )
  {
    for( byte k = 0; k < 4; k++ )
    {
      if(currentBrick.pattern[i][k] != 0){
        wall[currentBrick.positionX + i][currentBrick.positionY + k] = currentBrick.color;
        
      }
    }
  }
}

//removes brick from wall, used by ai algo
void removeFromWall(){
  for( byte i = 0; i < 4; i++ )
  {
    for( byte k = 0; k < 4; k++ )
    {
      if(currentBrick.pattern[i][k] != 0){
        wall[currentBrick.positionX + i][currentBrick.positionY + k] = 0;
        
      }
    }
  }
}

//uses the currentBrick_type and rotation variables to render a 4x4 pixel array of the current block
// from the 2-byte binary reprsentation of the block
void updateBrickArray()
{
  unsigned int data = pgm_read_word(&(bricks[ currentBrick.type ][ currentBrick.rotation ]));
  for( byte i = 0; i < 4; i++ )
  {
    for( byte k = 0; k < 4; k++ )
    {
      if(bitRead(data, 4*i+3-k))
      currentBrick.pattern[k][i] = currentBrick.color; 
      else
      currentBrick.pattern[k][i] = 0;
    }
  }
}
//clears the wall for a new game
void clearWall()
{
  for( byte i = 0; i < FIELD_WIDTH; i++ )
  {
    for( byte k = 0; k < FIELD_HEIGHT; k++ )
    {
      wall[i][k] = 0;
    }
  }
}

// find the lowest completed line, do the removal animation, add to score.
// returns true if a line was removed and false if there are none.
bool clearLine()
{
  int line_check;
  for( byte i = 0; i < FIELD_HEIGHT; i++ )
  {
    line_check = 0;

    for( byte k = 0; k < FIELD_WIDTH; k++ )
    {
      if( wall[k][i] != 0)  
      line_check++;
    }

    if( line_check == FIELD_WIDTH )
    {
      flashLine( i );
      for( int  k = i; k >= 0; k-- )
      {
        for( byte m = 0; m < FIELD_WIDTH; m++ )
        {
          if( k > 0)
          {
            wall[m][k] = wall[m][k-1];
          }
          else
          {
            wall[m][k] = 0;
          }
        }
      }

      return true; //line removed.
    }
  }
  return false; //no complete line found
}

//randomly selects a new brick and resets rotation / position.
void nextBrick(){
  Serial.print(F("Next Brick"));
  Serial.print(F(", useAI mode = ")); 
  Serial.print(useAi); 
  Serial.print(F(", idleBricks = ")); 
  Serial.println(idleBricks); 
  Serial.print(F("bounce_tick = "));
  Serial.print(bounce_tick);
  Serial.print(F(" millis() = "));
  Serial.println(millis());

  numberOfBricksInGame++;
  Serial.print(F("numberOfBricksInGame = "));
  Serial.println(numberOfBricksInGame);

  if (!useAi) {
    numberOfBricksSinceAI++;
    if (numberOfBricksSinceAI > (ADD_EXTRA_BRICK_COUNT * 1.5)) {
      brick_count = sizeof(bricks)/sizeof(bricks[0]); 
    } else if (numberOfBricksSinceAI > ADD_EXTRA_BRICK_COUNT) {
      brick_count = sizeof(bricks)/sizeof(bricks[0])  - (EXTRABRICKS / 2); 
    } else {
      brick_count = (sizeof(bricks)/sizeof(bricks[0])) - EXTRABRICKS; 
    }
  } else {
    numberOfBricksSinceAI = 0;
    brick_count = (sizeof(bricks)/sizeof(bricks[0])) - EXTRABRICKS; 
  }
  Serial.print(F("numberOfBricksSinceAI = "));
  Serial.println(numberOfBricksSinceAI);

  Serial.print(F("Numbers of Bricks in Play = "));
  Serial.println(brick_count);
	
  if (!useAi) {
    idleBricks++;
    if (idleBricks > MAX_IDLEBRICKS) {
      useAi = !useAi;
      Serial.print(F("MAX_IDLEBRICKS of ")); 
      Serial.print(MAX_IDLEBRICKS); 
      Serial.println(F(" exceeded")); 
      Serial.println(F("Swithing to AI mode!")); 
      idleBricks = 0;
    }
    Serial.print(F("idle manual bricks = ")); 
    Serial.println(idleBricks);
  }
  currentBrick.rotation = 0;
  currentBrick.positionX = round(FIELD_WIDTH / 2) - 2;
  currentBrick.positionY = -3;


  currentBrick.type = random( 0, brick_count );

  currentBrick.color = pgm_read_byte(&(brick_colors[ currentBrick.type ]));

  aiCalculatedAlready = false;

  updateBrickArray();

  //displayPreview();
}

//effect, flashes the line at the given y position (line) a few times.  
void flashLine( int line ){

  bool state = 1;
  for(byte i = 0; i < 6; i++ )
  {
    for(byte k = 0; k < FIELD_WIDTH; k++ )
    {  
      if(state)
      wall[k][line] = 0b11111111;
      else
      wall[k][line] = 0;
      
    }
    state = !state;
    drawWall();
    updateDisplay();
    delay(200);
  }

}


//draws wall only, does not update display
void drawWall(){
  for(int j=0; j < FIELD_WIDTH; j++){
    for(int k = 0; k < FIELD_HEIGHT; k++ )
    {
      draw(wall[j][k],FULL_INTENSITY,j,k);
    }
    
  }

}

//'Draws' wall and game piece to screen array 
void drawGame()
{

  //draw the wall first
  drawWall();

  //now draw current piece in play
  for( int j = 0; j < 4; j++ )
  {
    for( int k = 0; k < 4; k++ )
    {
      if(currentBrick.pattern[j][k] != 0)
      {
        if( currentBrick.positionY + k >= 0 )
        {
          draw(currentBrick.color, FULL_INTENSITY, currentBrick.positionX + j, currentBrick.positionY + k);
          //field[ positionX + j ][ p osition_y + k ] = currentBrick_color;
        }
      }
    }
  }
  updateDisplay();
}

//takes a byte color values an draws it to pixel array at screen x,y values.
// Assumes a Down->UP->RIGHT->Up->Down->etc (Shorest wire path) LED strips display.
//new brightness value lets you dim LEDs w/o changing color.
/*
void draw(byte color, signed int brightness, byte x, byte y){
  
  unsigned short address=0;
  byte r,g,b;
  
  //flip y for new tie layout. remove if your strips go up to down
  y = (FIELD_HEIGHT-1) - y;
  
  //calculate address
  if(x%2==0) //even row
  address=FIELD_HEIGHT*x+y;
  else //odd row
  address=((FIELD_HEIGHT*(x+1))-1)-y;
  
  if(color==0 || brightness < 0){
    strip.setPixelColor(address, 0);
  }
  else{
    //calculate colors, map to LED system
    b=color&0b00000011;
    g=(color&0b00011100)>>2;
    r=(color&0b11100000)>>5;
    
    //make sure brightness value is correct
    brightness=constrain(brightness,0,FULL_INTENSITY);
    
    strip.setPixelColor(address, map(r,0,7,0,brightness), map(g,0,7,0,brightness), map(b,0,3,0,brightness));

  }
  
}
*/
void draw(byte color, signed int brightness, byte x, byte y){
  byte r,g,b;
  //flip y for new tie layout. remove if your strips go up to down
  y = (FIELD_HEIGHT-1) - y;
  
  //calculate address
  //if(x%2==0) //even row
  //address=FIELD_HEIGHT*x+y;
  //else //odd row
  //address=((FIELD_HEIGHT*(x+1))-1)-y;
  uint16_t address = computeAddress(y,x);
  if(color==0 || brightness < 0){
    strip.setPixelColor(address, 0);
  }
  else{
    //calculate colors, map to LED system
    b=color&0b00000011;
    g=(color&0b00011100)>>2;
    r=(color&0b11100000)>>5;

    //make sure brightness value is correct
    brightness=constrain(brightness,0,FULL_INTENSITY);
    
    strip.setPixelColor(address, map(r,0,7,0,brightness), map(g,0,7,0,brightness), map(b,0,3,0,brightness));

  }
  
}
//obvious function
void gameOver()
{
  Serial.println(F("GAME Over"));
//  Serial.println(F("Free RAM = ")); //F function does the same and is now a built in library, in IDE > 1.0.0
//  Serial.println(freeMemory(), DEC);  // print how much RAM is available.
  Serial.print(F("bounce_tick = "));
  Serial.print(bounce_tick);
  Serial.print(F(" millis() = "));
  Serial.println(millis());

  int y;
  for ( y = 0; y < FIELD_HEIGHT; y++ ) {
	  colorRow(Color(FULL_INTENSITY, 0, 0), y);
	  strip.show();
	  delay(80);
  }
  fadeGrid(Color(FULL_INTENSITY, 0, 0), Color(FULL_INTENSITY,FULL_INTENSITY,FULL_INTENSITY),0, 100);
  fadeGrid(Color(FULL_INTENSITY,FULL_INTENSITY,FULL_INTENSITY), Color(0,0,0), 8, 200);
  delay(1500);
  //dissolveGrid(5, 250);

  newGame();

}

//clean up, reset timers, scores, etc. and start a new round.
void newGame()
{
  numberOfBricksInGame = 0;
  Serial.print(F("New GAME"));
  Serial.print(F(", useAI mode = ")); 
  Serial.println(useAi); 
	Serial.print(F("bounce_tick = "));
	Serial.print(bounce_tick);

	Serial.print(F(" millis() = "));
	Serial.println(millis());
  aiCalculatedAlready = false;
  //  level = 0;
  // ticks = 0;
  //score = 0;
  //score_lines = 0;
  //last_key = 0;
  bounce_tick = millis() + BOUNCE_DELAY;
  Serial.print(F("bounce_tick = "));
  Serial.print(bounce_tick);
  clearWall();

  nextBrick();
}

//Update LED strips
void updateDisplay(){

  strip.show();
  
  
}
uint32_t Color(byte r, byte g, byte b) {
	uint32_t c;
	c = r;
	c <<= 8;
	c |= g;
	c <<= 8;
	c |= b;
	return c;
}
void colorGrid(uint32_t color) {
	int i;
	for (i=0; i < strip.numPixels(); i++) {
		strip.setPixelColor(i, color);
	}
}

void colorRow(uint32_t color, int row) {
	int x;

	for ( x = 0; x < FIELD_WIDTH; x++ ) {
		strip.setPixelColor(computeAddress(row, x), color);
	}
}

void fadeGrid(uint32_t s_color, uint32_t e_color, uint16_t pause, float steps) {
	float s_color_r = (( s_color >> 16 ) & 0xFF);
	float s_color_g = (( s_color >> 8 ) & 0xFF);
	float s_color_b = ( s_color & 0xFF );

	float e_color_r = (( e_color >> 16 ) & 0xFF);
	float e_color_g = (( e_color >> 8 ) & 0xFF);
	float e_color_b = ( e_color & 0xFF );
	uint32_t currentColor = s_color;
	long i;
	for ( i = 0; i <= steps; i++) {
		//currentColor = map(i, 0, steps, min(s_color, e_color), max(s_color, e_color) );
		//colorGrid(currentColor);

		//Serial.println((s_color_r + ((e_color_r - s_color_r) / steps)*i));
		colorGrid(Color((s_color_r + ((e_color_r - s_color_r) / steps)*i),(s_color_g + ((e_color_g - s_color_g) / steps)*i),(s_color_b + ((e_color_b - s_color_b) / steps)*i)));
		strip.show();
		delay(pause);
	}
}

void dissolveGrid(uint16_t pause, uint16_t steps) {
	int i;
	for (i = 0; i<steps; i++) {
		strip.setPixelColor(random(0, strip.numPixels()), 0);
		strip.show();
		delay(pause);
	}
}
