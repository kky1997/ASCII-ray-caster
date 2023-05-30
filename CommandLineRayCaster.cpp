/*
    ASCII ray caster using console screen buffer provided by the Windows library.

    Free software originall by Javidx9 

    Slightly modified to run on std:c++17, which will now give an byte ambiguous error if declaring "using namespace std;" before include <Windows.h>

    If running on previous version (up to std:c++14), can declare namespace std; before including windows header.

    Also changed the map layout so that there is no possiblity that the player can move beyond the map and into an eternal void.

    NOTE: console size should be 120 * 40 (to match screen width and height)
    
    
    Quick overview: 
        This raycasting program uses a "map" wstring, which defines the layout of the map. By determining the players position in the map, we use raycasting
        to sample the viewport of the player within the map and render walls, floor, and ceiling accordingly onto the "screen" array which is what is written
        to the console screen buffer.
    
    Definition of Raycasting:

        Raycasting refers to the technique used to simulate a 3D environment by casting rays from the player's position and determining the 
        distance to walls in the virtual world.
        The program uses raycasting to create a simple 3D rendering of a maze-like environment. It calculates the distance from the player's position 
        to each wall in the scene by incrementally casting rays and checking for intersections with wall blocks. The program then uses this distance information 
        to shade the walls and create a 3D effect.

        Here's how raycasting works in the program:

        During each iteartion of the for-loop a single ray is cast for a column of the pixels on the screen, the for-loop iterats from left to right,
        ensuring that each column has a single ray cast to it.

        1. Calculation of ray angle:

            Ray angle (fRayAngle) is calculated based on player's viewing angle (fPlayerA) and their field of view (fFOV):

                The forumla "(fPlayerA - fFOV/2.0f) + ((float)x / (float)nScreenWidth) * fFOV" is used to determine the angle of the ray projected
                into the map from the player's perspective
                    we bisect the players FOV so that we have a left side and a right side of the FOV and we iterate from 0 - 119, (the screenwidth) 
                    for every column on the screen. This means we calculate 120 angles for 120 rays for each column
        
        2. Calculation of Unit Vectors:

            we calculate fEyeX and fEyeY using sinf() and cosf() respectively. We determine these at each iteration of the loop 
            using the ray angles calculated above. These units represnet the direction in which the ray is cast from the player's viewpoint.
            They are used to get the x and y position of the ray as it samples the game world during the path of the ray.
            The ray moves at 0.1 step intervals, allowing use to check at every StepSize (0.1) what is in the game world (is it wall or not, shading of the wall, floor and ceiling)
            
        3. Ray casting:

            the ray is ast incrementally from the player's position along the calculated ray angle. We check if the ray has met a wall or boundry by checking
            corresponding cells in the "map" array.

            We used nTestX and nTestY derived from: 
                fPlayerX + fEyeX * fDistanceToWall
                fPlayerY + fEyeY * fDistanceToWall respectively,
            In order to check if the ray has hit a wall in the map array (map.c_str()[nTestX * nMapWidth + nTestY] == '#'). If it has we set the bHitWall flag.

        4. Boundary detection:

            If the ray hits awall, we check if it has hit the boundary between two wall blocks by casting four rays, one from each corner of the wall tile hit, towards
            the players position.

            The distance from the player and the dot product (how coincident) between the rendering ray (from player) and the corner rays are calculated.

            by comparing these values, the closest corners to the player are determed and if their dot product is within a certain threshold, the bBoundary flag is set.


    
*/
#include <iostream>
#include <cmath> //for trigonometry functions
#include <vector>
#include <utility>
#include <algorithm>
#include <chrono>


#include <stdio.h>
#include <Windows.h>
using namespace std; //changed namespace std declaration to be after Windows.h header to fix byte amgious error.

int nScreenWidth = 120;			// Console Screen Size X (columns)
int nScreenHeight = 40;			// Console Screen Size Y (rows)
int nMapWidth = 16;				// World Dimensions
int nMapHeight = 16;

//use floats for more precise player positions
float fPlayerX = 14.7f;			// Player Start Position
float fPlayerY = 5.09f;
float fPlayerA = 0.0f;			// Player Start Rotation
float fFOV = 3.14159f / 4.0f;	// Field of View (pi/4)
float fDepth = 16.0f;			// Maximum rendering distance
float fSpeed = 5.0f;			// Walking Speed

