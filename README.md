# task-monitor
A system that closes tasks without user interaction for a set time.

The system only works on windows OS.

# ABOUT
The user can monitor a program/task that is running on computer and define a timeout to close if there is no user interaction
when the time of no interaction is reached the system must close the program.

The timeout must be entered in seconds.

# How does the algorithm work?
The algorithm do a global listen on the keyboard and mouse so every time that the user do a input in keyboard or mouse the timeout of the program
that is foreground is reset to zero. If the user does not input on keyboard or mouse over a program registered then it´ll be closed when timeout is reached.

The programs that be monitored stay stored in a file named "programs.txt" and must obey a pattern of sintax to be a valid file to the algortihm.

Pattern: PATH=TIMEOUT(SECONDS);
Example: C:\Program Files\Exemple\Exemple.exe=60;

The exemple above define that a hypothetical program "Exemple.exe" must be closed after 60 seconds without any user input of keyboard or mouse.
The user does not need to worry with this file beacause it will be filled automatically when uses the CLI and register a program.
