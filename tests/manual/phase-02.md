# Phase 02 hands-on checks

**There's no save yet.** Projects arrive in Phase 03, so anything you build here is lost when you close the app.

Use the program from the extracted ZIP, and turn your volume down first. Please use your own audio files, including at least one WAV and one MP3.

| # | Do this | Pass if |
|---|---|---|
| 1 | Drag a WAV from Explorer onto the Timeline panel | A track appears with the clip and its waveform, where you dropped it. The UI never freezes, even with a long file |
| 2 | Import an MP3, a FLAC and an M4A, by dropping or with File > Import audio | Each one plays and sounds right. **MP3 matters most: no automated test could cover it** |
| 3 | Import a file with a different sample rate from your device (for example 44.1 kHz on a 48 kHz device) | It plays at the right pitch and speed |
| 4 | Press Play or Space | The file plays, and the playhead moves in step with it |
| 5 | Click in the ruler while stopped, then while playing | The playhead jumps there, and playback continues from that point, with no long gap |
| 6 | Drag a clip sideways, then onto another track | It moves, snapping to beats. Hold Alt for no snapping |
| 7 | Drag a clip's left or right edge | It trims, and the audio that remains stays in the same place. It can't go past the ends of the file |
| 8 | Select a clip, put the playhead inside it, press S | It splits in two. Playing across the join sounds seamless |
| 9 | Press Ctrl+D with a clip selected | A copy appears right after it |
| 10 | Press Delete with a clip selected | The clip goes. The file on disk is untouched |
| 11 | Ctrl+Z and Ctrl+Y after several of the above, or the Edit menu | Each edit undoes and redoes in order. One drag is one undo step |
| 12 | Shift-drag in the ruler to mark a region, tick Loop, press Play | It loops that region in time. A faint click at the loop point is a known limit of this phase, but any stutter isn't |
| 13 | While playing, use a track's gain and pan sliders, M and S | Changes are smooth, with no zipper noise or pops. Right-click resets a slider |
| 14 | Put two clips on two tracks; mute one, solo one | Mute and solo behave as expected |
| 15 | Ctrl+wheel to zoom; wheel to scroll; Shift+wheel to scroll tracks | Zoom stays centred on the mouse, and it all stays responsive, including zoomed far out on a long file |
| 16 | Play a long file for a few minutes while zooming, scrolling and dragging panels | No stutter. Diagnostics shows 0 dropouts and 0 frames missed, and memory stays roughly level |
| 17 | While playing, switch the output device in Audio settings | Audio moves, and the transport stops. Press Play: the clips still play, at the right pitch, even at a new sample rate (it may take a moment to re-prepare) |
| 18 | Try to import a file that isn't audio | A clear "Import failed" appears in the top bar, with the reason on hover. Nothing else changes |