int main()
{
	// Create Screen Buffer (using unicode, hence wchar, wprint, etc), initilize screen* ptr to wchar array the size of area of screen
	wchar_t *screen = new wchar_t[nScreenWidth*nScreenHeight];

    /*
    using Wndows library function, create a console screen buffer (this is just a text mode buffer to display textual information and recieve user input)
    to command line environment
    */
	HANDLE hConsole = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);

    //set the above buffer as the tragetd buffer of the console
	SetConsoleActiveScreenBuffer(hConsole);
	DWORD dwBytesWritten = 0;

	// Create Map of world space # = wall block, . = space
    //L prefix indices string literals are wide character strings
    //we use '#' and '.' which are part of the ASCII character set
    //which can actually just be stored in a std::string, but we use wstring and treat them as unicode
	wstring map;
	map += L"################";
	map += L"#..............#";
	map += L"#.......########";
	map += L"#..............#";
	map += L"#......##......#";
	map += L"#......##......#";
	map += L"#..............#";
	map += L"###............#";
	map += L"##.............#";
	map += L"#......####..###";
	map += L"#......#.......#";
	map += L"#......#.......#";
	map += L"#..............#";
	map += L"#......#########";
	map += L"#..............#";
	map += L"################";

    // two timepoints to get an elapsed time
	auto tp1 = chrono::system_clock::now();
	auto tp2 = chrono::system_clock::now();
	
	while (1)
	{
		// We'll need time differential per frame to calculate modification
		// to movement speeds, to ensure consistant movement, as ray-tracing
		// is non-deterministic
		tp2 = chrono::system_clock::now(); // grab current system time
		chrono::duration<float> elapsedTime = tp2 - tp1; // get elapsed time between current and previous system time
		tp1 = tp2; //update tp1 to hold previous system time
		float fElapsedTime = elapsedTime.count(); // get the elapsed time as float (in seconds)


		// Handle CCW Rotation
        // using A key, decrease players angle by 0.75 (to the left)
		if (GetAsyncKeyState((unsigned short)'A') & 0x8000)
			fPlayerA -= (fSpeed * 0.75f) * fElapsedTime;

		// Handle CW Rotation
        // using D key, increase players angle by 0.75 (to the right)
		if (GetAsyncKeyState((unsigned short)'D') & 0x8000)
			fPlayerA += (fSpeed * 0.75f) * fElapsedTime;
		
		// Handle Forwards movement & collision
		if (GetAsyncKeyState((unsigned short)'W') & 0x8000)
		{
            // modify players x and y position by changing unit vecctor to see what where the rays are being cast
            // we += to x and y as we are moving forward and player's x and y is approaching the bound of the map (16 height and width)
            // this is in respect to fPlayerA's current rotation (so where they are facing)
			fPlayerX += sinf(fPlayerA) * fSpeed * fElapsedTime;;
			fPlayerY += cosf(fPlayerA) * fSpeed * fElapsedTime;;
			if (map.c_str()[(int)fPlayerX * nMapWidth + (int)fPlayerY] == '#')
			{
				fPlayerX -= sinf(fPlayerA) * fSpeed * fElapsedTime;;
				fPlayerY -= cosf(fPlayerA) * fSpeed * fElapsedTime;;
			}			
		}

		// Handle backwards movement & collision
		if (GetAsyncKeyState((unsigned short)'S') & 0x8000)
		{
            //modify players x and y position by changing unit vecctor to see what where the rays are being cast
            // we -= to x and y as we are moving away and player's x and y is moving backwards from bound of the map (16 height and width)
            // this is in respect to fPlayerA's current rotation (so where they are facing)
			fPlayerX -= sinf(fPlayerA) * fSpeed * fElapsedTime;;
			fPlayerY -= cosf(fPlayerA) * fSpeed * fElapsedTime;;
			if (map.c_str()[(int)fPlayerX * nMapWidth + (int)fPlayerY] == '#')
			{
				fPlayerX += sinf(fPlayerA) * fSpeed * fElapsedTime;;
				fPlayerY += cosf(fPlayerA) * fSpeed * fElapsedTime;;
			}
		}

        /* this for loop is for ray casting from current players rotation as well as their x and y position
           for x = 0 till screenWidth (left to right), every column of "pixels" (unicode) has a ray shot to it
           this algorithm calcualtes ray angle to evenly distribute the ray across trhe field of view 
           each iteration casts a single ray to a column of pixels on the screen
           the unit vectors represent the direction the ray is cast.
           if the ray hits a wall ("#"), the bHitWall flag is set.

           Boundary detection also occurs in this loop:
            1. When a ray hits a wall, 4 additional rays are cast from each corner of the wall tile towards
            the player position. 

            2. The distnace between the player and each corner is calculate, this is to indicate how far away each corner is from player's position.
           
            3. dot product between the rendering ray (main ray from player) and each corner ray is calcuate, measuring the similariting or alignment 
               between the two vectors.

            4. by comparing the distances and dot products of the corners, we can determine which corners are closest to the player. The closest corners
               are the ones that will have the most impact on the rendering of the wall.

            5. if the dot product of any of the closest corners is within a certain threshold (meaning aligned or similar), this indicats the ray is hitting a boundary
               between two wall blocks. Hence, set bBoundary flag to true.
           

        */
		for (int x = 0; x < nScreenWidth; x++)
		{
			// For each column, calculate the projected ray angle into world space from the player's perspective
			float fRayAngle = (fPlayerA - fFOV/2.0f) + ((float)x / (float)nScreenWidth) * fFOV;

			// Find distance to wall
			float fStepSize = 0.1f;		  // Increment size for ray casting, decrease to increase										
			float fDistanceToWall = 0.0f; //                                      resolution

			bool bHitWall = false;		// flag to signify ray has hit wall block (player hit the wall)
			bool bBoundary = false;		// Set when ray hits boundary between two wall blocks

            // these unit vectors represent the direction in which the ray is cast from player's viewpoint
            // as the ray advances, it samples the game world and determines the distance to objects/walls it encounters
            // allowing the proper rendering of the visuals on the screen.
			float fEyeX = sinf(fRayAngle); // Unit vector for horizontal ray movement
			float fEyeY = cosf(fRayAngle); // unit vector for vertical ray movement

			// Incrementally cast ray from player, along ray angle, testing for 
			// intersection with a block
            //fDepth is maximum width/height of map, so if we go beyond a wall, player is still stopped once they are at bounds of map, regardless of wall or not.
			while (!bHitWall && fDistanceToWall < fDepth) 
			{
				fDistanceToWall += fStepSize; //test at each step of 0.1, if ray has hit a wall

                // unit vector will grow as fDistanceToWall does, nTextX and nTestY hold the coordinates of the current sample point along the
                // ray being cast. This allows use to sample at every StepSize (0.1) what is in the game world (is it wall or not, shading of the wall, floor and ceiling)
				int nTestX = (int)(fPlayerX + fEyeX * fDistanceToWall);
				int nTestY = (int)(fPlayerY + fEyeY * fDistanceToWall);
				
				// Test if ray is out of bounds
				if (nTestX < 0 || nTestX >= nMapWidth || nTestY < 0 || nTestY >= nMapHeight)
				{
					bHitWall = true;			// Just set distance to maximum depth
					fDistanceToWall = fDepth;
				}
				else
				{
					// Ray is inbounds so test to see if the ray cell is a wall block
                    // using c_str() we get a pointer to the underlying char array of the map String (now can be accessed as a char array)
                    // by using x and y coordinates of ray (x * mapwidth + y), we can get the linear index of the array which represents the map
                    // hence if this index holds "#", we know the ray has hit a wall.
					if (map.c_str()[nTestX * nMapWidth + nTestY] == '#')
					{
						// Ray has hit wall
						bHitWall = true;
                

                //below is the boundary detection portion, above is the ray casting section.
                /*-----------------------------------------------------------------------------------------------------------------------------------*/


						// To highlight tile boundaries, cast a ray from each corner
						// of the tile, to the player. The more coincident this ray
						// is to the rendering ray, the closer we are to a tile 
						// boundary, which we'll shade to add detail to the walls
                        
                        //use vector p to store distance from player to each corner and calculated dot product
						vector<pair<float, float>> p;

						// Test each corner of hit tile, storing the distance from
						// the player, and the calculated dot product of the two rays
						for (int tx = 0; tx < 2; tx++)
							for (int ty = 0; ty < 2; ty++)
							{
								// Angle of corner to eye
								float vy = (float)nTestY + ty - fPlayerY;
								float vx = (float)nTestX + tx - fPlayerX;
								float d = sqrt(vx*vx + vy*vy); 
								float dot = (fEyeX * vx / d) + (fEyeY * vy / d);
								p.push_back(make_pair(d, dot));
							}

						// Sort Pairs from closest to farthest
						sort(p.begin(), p.end(), [](const pair<float, float> &left, const pair<float, float> &right) {return left.first < right.first; });
						
						// First two/three are closest (we will never see all four)
						float fBound = 0.01;
						if (acos(p.at(0).second) < fBound) bBoundary = true;
						if (acos(p.at(1).second) < fBound) bBoundary = true;
						if (acos(p.at(2).second) < fBound) bBoundary = true;
					}
				}
			}
		
			// Calculate distance to ceiling and floor, this gives the 3D illusion of higher ceiling to floor height at greatest distances from the wall
            // hence as distance to the wall gets larger, the less we subtract from the screen height, hence we have a height ceiling
            // As fDistanceToWall increases, the larger the denominator for nScreenHeight division, hence the term we minus by gets smaller.
			int nCeiling = (float)(nScreenHeight/2.0) - nScreenHeight / ((float)fDistanceToWall);

            //floor is same as ceiling, so we just use the calculated nCeiling to determin the floor height
			int nFloor = nScreenHeight - nCeiling;

			// Shader walls based on distance
			short nShade = ' '; //variable to hold the shaded extended ASCII character depending on distane to wall

            //set nShade depending on distance to wall and max rendering disntace (fDepth)
            //if distance to wall is > than max rendering distance, nShade is ' ' blank space
			if (fDistanceToWall <= fDepth / 4.0f)			nShade = 0x2588;	// Very close 	
			else if (fDistanceToWall < fDepth / 3.0f)		nShade = 0x2593;
			else if (fDistanceToWall < fDepth / 2.0f)		nShade = 0x2592;
			else if (fDistanceToWall < fDepth)				nShade = 0x2591;
			else											nShade = ' ';		// Too far away

			if (bBoundary)		nShade = ' '; // Black it out
			
            // iterating over each row till nScreenHeight, this loop is where the actual rendering is done into the screen array which is what
            // will be written to the console screen buffer.
			for (int y = 0; y < nScreenHeight; y++)
			{
				// Each Row if y <= to nCeiling, this pixel is represents the ceiling and is just empty space ' '
				if(y <= nCeiling)
					screen[y*nScreenWidth + x] = ' ';
				else if(y > nCeiling && y <= nFloor) //if y is ceiling and floor height, it is a wall, so we set the shading to the shades determined above
					screen[y*nScreenWidth + x] = nShade;
				else // else the pixel is floor and we can change the shading of the flood by using the nShade variable again
				{				
					// Shade floor based on distance
					float b = 1.0f - (((float)y -nScreenHeight/2.0f) / ((float)nScreenHeight / 2.0f));
					if (b < 0.25)		nShade = '#';
					else if (b < 0.5)	nShade = 'x';
					else if (b < 0.75)	nShade = '.';
					else if (b < 0.9)	nShade = '-';
					else				nShade = ' ';
					screen[y*nScreenWidth + x] = nShade; //assign the nShade for the floor with the screen array.
				}
			}
		}

		// Display Stats
		swprintf_s(screen, 40, L"X=%3.2f, Y=%3.2f, A=%3.2f FPS=%3.2f ", fPlayerX, fPlayerY, fPlayerA, 1.0f/fElapsedTime);

		// Display Map
		for (int nx = 0; nx < nMapWidth; nx++)
			for (int ny = 0; ny < nMapWidth; ny++)
			{
				screen[(ny+1)*nScreenWidth + nx] = map[ny * nMapWidth + nx];
			}
		screen[((int)fPlayerX+1) * nScreenWidth + (int)fPlayerY] = 'P';


		// Display Frame
        // assign the element of the array (length - 1 gives last index) the null character ('\0') as the WriteConsoleOutputCharacterW
        // requires a Null terminated string
		screen[nScreenWidth * nScreenHeight - 1] = '\0';

        //hConsole - handle to the console screen buffer, specifying which console screen buffer the function should write to
        //screen - array of characters that will be written to the console screen buffer (content to display on screen; needs a null terminated string)
        //nScreenWidth * nScreenHeight - specify the number of characters written from the screen array.
        //{0,0} - starting position in console screen buffer where characters should be written (0, 0 represents top left corner)
        //&dwBytesWritten - pass by reference, DWORD variable that recieves the number of characters aactually written to the console
		WriteConsoleOutputCharacterW(hConsole, screen, nScreenWidth * nScreenHeight, { 0,0 }, &dwBytesWritten);
	}

	return 0;
}