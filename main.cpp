
/*
	NOTE: Requires at least C++17 (Properties --> C/C++ --> Language --> C++ Language Standard)
	Uses a slightly modified version of olcPixelGameEngine.h in order to have more control 
	over the window title.
*/

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#include <algorithm>
#include <chrono>
#include <fstream>

#include "tinyfiledialogs.h"

constexpr int nWorldWidth = 50;
constexpr int nWorldHeight = 50;

template<int width, int height>
class PixelWire : public olc::PixelGameEngine
{
private:

	bool m_bDidSplashScreen;
	float m_fSplashScreenDuration;
#ifdef _DEBUG
	const int SPLASH_SCREEN_SECONDS = 0;
#else
	const int SPLASH_SCREEN_SECONDS = 3;
#endif

	struct Camera
	{
		float x = -width / 2.0f;
		float y = -height / 2.0f;
		float scale = 2.0f;
		float snappedScale;

		inline olc::vi2d WorldSpaceToScreenSpace(olc::vi2d position)
		{
			olc::vi2d worldSpace = { (int)(((float)position.x) / (float)nWorldWidth * (float)width * snappedScale),
									 (int)((float)position.y / (float)nWorldHeight * (float)height * snappedScale)};

			worldSpace.x += (int)x; worldSpace.y += (int)y;

			return worldSpace;
		}

		inline olc::vf2d ScreenSpaceToWorldSpace(olc::vi2d position)
		{
			olc::vf2d screenSpace = { (float)position.x, (float)position.y };
			screenSpace.x -= x; screenSpace.y -= y;
			screenSpace /= snappedScale;
			screenSpace.x /= (float)width; screenSpace.y /= (float)height;
			screenSpace.x *= (float)nWorldWidth; screenSpace.y *= (float)nWorldHeight;

			return screenSpace;
		}

		inline olc::vi2d GetCellSize()
		{
			olc::vi2d cellSize = olc::vi2d((int)((float)width / (float)nWorldWidth * snappedScale), (int)((float)height / (float)nWorldHeight * snappedScale));
			return cellSize;
		}
	};
	Camera m_Camera;

	struct Cell
	{
		enum State { EMPTY, HEAD, TAIL, CONDUCTOR };
		State state = EMPTY;

		olc::Pixel GetColour()
		{
			switch (state)
			{
				case Cell::EMPTY:		return olc::BLACK;
				case Cell::HEAD:		return olc::BLUE;
				case Cell::TAIL:		return olc::RED;
				case Cell::CONDUCTOR:	return olc::YELLOW;
			}
			return olc::RED;
		}

		const std::string GetName()
		{
			switch (state)
			{
				case Cell::EMPTY:		return "Empty";
				case Cell::HEAD:		return "Head";
				case Cell::TAIL:		return "Tail";
				case Cell::CONDUCTOR:	return "Conductor";
			}
			return "None";
		}
	};
	Cell** m_World; // 2D array
	Cell** m_WorldLogic; // 2D array

	bool bPaused = true;
	Cell m_SelectedCell;
	float m_fTargetTicksPerSecond = 5.0f;
	float m_fTicksSinceLastUpdate = 0.0f;

public:

	PixelWire()
	{
		
	}

	bool OnUserCreate() override
	{
		// Set window title, start splashscreen
		sAppName = "PixelWire - olcCodeJam2020";
		m_bDidSplashScreen = false;

		// Allocate space for world
		m_World = new Cell*[nWorldWidth];
		for (int i = 0; i < nWorldWidth; ++i) m_World[i] = new Cell[nWorldHeight];
		m_WorldLogic = new Cell*[nWorldWidth];
		for (int i = 0; i < nWorldWidth; ++i) m_WorldLogic[i] = new Cell[nWorldHeight];

		// Selected cell type
		m_SelectedCell.state = Cell::HEAD;

		return true;
	}

