#include "TreasureSweeperController.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>
#include <random>

namespace
{
	// Treasure Sweeper (v1) config
	const bool DEBUG = true;
	const bool AUTO_RESET = true;
	const int GRID_COLS = 4;
	const int GRID_ROWS = 4;
	const int TREASURE_COUNT = 1;
	const int MINE_COUNT = 3;
	const float DIG_DROP_THRESHOLD = 9.0f;
	const int DIG_CONFIRM_FRAMES = 3;
	const float DIG_COOLDOWN_SECONDS = 0.75f;
	const float DIG_MIN_TIME_BETWEEN_SAME_CELL = 2.0f;
	const float ROUND_START_GRACE = 1.5f;
	const int MAX_DIGS = 18;
	const bool SHOW_GRID = true;
	const float GRID_ALPHA = 0.25f;
	const float BASELINE_ALPHA = 0.02f;
	const float REVEAL_SECONDS_LOSE = 2.0f;
	const float REVEAL_SECONDS_WIN = 3.0f;
	const int CELL_SAMPLE_STEP = 4;
	const float FALLBACK_THRESHOLD_SCALE = 0.07f;
	const int CHEAT_TOGGLE_KEY = "y"[0];

	int clampi(int value, int low, int high)
	{
		return std::max(low, std::min(high, value));
	}
}

CTreasureSweeperController::CTreasureSweeperController()
	: active(false)
	, initialized(false)
	, hasCandidate(false)
	, candidateFrames(0)
	, lastDigTime(-1000.0)
	, roundState(ROUND_PLAYING)
	, roundStartTime(0.0)
	, stateEndTime(0.0)
	, digsUsed(0)
	, adaptiveDropThreshold(DIG_DROP_THRESHOLD)
	, revealMines(false)
	, cheatRevealAll(false)
{
}

CTreasureSweeperController::~CTreasureSweeperController() {}

void CTreasureSweeperController::setup(std::shared_ptr<KinectProjector> const& k)
{
	kinectProjector = k;
	baselineMeans.assign(GRID_COLS * GRID_ROWS, 0.0f);
	startRound();
}

void CTreasureSweeperController::setProjectorRes(ofVec2f& PR)
{
	projRes = PR;
}

void CTreasureSweeperController::setKinectRes(ofVec2f& KR)
{
	kinectRes = KR;
}

void CTreasureSweeperController::setKinectROI(ofRectangle& KROI)
{
	kinectROI = KROI;
}

void CTreasureSweeperController::toggleActive()
{
	active = !active;
	if (active)
	{
		startRound();
	}
}

bool CTreasureSweeperController::isActive() const
{
	return active;
}

void CTreasureSweeperController::resetRound()
{
	startRound();
}

bool CTreasureSweeperController::keyPressed(int key)
{
	if (!active)
	{
		return false;
	}
	if (key == 'r' || key == 'R')
	{
		resetRound();
		lastMessage = "Round reset.";
		return true;
	}
	if (key == CHEAT_TOGGLE_KEY || key == std::toupper(CHEAT_TOGGLE_KEY))
	{
		cheatRevealAll = !cheatRevealAll;
		lastMessage = cheatRevealAll ? "Cheat reveal ON" : "Cheat reveal OFF";
		return true;
	}
	return false;
}

void CTreasureSweeperController::startRound()
{
	hasCandidate = false;
	candidateFrames = 0;
	lastDigTime = -1000.0;
	lastDigTimeByCell.clear();
	revealMines = false;
	initialized = false;
	roundState = ROUND_PLAYING;
	roundStartTime = ofGetElapsedTimef();
	stateEndTime = 0.0;
	digsUsed = 0;
	dugCellIndices.clear();
	resultByCell.clear();
	lastMessage = "Dig to find the treasure!";

	std::vector<int> allCells(GRID_COLS * GRID_ROWS);
	for (int i = 0; i < static_cast<int>(allCells.size()); ++i)
	{
		allCells[i] = i;
	}
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::shuffle(allCells.begin(), allCells.end(), gen);
	const int treasureIdx = allCells[0];
	treasureCell = indexToCell(treasureIdx);
	mineCellIndices.clear();
	for (int i = TREASURE_COUNT; i < TREASURE_COUNT + MINE_COUNT && i < static_cast<int>(allCells.size()); ++i)
	{
		mineCellIndices.insert(allCells[i]);
	}

	if (DEBUG)
	{
		ofLogNotice("TreasureSweeper") << "Round start: treasure=(" << treasureCell.row << "," << treasureCell.col << ")";
		std::string mineText;
		for (std::set<int>::const_iterator it = mineCellIndices.begin(); it != mineCellIndices.end(); ++it)
		{
			CellCoord c = indexToCell(*it);
			mineText += " (" + ofToString(c.row) + "," + ofToString(c.col) + ")";
		}
		ofLogNotice("TreasureSweeper") << "Mines:" << mineText;
	}
}

