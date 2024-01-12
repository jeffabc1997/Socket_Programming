# TCP and HTTP

The program can display all the hyperlinks on a given web page. The primary objective is
to familiarize the HTTP protocol and socket programming.

# Input 
The URL of a webpage without “http://”. 
Take http://can.cs.nthu.edu.tw/index.php as an example; the input is can.cs.nthu.edu.tw/index.php.
Note that both can.cs.nthu.edu.tw/ and can.cs.nthu.edu.tw are valid URLs. 
The first step to do is parsing the input URL into hostname and path. 

# Output
Print all hyperlinks and the total number of hyperlinks in the given webpage. (not including hyperlinks that is in comment)
Note that only the hyperlinks in the format of <a … href="abc" …></a> should be printed out and counted.
Do not print/count other formats such as <link … href="abc" …>.