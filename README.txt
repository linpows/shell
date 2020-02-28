

Student Information
-------------------
Daniel Imondo (djimondo) 
Swopnil Joshi (linpows)

How to execute the shell
------------------------
type "esh"
	-p "/plugin-directory" to run plugin

Important Notes
---------------
"Thank you to all of the TA's that helped out."


Description of Base Functionality
---------------------------------
"These funtions were implemented by checking the command_structs argv to see
if it was among the builtins, then the functionality was implemented in a seperate
builtins.c file. When a change of status was required, the pipeline object was 
updated and the corresponding signal was sent. ^C/^Z were recieved by wait for job
and corresponding updates were made in jobs list/ pipeline objects"

Description of Extended Functionality
-------------------------------------
"a 2d array of ints was used, initialized to a size proportional to the number of
commands in the pipeline, these were then dup2'ed when required and immediatly 
closed in the child. In the case that iored was indicated for input or output the 
indicated file was open()'ed with flags corresponding to user indicated  and 
desired behavior
for exclusive access, any pipeline has each process set to the same group, and this
group was then given terminal access until exit, terminal was then returned to the
saved state of the shell"

List of Plugins Implemented
---------------------------
(Written by Your Team)
"we wrote no plugins"

(Written by Others)
1)
zodiac
robleshs+jamespur
2)
roll
robleshs+jamespur
3)
history
robleshs+jamespur
4)
ceasar
zihao225+zboren98
5)
fib
zihao225+zboren98
6)
timer
zihao225+zboren98
7)
toBinary
mnj98+enorth
8)
sort 
mnj98+enorth


"we couldnt get the python test driver working but were able to verify
 that they work"

