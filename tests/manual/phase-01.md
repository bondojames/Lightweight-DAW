# Phase 01 hands-on checks

These need ears. Use the program from the extracted ZIP, and turn your volume down before the first test.

| # | Do this | Pass if |
|---|---|---|
| 1 | Tick Metronome, then press Play | You hear steady clicks, twice a second at 120 BPM, with a higher, louder click every fourth beat. The position counter moves |
| 2 | Press Stop, then Play, several times; try the Space bar too | Clicks stop and start with no pops, and Space does the same as the buttons |
| 3 | Press Rewind, then Play | The counter returns to 1.1, and the first click is the accented one |
| 4 | While playing, change BPM to 60, then 180 | The click speed changes right away, with no glitch |
| 5 | Tick Test tone, with the transport stopped | A steady, quiet 440 Hz note (the A above middle C), with no click when it starts or stops |
| 6 | Audio > Audio settings: switch to each output device while playing | Audio moves to that device, and the transport stops safely. Press Play to continue. Diagnostics shows the device |
| 7 | Try buffer sizes 128, 256, 512, 1024, and sample rates 44100 and 48000 | Sound stays clean. Preferences shows what the driver actually gave |
| 8 | With the metronome playing, use File > Test Open dialog, leave it open a few seconds, then cancel | Clicks never stumble, and Diagnostics dropouts stay at 0. The UI itself may pause while the dialog is open |
| 9 | With the metronome playing, drag panels around, detach one, switch workspaces, resize the window | Clicks never stumble |
| 10 | While playing through a USB or Bluetooth device, unplug it or turn it off | The app says the device was lost and does not crash. Reconnecting brings audio back, or Windows moves it to another device |
| 11 | Audio > Run offline metronome test, and save the WAV anywhere | Diagnostics reports PASS. The WAV plays as a clean click track in another player |
| 12 | Close and reopen the app | Your chosen device, sample rate and buffer size are remembered |
