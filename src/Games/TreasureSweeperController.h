/***********************************************************************
TreasureSweeperController.h - Controller for Treasure Sweeper game mode
***********************************************************************/

#ifndef _TreasureSweeperController_h_
#define _TreasureSweeperController_h_

#include "../KinectProjector/KinectProjector.h"
#include <map>
#include <set>
#include <vector>

class CTreasureSweeperController
{
public:
	CTreasureSweeperController();
	virtual ~CTreasureSweeperController();

	void setup(std::shared_ptr<KinectProjector> const& k);
	void update();
	void drawProjectorWindow();
	void drawMainWindow(float x, float y, float width, float height);

	void setProjectorRes(ofVec2f& PR);
	void setKinectRes(ofVec2f& KR);
	void setKinectROI(ofRectangle& KROI);

	void toggleActive();
	bool isActive() const;
	void resetRound();

	// Returns true if key was handled by this controller.
	bool keyPressed(int key);

private:
	struct CellCoord
	{
		int row;
		int col;
		bool operator<(const CellCoord& other) const
		{
			if (row != other.row) return row < other.row;
			return col < other.col;
		}
		bool operator==(const CellCoord& other) const
		{
			return row == other.row && col == other.col;
		}
	};

	void startRound();
	bool computeCellMeans(std::vector<float>& means, float& minElevation, float& maxElevation);
	CellCoord indexToCell(int idx) const;
	int cellToIndex(const CellCoord& cell) const;
	CellCoord pickCellWithLargestDrop(const std::vector<float>& means, const std::vector<float>& baselines, float threshold, float& outDrop) const;
	void resolveDig(const CellCoord& cell, double nowSec);
	std::string warmthText(int manhattanDistance) const;
	int countMinesNearby(const CellCoord& cell) const;
	void drawCellMark(const CellCoord& cell, const std::string& result, bool emphasize) const;
	void drawFilledCellOverlay(const CellCoord& cell, const ofColor& color, float alphaScale) const;
	ofRectangle cellKinectBounds(const CellCoord& cell) const;
	ofVec2f cellProjectorCenter(const CellCoord& cell) const;
	void drawGrid() const;
	void drawHud() const;
	void maybeAutoReset(double nowSec);

	std::shared_ptr<KinectProjector> kinectProjector;

	ofVec2f projRes;
	ofVec2f kinectRes;
	ofRectangle kinectROI;

	bool active;
	bool initialized;

	std::vector<float> baselineMeans;
	std::vector<float> accumulatedDrops;
	CellCoord candidateCell;
	bool hasCandidate;
	int candidateFrames;
	double lastDigTime;
	std::map<int, double> lastDigTimeByCell;

	enum RoundState
	{
		ROUND_INTRO,
		ROUND_PLAYING,
		ROUND_WIN,
		ROUND_LOSE
	};

	RoundState roundState;

	enum LoseReason
	{
		LOSE_REASON_NONE,
		LOSE_REASON_MINE,
		LOSE_REASON_OUT_OF_DIGS
	};
	double roundStartTime;
	double stateEndTime;
	std::string lastMessage;
	int digsUsed;
	CellCoord treasureCell;
	std::set<int> mineCellIndices;
	std::set<int> dugCellIndices;
	std::map<int, std::string> resultByCell;
	float adaptiveDropThreshold;
	bool revealMines;
	bool cheatRevealAll;
	LoseReason loseReason;
	CellCoord lastMineHitCell;
	double mineFlashEndTime;
};

#endif
