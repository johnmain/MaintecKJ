# MaintecKJ - Native Linux Karaoke Host System Specification

## 1. System Overview & Tech Stack
- **Target OS:** Linux (Arch/CachyOS, Fedora, Ubuntu)
- **Framework:** Qt 6.x (C++20 Backend + QML UI)
- **Audio Engine:** Qt Multimedia / PipeWire / PulseAudio with SoundTouch/RubberBand DSP pipeline
- **Video/Graphics:** Qt Quick Hardware-Accelerated Rendering (Vulkan/OpenGL)
- **Database:** SQLite3 / Qt SQL Module
* **Search Tools:** Use `rg` (ripgrep) and `fd` for searching files and code patterns. Do NOT use `find` or `grep`.
  * Search code: `rg "Pattern"`
  * Search filenames: `fd "FileName"`
* **Build Artifact Exclusion:** Always exclude `build/` and `.cache/` directories when searching or listing files.

---

## 2. Window Architecture
- **Dual-Window System:**
  - **Main Window:** Primary Host DJ Workspace (Controls, Singer List, Database Search, Queue, Secondary Preview Window).
  - **Secondary Window (Display/Canvas):** Displays CDG graphic output, MP4/MKV video streams, or static background image when idle.
  - **Window Toggle:** Dedicated toolbar/header button to launch, hide, or re-target the Secondary Window to a specific monitor output.

---

## 3. Singer Rotation & Queue Management
- **Singer Data Structure:**
  - `ID`, `Name`, `Status` (Active / Inactive), `OrderIndex`
  - `SongQueue` (List of ordered `SongItem` objects)
- **Song Data Structure:**
  - `ID`, `Title`, `Artist`, `FilePath`, `Duration`, `PlayedStatus` (Played / Unplayed)
- **Rotation Rules:**
  - Each Singer contains 1 or more songs in their personal queue.
  - Playing a song sets its state to `PlayedStatus = True`.
  - When all songs for a Singer are `PlayedStatus = True`, the Singer is automatically marked `Inactive`.
  - **Rotation Engine:** Auto-advance skips `Inactive` singers. Manual overrides allow re-ordering, adding, removing, or toggling active status manually.
  - **Queue Interaction:** The active Queue Panel displays the song queue of the currently selected singer or the active singer in rotation.

---

## 4. Song Database Manager
- **Directory Management:**
  - Add, remove, and rescan multiple root directories.
  - Support automatic recursive directory traversing.
- **Naming Pattern Parser:**
  - Token-based patterns:
    - `{Artist} - {Title}`
    - `{Title} - {Artist}`
    - `{Track} - {Artist} - {Title}`
  - Custom RegEx string parser for edge cases.
- **File Format Support:**
  - Paired formats: `.cdg` + `.mp3`
  - Compressed formats: `.zip` (containing matching `.cdg` + `.mp3`)
  - Video formats: `.mp4`, `.mkv`, `.avi`
- **Database Operations:**
  - Fast indexed search across Artist and Title.
  - Rescan engine: Adds new entries and removes stale/orphaned file records from the database.

---

## 5. Main UI Layout (OpenKJ-Inspired, Resizable Panels)

The main window uses nested `SplitView` components to allow full user control over panel proportions, with min/max size constraints to protect usability. Panel dimensions are persisted across sessions via `QSettings`.

- **Outer Layout (Horizontal SplitView):**
  - **Left Panel — Singer Rotation List:**
    - `SplitView.minimumWidth: 200`
    - `SplitView.preferredWidth: 280`
    - `SplitView.maximumWidth: 450`
    - Contains: Active singer list (Add, Remove, Reorder up/down, Inactive status toggle).
  
  - **Center Panel — Main Workspace (Vertical SplitView):**
    - `SplitView.fillWidth: true` (Grows dynamically with window resize)
    - **Top Sub-Panel — Song Database View:**
      - `SplitView.minimumHeight: 200`
      - `SplitView.preferredHeight: 350`
      - `SplitView.fillHeight: true`
      - Contains: Search bar, Filter options, Track list (`Artist`, `Title`, `Duration`).
    - **Bottom Sub-Panel — Current Singer Queue View:**
      - `SplitView.minimumHeight: 150`
      - `SplitView.preferredHeight: 250`
      - Bound directly to the active singer or selected singer from the Left Panel.
      - Double-clicking a track loads it into the playback deck and begins playback.

  - **Right Panel — Deck & Secondary Preview:**
    - `SplitView.minimumWidth: 250`
    - `SplitView.preferredWidth: 320`
    - `SplitView.maximumWidth: 480`
    - Contains: Secondary Window Mini-Preview, Now Playing details, Media controls, Pitch/Key shift ($\pm 6$ semitones), and Tempo adjustments.

---

## 6. Settings & Configuration

### Audio Tab
- **Audio Output Device:** Dropdown selector for ALSA / PipeWire / PulseAudio sinks.
- **Channel Downmixing:** Mono Downmix toggle switch.
- **DSP Engine:** Master volume and Pitch/Tempo algorithm settings.

### Video / Graphics Tab
- **Hardware Acceleration:** Toggle enable/disable GPU acceleration for QML and video decoding (VA-API / NVDEC).
- **CDG Upscaling:** Option to apply bilinear/nearest-neighbor scaling filters to low-res CDG graphics.
- **Secondary Background:** Custom image selector for display when no karaoke track is playing.
- **Display Target Routing:** Monitor selection dropdown for Secondary Window output.

---

## 7. Code Editing Rules
* **Single-File Changes:** Edit or create ONLY ONE file per turn using native file-writing tools.
* **No Multi-File Bash Scripts:** NEVER execute monolithic `cat << 'EOF'` bash scripts to write multiple files simultaneously.
* **Relative Imports:** Standardize QML component imports using relative directory imports (e.g., `import "components"`) to maintain QML engine resolution stability.

---

## 8. Build & Verification Standard
* Mandatory Build Check: After every code change, immediately verify compilation by running:
  cmake -B build -S . && cmake --build build
* Verify Success: Check for [100%] Built target mainteckj-app before declaring a task complete.

---

## 9. Diagnostics & Failure Recovery
* Targeted Fixes: If the build fails:
  1. Inspect the exact compiler or QML engine error message.
  2. Modify ONLY the file causing the build failure.
  3. Do NOT rewrite unrelated QML components or reconfigure CMake URIs unless explicitly required.
* Two-Failure Stop Policy: If a build fails twice consecutively on the same issue, STOP immediately. Report the exact compiler log and request human guidance instead of attempting further automated fixes.
