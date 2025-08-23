# Score on the Go
Score on the Go is currently a ballfile editor for creating the game's charts.

This editor allows interactive editing of
- a Ballfile (a 'chart', one for each difficulty - contains its own BPM changes and Paddle Width & Speed changes)
- a Background Commands file (a format for playing different background animations outside the editor.)

To learn more about the keybinds in this program, press `F1` to open a list of actions with the keyboard.
You must be able to compile this app to use it with the SDL libraries to start using the app.
Be aware that bugs and glitches may exist in this application but the editor is in a mostly-usable state.

## Dependencies
These dependencies are required to compile the program:
- SDL3
- SDL3_ttf
- SDL3_image
- SDL3_mixer

Yes, it doesn't use SDL2. It uses *SDL3*.

## Using with Scratch project
The following tutorials cover the basics. To use the editor's full potential, open help with `F1`.

### Charts/Ballfiles (.txt)
Making charts tediously with editing a text file was not productive - so this editor can be used to help speed up the progress of making 'Ballfiles':
1.  Open the Editor
2.  Load your music as a reference by using the key `M`.
3.  Place balls in different x-positions and playtest your creation with `F3`. When you're done, use `F2` for the entire chart.
    Use the `P` key to switch between different 'placing modes' or 'ball types' in the game.
    Optionally to create holds and pits, where you would create your balls, instead hold your left mouse button and move down using your mouse scroll or arrow keys.
    Then you can move the linear points to your desire. While moving a point press `CTRL` to clone it. This is how holds can have an irregular track.
5.  Export your chart using the `S` key.
6.  Go to the Scratch Project and paste in the chart into a list entry of the list `& SONGS`; the newlines are converted to spaces and the game can read the ballfile.
7.  To import your ballfiles use the `L` key. The balls and entries must be separated per line - so keep a backup of your ballfiles before loading them in!

### BG Commands (.bgc)
1.  Open the Editor
2.  Load your music and a chart as a reference. Beware that this will wipe your currently loaded commands.
3.  At certain places press the `C` to insert commands.
4.  When you are done press `O` to save your commands into a file (this is not a ballfile!).
5.  These files can then be imported with the `I` key - your currently-loaded commands will then be overridden and lost if not saved! The entries must be separated per line - so keep a backup of your ballfiles before loading them in!