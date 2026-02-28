# Treasure Sweeper discovery notes

- **Main update loop:** `ofApp::update()` in `src/ofApp.cpp` updates `kinectProjector`, `sandSurfaceRenderer`, and game controllers each frame.
- **Projector/main rendering pipeline:** `ofApp::drawProjWindow()` and `ofApp::draw()` in `src/ofApp.cpp` draw the sand surface first, then game overlays.
- **Input handling:** `ofApp::keyPressed()` in `src/ofApp.cpp` is the central keybinding handler.
- **Mode/game structure:** existing game modules are independent controllers (`CMapGameController`, `CBoidGameController`) called from `ofApp` rather than a shared base class.
- **Height units:** elevation values are produced via `KinectProjector::elevationAtKinectCoord()` in `src/KinectProjector/KinectProjector.cpp`; this computes elevation relative to the calibrated base plane.
- **Coordinate mapping:** Kinect-to-projector mapping is done with `KinectProjector::kinectCoordToProjCoord(...)` in `src/KinectProjector/KinectProjector.h/.cpp`.
- **Insertion points for new mode:** Added `CTreasureSweeperController` setup/update/draw/key integration in `src/ofApp.h` and `src/ofApp.cpp`, with implementation in `src/Games/TreasureSweeperController.h/.cpp`.
