# WheresMyMouse
## A virtual mouse for users without a mouse, coded entirely without a mouse.

### Usage:
To use this use the NUMPAD 8, 4, 6, and 2. Optionally if you want a more closer layout you can use 5 to go down instead of 2.<br>
<br>
NUMPAD 0 makes the mouse move slower for more precise spots.<br>
<br>
NUMPAD 7 is left click, and NUMPAD 9 is right click.<br>
<br>
This does not support scrolling.<br>
<br>
To exit and close the virtual mouse at any time press the DEL key.<br>

### The keys aren't working!
Ensure the keys are enabled by making sure NUMLOCK is on.

### I can't run the program!
Go to the Group Policy Editor > Computer Configuration > Windows Settings > Security Settings > Local Policies > Security Options and set User Account Control: Only elevate executables that are signed and validated to Disabled.<br>
<br>
The reason this happens is because this program is not a signed binary due to me not having a certificate to sign it with.<br>
<br>
If this doesn't work make sure your anti-virus isn't blocking this program. It may be classified as a keylogger.<br>

### Why does this need administrator?
This needs admin due to how Windows' trust system works. If you try to open task manager with the virtual mouse it will not inject into task manager and instead it will bug out due to the mouse lacking permissions.<br>
<br>
By running this as administrator you give it the privileges to allow it to run inside administrator programs and allow it to give itself a high priority so that the mouse movement is prioritized first.<br>
