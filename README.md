## Simple Http webserver
A simple http server written in C/C++. Written entirely with system calls/standard library features. A filepath points to where all the valid files for the webserver are in the system, and a simple string parser determines what file to send.
Feel free to use any of this code with some sort of credit to me.
## Compile and Run
You can compile with 
```BASH
make all
```
provided you have g++ installed. This will yield a server executable. To run, create a folder that holds at least an index.html file and start it as the server directory with
```BASH
./server /path/to/server/directory
```