bool CTreasureSweeperController::computeCellMeans(std::vector<float>& means, float& minElevation, float& maxElevation)
{
	if (!kinectProjector)
	{
		return false;
	}
	means.assign(GRID_ROWS * GRID_COLS, 0.0f);
	minElevation = std::numeric_limits<float>::max();
	maxElevation = -std::numeric_limits<float>::max();

	for (int row = 0; row < GRID_ROWS; ++row)
	{
		for (int col = 0; col < GRID_COLS; ++col)
		{
			CellCoord cell;
			cell.row = row;
			cell.col = col;
			ofRectangle b = cellKinectBounds(cell);
			float sum = 0.0f;
			int count = 0;
			for (int y = static_cast<int>(b.y); y < static_cast<int>(b.y + b.height); y += CELL_SAMPLE_STEP)
			{
				for (int x = static_cast<int>(b.x); x < static_cast<int>(b.x + b.width); x += CELL_SAMPLE_STEP)
				{
					const float e = kinectProjector->elevationAtKinectCoord(static_cast<float>(x), static_cast<float>(y));
					sum += e;
					count++;
					if (e < minElevation) minElevation = e;
					if (e > maxElevation) maxElevation = e;
				}
			}
			const int idx = cellToIndex(cell);
			means[idx] = (count > 0) ? (sum / static_cast<float>(count)) : 0.0f;
		}
	}

	if (minElevation >= maxElevation)
	{
		minElevation = 0.0f;
		maxElevation = 100.0f;
	}
	return true;
}

CTreasureSweeperController::CellCoord CTreasureSweeperController::indexToCell(int idx) const
{
	CellCoord cell;
	cell.row = idx / GRID_COLS;
	cell.col = idx % GRID_COLS;
	return cell;
}

int CTreasureSweeperController::cellToIndex(const CellCoord& cell) const
{
	return cell.row * GRID_COLS + cell.col;
}

CTreasureSweeperController::CellCoord CTreasureSweeperController::pickCellWithLargestDrop(const std::vector<float>& means, const std::vector<float>& baselines, float threshold, float& outDrop) const
{
	CellCoord best;
	best.row = -1;
	best.col = -1;
	outDrop = threshold;
	for (int idx = 0; idx < static_cast<int>(means.size()); ++idx)
	{
		const float drop = baselines[idx] - means[idx];
		if (drop > outDrop)
		{
			best = indexToCell(idx);
			outDrop = drop;
		}
	}
	return best;
}

void CTreasureSweeperController::resolveDig(const CellCoord& cell, double nowSec)
{
	const int idx = cellToIndex(cell);
	if (dugCellIndices.find(idx) != dugCellIndices.end())
	{
		lastMessage = "Already checked!";
		lastDigTime = nowSec;
		lastDigTimeByCell[idx] = nowSec;
		return;
	}

	dugCellIndices.insert(idx);
	digsUsed++;

	if (mineCellIndices.find(idx) != mineCellIndices.end())
	{
		resultByCell[idx] = "mine";
		roundState = ROUND_LOSE;
		revealMines = true;
		lastMessage = "BOOM! Mine!";
		stateEndTime = nowSec + REVEAL_SECONDS_LOSE;
	}
	else if (cell == treasureCell)
	{
		resultByCell[idx] = "treasure";
		roundState = ROUND_WIN;
		lastMessage = "TREASURE FOUND!";
		stateEndTime = nowSec + REVEAL_SECONDS_WIN;
	}
	else
	{
		resultByCell[idx] = "safe";
		const int dist = std::abs(cell.row - treasureCell.row) + std::abs(cell.col - treasureCell.col);
		const int danger = countMinesNearby(cell);
		lastMessage = warmthText(dist) + " | Danger: " + ofToString(danger);
		if (digsUsed >= MAX_DIGS)
		{
			roundState = ROUND_LOSE;
			revealMines = true;
			lastMessage = "Out of digs!";
			stateEndTime = nowSec + REVEAL_SECONDS_LOSE;
		}
	}

	lastDigTime = nowSec;
	lastDigTimeByCell[idx] = nowSec;
}

std::string CTreasureSweeperController::warmthText(int manhattanDistance) const
{
	if (manhattanDistance <= 1) return "BURNING!";
	if (manhattanDistance == 2) return "HOT";
	if (manhattanDistance == 3) return "WARM";
	if (manhattanDistance == 4) return "COOL";
	return "COLD";
}

