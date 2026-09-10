# MaintecKJ Development Checklist

## Phase 1: Core Layout & Dual Windows
- [x] Create project `agent.md` specification
- [x] Create `CMakeLists.txt` with Qt 6 dependencies
- [x] Implement basic `main.cpp` entrypoint
- [x] Build `Main.qml` with 3-panel `SplitView` layout
- [x] Build `SecondaryWindow.qml` with display toggle button

## Phase 2: C++ Backend Models (Next Steps)
- [x] Create `SingerModel` (QAbstractListModel) for singer list & status
- [x] Create `SongQueueModel` for singer-specific track ordering
- [ ] Implement SQLite database manager (`DatabaseManager.cpp`)
- [ ] Write directory scanner and file pattern parser (`{Artist} - {Title}`)

## Phase 3: Media & DSP Pipeline
- [ ] Set up Qt Multimedia `QMediaPlayer` audio pipeline
- [ ] Integrate pitch change ($\pm 6$ semitones) and tempo controls
- [ ] Implement CDG file reader and QuickItem renderer

## Phase 4: Host Controls & Settings
- [ ] Connect queue double-click action to player load/play
- [ ] Add audio device selector dropdown to Settings
- [ ] Store panel split sizes and settings in `QSettings`