	bool DrawSplashScreen(float fElapsedTime)
	{
		const olc::vf2d scale = olc::vf2d(1.0f, 1.0f);

		const std::string sSplashScreen[3] = { "Made using the olc::PixelGameEngine", "Copyright 2018 - 2020 OneLoneCoder.com", "WARNING: MAY CONTAIN FLASHING IMAGES!" };

		// Draw splash screen
		Clear(olc::BLACK);
		DrawStringDecal(olc::vf2d(ScreenWidth() / 2.0f - GetTextSize(sSplashScreen[0]).x / 2.0f, ScreenHeight() / 2.0f - GetTextSize(sSplashScreen[0]).y), sSplashScreen[0], olc::YELLOW, scale);
		DrawStringDecal(olc::vf2d(ScreenWidth() / 2.0f - GetTextSize(sSplashScreen[1]).x / 2.0f, ScreenHeight() / 2.0f + 10.0f - GetTextSize(sSplashScreen[1]).y), sSplashScreen[1], olc::WHITE, scale);
		DrawStringDecal(olc::vf2d(ScreenWidth() / 2.0f - GetTextSize(sSplashScreen[2]).x / 2.0f, ScreenHeight() / 2.0f + 20.0f - GetTextSize(sSplashScreen[2]).y), sSplashScreen[2], olc::RED, scale);

		// Keep track of time
		m_fSplashScreenDuration += fElapsedTime;
		if (m_fSplashScreenDuration >= SPLASH_SCREEN_SECONDS) m_bDidSplashScreen = true;

		return true;
	}

	void SaveWorldToFile()
	{
		char const* fileTypes[1] = { "*.wire" };
		char* sFilePath = tinyfd_saveFileDialog("Select a .wire file to save", NULL, 1, fileTypes, NULL);
		if (sFilePath == NULL) return;

		std::ofstream sFile;
		sFile.open(sFilePath);
		for (int y = 0; y < nWorldHeight; ++y)
			for (int x = 0; x < nWorldWidth; ++x)
				sFile << (int)m_World[x][y].state;
		sFile.close();
	}

