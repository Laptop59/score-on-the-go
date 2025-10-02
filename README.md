# Score on the Go
Score on the Go is currently a ballfile editor for creating the game's charts.

https://github.com/user-attachments/assets/277f0623-df65-4578-854b-e79091d91d38

This editor allows interactive editing of
- a Ballfile (a 'chart', one for each difficulty - contains its own BPM changes and Paddle Width & Speed changes)
- a Background Commands file (a format for playing different background animations outside the editor.)

To learn more about the keybinds in this program, press `F1` once to open a list of actions with the keyboard.

Be aware that bugs and glitches may exist in this application but the editor is in a mostly-usable state.

# Compiling the project
This editor adds the required SDL3 modules (`SDL`, `SDL_ttf`, `SDL_image`, `SDL_mixer`) as *submodules* of this repository.
Run the following once to compile the program with its source code:
```
git clone https://github.com/Laptop59/score-on-the-go --depth=1 --recurse-submodules
cd score-on-the-go

## From here, choose your appropriate script:
# For Windows
build_windows.bat
# For native platforms
./build_generic.sh
# For the web (Unix)
./build_web_unix.sh
# For the web (Windows)
build_web_windows.bat
```
- For the web, make sure to install the `emsdk` and make it accessiblt to the terminal: https://emscripten.org/docs/getting_started/downloads.html
- For native platforms and for Windows, you can replace `build` with `test` to also open the built executable. This is preferable for testing some new code.

> **NOTE:** I have only tested the `generic` and `unix` scripts (so take the rest with a grain of salt!)

Run `update_sdl.sh` to update the SDL modules in the project.

## Using with Scratch project
The following tutorials cover the basics.
To use the editor's full potential, open help with `F1`.

### Charts/Ballfiles (.txt)
Making charts tediously with editing a text file was not productive - so this editor can be used to help speed up the progress of making 'Ballfiles':
1.  Open the Editor.
2.  Load your music as a reference by using the key `M`.
3.  Place balls in different x-positions and playtest your creation with `F3`. When you're done, use `F2` for the entire chart.
    Use the `P` key to switch between different 'placing modes' or 'ball types' in the game.
    Optionally to create holds and pits, where you would create your balls, instead hold your left mouse button and move down using your mouse scroll or arrow keys.
    Then you can move the linear points to your desire. While moving a point press `CTRL` to clone it. This is how holds can have an irregular track.
5.  Export your chart using the `S` key.
6.  Go to the Scratch Project and paste in the chart into a list entry of the list `& SONGS`; the newlines are converted to spaces and the game can read the ballfile.
7.  To import your ballfiles use the `L` key. The balls and entries must be separated per line - so keep a backup of your ballfiles before loading them in!

### BG Commands (.bgc)
1.  Open the Editor.
2.  Load your music and a chart as a reference. Beware that this will wipe your currently loaded commands.
3.  At certain places press the `C` to insert commands.
4.  When you are done press `O` to save your commands into a file (this is not a ballfile!).
5.  Go to the Scratch Project and paste in the chart into a list entry of the list `& SONG BG CHANGES`; the newlines are converted to spaces and the game can read the commands.
6.  These files can then be imported with the `I` key - your currently-loaded commands will then be overridden and lost if not saved!
    The entries must be separated per line - so keep a backup of your command files before loading them in.

## Credits
The SDL3 library acting as the framework for this project: [SDL](https://github.com/libsdl-org/SDL) and its other libraries.

The sample SDL3 project: [sdl3-sample](https://github.com/Ravbug/sdl3-sample).

The **Noto Sans** font: [Noto Dashboard](https://notofonts.github.io/).