int CTreasureSweeperController::countMinesNearby(const CellCoord& cell) const
{
	int count = 0;
	for (int dr = -1; dr <= 1; ++dr)
	{
		for (int dc = -1; dc <= 1; ++dc)
		{
			if (dr == 0 && dc == 0)
			{
				continue;
			}
			CellCoord n;
			n.row = clampi(cell.row + dr, 0, GRID_ROWS - 1);
			n.col = clampi(cell.col + dc, 0, GRID_COLS - 1);
			if (n.row == cell.row + dr && n.col == cell.col + dc)
			{
				if (mineCellIndices.find(cellToIndex(n)) != mineCellIndices.end())
				{
					count++;
				}
			}
		}
	}
	return count;
}

ofRectangle CTreasureSweeperController::cellKinectBounds(const CellCoord& cell) const
{
	const float cellW = kinectROI.width / static_cast<float>(GRID_COLS);
	const float cellH = kinectROI.height / static_cast<float>(GRID_ROWS);
	return ofRectangle(
		kinectROI.x + cell.col * cellW,
		kinectROI.y + cell.row * cellH,
		cellW,
		cellH);
}

ofVec2f CTreasureSweeperController::cellProjectorCenter(const CellCoord& cell) const
{
	ofRectangle b = cellKinectBounds(cell);
	const float x = b.getCenter().x;
	const float y = b.getCenter().y;
	return kinectProjector->kinectCoordToProjCoord(x, y);
}

void CTreasureSweeperController::update()
{
	if (!active || !kinectProjector || kinectProjector->GetApplicationState() != KinectProjector::APPLICATION_STATE_RUNNING)
	{
		return;
	}

	double nowSec = ofGetElapsedTimef();
	maybeAutoReset(nowSec);
	if (roundState != ROUND_PLAYING)
	{
		return;
	}

	std::vector<float> currentMeans;
	float minElevation = 0.0f;
	float maxElevation = 0.0f;
	if (!computeCellMeans(currentMeans, minElevation, maxElevation))
	{
		return;
	}

	const float observedRange = maxElevation - minElevation;
	const float fallbackThreshold = std::max(2.0f, observedRange * FALLBACK_THRESHOLD_SCALE);
	adaptiveDropThreshold = DIG_DROP_THRESHOLD;
	if (DIG_DROP_THRESHOLD > fallbackThreshold * 2.5f || DIG_DROP_THRESHOLD < fallbackThreshold * 0.4f)
	{
		adaptiveDropThreshold = fallbackThreshold;
	}

	if (!initialized)
	{
		baselineMeans = currentMeans;
		initialized = true;
		return;
	}

	float largestDrop = adaptiveDropThreshold;
	CellCoord maxDropCell = pickCellWithLargestDrop(currentMeans, baselineMeans, adaptiveDropThreshold, largestDrop);
	const bool hasMaxDrop = maxDropCell.row >= 0;

	if (nowSec - roundStartTime < ROUND_START_GRACE)
	{
		hasCandidate = false;
		candidateFrames = 0;
	}
	else if (hasMaxDrop)
	{
		if (hasCandidate && maxDropCell == candidateCell)
		{
			candidateFrames++;
		}
		else
		{
			candidateCell = maxDropCell;
			hasCandidate = true;
			candidateFrames = 1;
		}
	}
	else
	{
		hasCandidate = false;
		candidateFrames = 0;
	}

	for (int idx = 0; idx < static_cast<int>(baselineMeans.size()); ++idx)
	{
		if (hasMaxDrop && idx == cellToIndex(maxDropCell))
		{
			continue;
		}
		baselineMeans[idx] = (1.0f - BASELINE_ALPHA) * baselineMeans[idx] + BASELINE_ALPHA * currentMeans[idx];
	}

	if (hasCandidate && candidateFrames >= DIG_CONFIRM_FRAMES)
	{
		const int idx = cellToIndex(candidateCell);
		const bool globalCooldown = (nowSec - lastDigTime) < DIG_COOLDOWN_SECONDS;
		const bool sameCellCooldown = (lastDigTimeByCell.find(idx) != lastDigTimeByCell.end() && (nowSec - lastDigTimeByCell[idx]) < DIG_MIN_TIME_BETWEEN_SAME_CELL);
		if (!globalCooldown && !sameCellCooldown)
		{
			resolveDig(candidateCell, nowSec);
		}
		hasCandidate = false;
		candidateFrames = 0;
	}
}

void CTreasureSweeperController::maybeAutoReset(double nowSec)
{
	if (!AUTO_RESET)
	{
		return;
	}
	if (roundState != ROUND_PLAYING && nowSec >= stateEndTime)
	{
		startRound();
	}
}

