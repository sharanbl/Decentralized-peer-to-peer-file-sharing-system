The program ipeer.cpp contains gnutella client program.

Compilation instructions:
g++ ipeer.cpp -lpthread

Execution instructions:
It requires four arguments.
Argument 1 is ip address of the client system.
Argument 2 is the path of the file location in the client.
Argument 3 is the file contatining the list of its neighbours.
Argument 4 is the file containing the list of files to download in batch mode. 

./a.out [Peer IP Address] [file path] [neightbour list file] [download file list]

eg: ./a.out 192.168.56.1 files/ neighbours.txt downloadlist.txt