	void ReadWorldFromFile()
	{
		char const* fileTypes[1] = { "*.wire" };
		char* sFilePath = tinyfd_openFileDialog("Select a .wire file to load", NULL, 1, fileTypes, NULL, 0);
		if (sFilePath == NULL) return;

		std::ifstream sFile;
		sFile.open(sFilePath);
		
		std::string line;
		if (!std::getline(sFile, line) || line.length() != nWorldWidth * nWorldHeight)
		{
			tinyfd_messageBox("Error", std::string("Invalid file - is it one long line " + std::to_string(nWorldWidth*nWorldHeight) +
				" characters long consisting of numbers 0 through to 3?").c_str(), "ok", "error", 1);
			return;
		}
		
		for (int x = 0; x < nWorldWidth; ++x)
			for (int y = 0; y < nWorldHeight; ++y)
				m_World[x][y].state = (PixelWire<width, height>::Cell::State) (line[y*nWorldWidth + x] - '0');
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		// Camera movement
		const float fCameraSpeed = 80.0f;
		if (GetKey(olc::W).bHeld || GetKey(olc::UP).bHeld)		m_Camera.y += fCameraSpeed * fElapsedTime;
		if (GetKey(olc::A).bHeld || GetKey(olc::LEFT).bHeld)	m_Camera.x += fCameraSpeed * fElapsedTime;
		if (GetKey(olc::S).bHeld || GetKey(olc::DOWN).bHeld)	m_Camera.y -= fCameraSpeed * fElapsedTime;
		if (GetKey(olc::D).bHeld || GetKey(olc::RIGHT).bHeld)	m_Camera.x -= fCameraSpeed * fElapsedTime;
		if (GetKey(olc::CTRL).bPressed) { m_Camera.x = -width / 2.0f; m_Camera.y = -height/2.0f; m_Camera.scale = 2.0f; }
		
		// File IO
		if (GetKey(olc::Z).bPressed) ReadWorldFromFile();
		if (GetKey(olc::X).bPressed) SaveWorldToFile();

		//if (GetMouseWheel() != 0) m_Camera.scale += GetMouseWheel() * 0.1f * fElapsedTime;
		m_Camera.scale = std::clamp(m_Camera.scale, 2.0f, 5.0f);
		m_Camera.snappedScale = roundf(m_Camera.scale / 1.1f) * 1.1f;

		// Simulation control
		if (GetKey(olc::SPACE).bPressed) bPaused = !bPaused;
		if (GetKey(olc::F1).bPressed) m_SelectedCell.state = Cell::EMPTY;
		if (GetKey(olc::F2).bPressed) m_SelectedCell.state = Cell::HEAD;
		if (GetKey(olc::F3).bPressed) m_SelectedCell.state = Cell::TAIL;
		if (GetKey(olc::F4).bPressed) m_SelectedCell.state = Cell::CONDUCTOR;
		if (GetKey(olc::F5).bPressed) m_fTargetTicksPerSecond = 1;
		if (GetKey(olc::F6).bPressed) m_fTargetTicksPerSecond = 5;
		if (GetKey(olc::F7).bPressed) m_fTargetTicksPerSecond = 10;
		if (GetKey(olc::F8).bPressed) m_fTargetTicksPerSecond = 50;
		if (GetKey(olc::F9).bPressed) m_fTargetTicksPerSecond = 10000;
		if (GetKey(olc::ESCAPE).bPressed)
		{
			for (int x = 0; x < nWorldWidth; ++x)
				for (int y = 0; y < nWorldHeight; ++y)
					m_World[x][y].state = Cell::EMPTY;
		}

		// Mouse selection
		if (GetMouse(0).bPressed || GetMouse(1).bHeld)
		{
			// Convert mouse position from screen space to world space
			olc::vf2d position = m_Camera.ScreenSpaceToWorldSpace(olc::vi2d(GetMouseX(), GetMouseY()));
			
			// Alter cell state if position is within reasonable confines
			if (position.x < nWorldWidth && position.y < nWorldWidth && position.x > 0 && position.y > 0)
				m_World[(int)position.x][(int)position.y].state = m_SelectedCell.state;
		}

		// Do cellular logic
		auto tLogicBegin = std::chrono::high_resolution_clock::now();
		float fTpsMillisecondDelay = 1.0f / m_fTargetTicksPerSecond;
		if (!bPaused && m_fTicksSinceLastUpdate >= fTpsMillisecondDelay)
		{
			// Copy array to output
			for (int x = 0; x < nWorldWidth; ++x)
				for (int y = 0; y < nWorldHeight; ++y)
					m_WorldLogic[x][y] = m_World[x][y];

			// Iterate through each cell
			for (int x = 0; x < nWorldWidth; ++x)
				for (int y = 0; y < nWorldHeight; ++y)
				{
					auto cellState = m_World[x][y].state;
					if (cellState == Cell::CONDUCTOR)
					{
						// Determine amount of neighbours that are heads
						int nHeads = 0;
						for (int headX = x - 1; headX <= x + 1; ++headX)
						{
							for (int headY = y - 1; headY <= y + 1; ++headY)
							{
								if (headX < 0 || headY < 0 || headX >= nWorldWidth || headY >= nWorldHeight) continue;
								else if (m_World[headX][headY].state == Cell::HEAD) nHeads++;
							}
						}

						// Switch state if nessecary
						if (nHeads == 1 || nHeads == 2) cellState = Cell::HEAD;
					}
					else if (cellState == Cell::HEAD) cellState = Cell::TAIL;
					else if (cellState == Cell::TAIL) cellState = Cell::CONDUCTOR;

					m_WorldLogic[x][y].state = cellState;
				}

			// Copy output to array
			for (int x = 0; x < nWorldWidth; ++x)
				for (int y = 0; y < nWorldHeight; ++y)
					m_World[x][y] = m_WorldLogic[x][y];

			m_fTicksSinceLastUpdate = 0.0f;
		}
		else m_fTicksSinceLastUpdate += fElapsedTime;
		auto tLogicEnd = std::chrono::high_resolution_clock::now();

		// Begin drawing
		auto tDrawingBegin = std::chrono::high_resolution_clock::now();
		Clear(olc::GREY);

		// Draw splash screen, otherwise carry on
		if (!m_bDidSplashScreen) return DrawSplashScreen(fElapsedTime);

		// Draw world grid
		for (int x = 0; x < nWorldWidth; ++x)
		{
			for (int y = 0; y < nWorldHeight; ++y)
			{
				// Determine cell colour based on state
				olc::Pixel colour = m_World[x][y].GetColour();
				
				// "Cull" cells outside view space for performance
				olc::vi2d cellPosition = m_Camera.WorldSpaceToScreenSpace(olc::vi2d(x, y));
				if (cellPosition.x < -m_Camera.GetCellSize().x || cellPosition.y < -m_Camera.GetCellSize().y || cellPosition.x > width || cellPosition.y > height) continue;

				// Draw cell one smaller than it should be, for nice border
				FillRect(cellPosition + olc::vi2d{ 1, 1 }, m_Camera.GetCellSize() - olc::vi2d {1, 1}, colour);
			}
		}

		// Draw pause or play icon
		if (bPaused)
		{
			FillRect(olc::vi2d(width - 20, 5), olc::vi2d(5, 20));
			FillRect(olc::vi2d(width - 10, 5), olc::vi2d(5, 20));
		}
		else
		{
			FillTriangle(olc::vi2d(width - 20, 5), olc::vi2d(width - 20, 20), olc::vi2d(width - 5, 12));
		}

		// Right panel background (also hides imperfections)
		FillRect(olc::vi2d(width+1, 0), olc::vi2d(width-1, height), olc::BLACK);

		// Draw right hand information pane
		const olc::vf2d scale = { 0.7f, 0.7f };

		DrawStringDecal(olc::vf2d(width + 10.0f, 10.0f), "Wireworld cellular automaton", olc::RED, { 1.0f, 1.0f } );

		DrawStringDecal(olc::vf2d(width + 10.0f, 30.0f), "Press space to pause and unpause the\nsimulation.", olc::WHITE, scale);
		DrawStringDecal(olc::vf2d(width + 10.0f, 50.0f), "Use F1 through F4 to change the selected\ncell type, and F5 through F9 to change\nthe speed of the simulation.", olc::WHITE, scale);
		DrawStringDecal(olc::vf2d(width + 10.0f, 80.0f), "WASD or the arrow keys can move the camera around.", olc::WHITE, scale);
		DrawStringDecal(olc::vf2d(width + 10.0f, 100.0f), "If you should get lost, pressing ctrl will\nreset the camera view.", olc::WHITE, scale);
		DrawStringDecal(olc::vf2d(width + 10.0f, 120.0f), "Use the left mouse button to press\nindividual cells, and hold down the right\nmouse button to place many at a time.", olc::WHITE, scale);
		DrawStringDecal(olc::vf2d(width + 10.0f, 150.0f), "Pressing escape will clear all cells.", olc::WHITE, scale);
		
		// Rules
		DrawStringDecal(olc::vf2d(width + 10.0f, 170.0f), "Rules:", olc::YELLOW, scale);
		DrawStringDecal(olc::vf2d(width + 10.0f, 180.0f), "* An empty cell stays empty", olc::YELLOW, scale);
		DrawStringDecal(olc::vf2d(width + 10.0f, 190.0f), "* A head becomes a tail", olc::YELLOW, scale);
		DrawStringDecal(olc::vf2d(width + 10.0f, 200.0f), "* A tail becomes a conductor", olc::YELLOW, scale);
		DrawStringDecal(olc::vf2d(width + 10.0f, 210.0f), "* A conductor becomes a head if either one or two\nneighbouring cells are also heads", olc::YELLOW, scale);

		// More instructions
		DrawStringDecal(olc::vf2d(width + 10.0f, 240.0f), "Press z to load a world and x to save one.", olc::WHITE, scale);

		// Draw right hand bottom panel
		olc::Pixel colour = m_SelectedCell.GetColour();
		FillRect(olc::vi2d(width + 10, height - 25), olc::vi2d(20, 20), colour);
		DrawRect(olc::vi2d(width + 9,  height - 26), olc::vi2d(21, 21), olc::GREY);
		DrawStringDecal(olc::vf2d(width + 35.0f, height - 12.5f), "Selected type: " + m_SelectedCell.GetName(), olc::WHITE, scale);
		DrawStringDecal(olc::vf2d(width + 35.0f, height - 25.0f), "Target TPS: " + std::to_string((int)m_fTargetTicksPerSecond), olc::WHITE, scale);

		// Performance statistics
		auto tDrawingEnd = std::chrono::high_resolution_clock::now();
		auto fLogicDuration = std::chrono::duration_cast<std::chrono::milliseconds>(tLogicEnd - tLogicBegin).count();
		auto fDrawingDuration = std::chrono::duration_cast<std::chrono::milliseconds>(tDrawingEnd - tDrawingBegin).count();
		DrawStringDecal(olc::vf2d(width + 200.0f, height - 12.5f), "Logic took " + std::to_string(fLogicDuration) + " ms", olc::WHITE, scale);
		DrawStringDecal(olc::vf2d(width + 200.0f, height - 25.0f), "Drawing took " + std::to_string(fDrawingDuration) + "ms", olc::WHITE, scale);

		return true;
	}

	bool OnUserDestroy() override
	{
		// Destroy world;
		for (int i = 0; i < nWorldWidth; ++i) delete[] m_World[i];
		delete[] m_World;
		for (int i = 0; i < nWorldWidth; ++i) delete[] m_WorldLogic[i];
		delete[] m_WorldLogic;
		
		return true;
	}

	constexpr int GetWidth() { return width * 2; }
	constexpr int GetHeight() { return height; }

};


int main()
{

	PixelWire<300, 300> window;

	if (window.Construct(window.GetWidth(), window.GetHeight(), 3, 3, false, true))
		window.Start();

	return 0;
}