void CTreasureSweeperController::drawCellMark(const CellCoord& cell, const std::string& result, bool emphasize) const
{
	ofVec2f center = cellProjectorCenter(cell);
	const float radius = emphasize ? 20.0f : 14.0f;

	if (result == "safe")
	{
		ofFill();
		ofSetColor(25, 120, 50, 220);
		ofDrawCircle(center, radius * 0.85f);
		ofNoFill();
		ofSetLineWidth(3);
		ofSetColor(80, 240, 120, 240);
		ofDrawCircle(center, radius);
	}
	else if (result == "mine")
	{
		ofSetLineWidth(4);
		ofSetColor(255, 70, 70, 240);
		ofDrawLine(center.x - radius, center.y - radius, center.x + radius, center.y + radius);
		ofDrawLine(center.x - radius, center.y + radius, center.x + radius, center.y - radius);
	}
	else if (result == "treasure")
	{
		ofFill();
		ofSetColor(255, 215, 40, 240);
		ofDrawCircle(center, radius * 0.8f);
		ofSetColor(255, 245, 160, 240);
		ofDrawCircle(center, radius * 0.35f);
	}
}

void CTreasureSweeperController::drawGrid() const
{
	if (!SHOW_GRID || !kinectProjector)
	{
		return;
	}
	ofSetLineWidth(4);
	ofSetColor(0, 0, 0, static_cast<int>(255.0f * GRID_ALPHA));
	for (int c = 0; c <= GRID_COLS; ++c)
	{
		float x = kinectROI.x + (kinectROI.width * c / static_cast<float>(GRID_COLS));
		ofVec2f p0 = kinectProjector->kinectCoordToProjCoord(x, kinectROI.y);
		ofVec2f p1 = kinectProjector->kinectCoordToProjCoord(x, kinectROI.y + kinectROI.height);
		ofDrawLine(p0, p1);
	}
	for (int r = 0; r <= GRID_ROWS; ++r)
	{
		float y = kinectROI.y + (kinectROI.height * r / static_cast<float>(GRID_ROWS));
		ofVec2f p0 = kinectProjector->kinectCoordToProjCoord(kinectROI.x, y);
		ofVec2f p1 = kinectProjector->kinectCoordToProjCoord(kinectROI.x + kinectROI.width, y);
		ofDrawLine(p0, p1);
	}
}

void CTreasureSweeperController::drawHud() const
{
	ofPushStyle();
	ofFill();
	ofSetColor(0, 0, 0, 140);
	ofDrawRectangle(20, 20, 560, 110);
	ofSetColor(255);
	ofDrawBitmapString("Treasure Sweeper", 35, 45);
	ofDrawBitmapString("Digs: " + ofToString(digsUsed) + " / " + ofToString(MAX_DIGS), 35, 70);
	ofDrawBitmapString("Last result: " + lastMessage, 35, 95);
	if (DEBUG)
	{
		ofDrawBitmapString("(DEBUG) Treasure at (" + ofToString(treasureCell.row) + "," + ofToString(treasureCell.col) + ")", 35, 120);
	}
	ofPopStyle();
}

void CTreasureSweeperController::drawProjectorWindow()
{
	if (!active || !kinectProjector || kinectProjector->GetApplicationState() != KinectProjector::APPLICATION_STATE_RUNNING)
	{
		return;
	}

	ofPushStyle();
	ofEnableAlphaBlending();
	drawGrid();

	for (std::map<int, std::string>::const_iterator it = resultByCell.begin(); it != resultByCell.end(); ++it)
	{
		drawCellMark(indexToCell(it->first), it->second, false);
	}

	if (roundState != ROUND_PLAYING || cheatRevealAll)
	{
		drawCellMark(treasureCell, "treasure", true);
		if (revealMines || cheatRevealAll)
		{
			for (std::set<int>::const_iterator it = mineCellIndices.begin(); it != mineCellIndices.end(); ++it)
			{
				drawCellMark(indexToCell(*it), "mine", true);
			}
		}
	}

	if (DEBUG)
	{
		ofVec2f c = cellProjectorCenter(treasureCell);
		ofFill();
		ofSetColor(180, 0, 255, 240);
		ofDrawCircle(c, 14.0f);
	}

	drawHud();

	if (roundState == ROUND_WIN)
	{
		ofSetColor(255, 220, 70);
		ofDrawBitmapString("YOU FOUND THE TREASURE!", projRes.x * 0.4f, projRes.y * 0.5f);
	}
	else if (roundState == ROUND_LOSE)
	{
		ofSetColor(255, 80, 80);
		ofDrawBitmapString("TRY AGAIN!", projRes.x * 0.48f, projRes.y * 0.5f);
	}
	else
	{
		ofSetColor(230);
		ofDrawBitmapString("Press R to reset round | Press Y to toggle cheat reveal", 35, 145);
	}
	ofPopStyle();
}

void CTreasureSweeperController::drawMainWindow(float x, float y, float width, float height)
{
	if (!active)
	{
		return;
	}
	ofPushStyle();
	ofSetColor(255);
	ofDrawBitmapString("Treasure Sweeper ACTIVE (T to toggle)", x + 20, y + 20);
	ofPopStyle();
}
