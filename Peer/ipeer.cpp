#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <netdb.h>
#include <thread>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <unordered_map>
#define BUF_SIZE 1025
#define NO_CLIENTS 5

std::unordered_map<std::string, std::string>fileMap;
std::mutex service_mutex;

void registerFiles(char* currServer, char* directory){
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (directory)) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            std::string fileName(ent->d_name);
            std::string ip(currServer);
            std::string temp(directory);
            temp += fileName;
        fileMap[temp] = ip;
        std::cout << "[INFO] Registering Files " << ": " << fileName << std::endl;
        }
        closedir (dir);
    }
}

void sendFile(int sockfd, char* file){
    char buffer[BUF_SIZE] = {0};
    int fd = open (file, O_RDONLY);
    int end = lseek(fd, 0, SEEK_END);
    int len = 0;
    int n = 0;
    lseek(fd, 0, SEEK_SET);
    do
    {
        n = read (fd, buffer, sizeof(buffer));
        send (sockfd, buffer, n, 0);
        len += n;
    } while (len < end);
    std::cout << "[INFO]" << len << "/" << end << std::endl;
    close(fd);
}

void receiveFile(int sockfd, char* file)
{
    int len = 0, cnt = 0;
    char buffer[BUF_SIZE] = {0};
    int n = send(sockfd, file, strlen(file), 0);
    int fd = open (file, O_CREAT | O_WRONLY, S_IRUSR|S_IWUSR);
    do {
        n = recv (sockfd, buffer, sizeof(buffer) - 1, 0);
        len += n;
        if (cnt == 1024){
            std::cout << "=";
            cnt = 0;
        }
        fflush(stdout);
        write (fd, buffer, n);
        cnt++;
    } while (n > 0);
    std::cout << std::endl;
    close (fd);
}

void connectHost(int sockfd, char* hostName)
{
    struct sockaddr_in serverIP;
    struct hostent *h = gethostbyname(hostName);
    memset(&serverIP, 0, sizeof(serverIP));
    serverIP.sin_family = AF_INET;
    serverIP.sin_addr.s_addr = ((struct in_addr*)(h->h_addr))->s_addr;
    serverIP.sin_port = htons(8080);
    connect (sockfd, (struct sockaddr *)&serverIP, sizeof(serverIP));
}

void bindSocket(int sockfd)
{
    struct sockaddr_in serverIP;
    memset(&serverIP, 0, sizeof(serverIP));
    serverIP.sin_family = AF_INET;
    serverIP.sin_addr.s_addr = INADDR_ANY;
    serverIP.sin_port = htons(8080);
    bind (sockfd, (struct sockaddr*)&serverIP, sizeof(serverIP));
    listen(sockfd, NO_CLIENTS);
}

char* getClientIP(char* file, char* peerIPList, int ttl){
    char* clientIP = (char* )malloc(sizeof(char)*20);
    char msg[20] = {0};
    char pIP[20] = {0};
    std::ifstream pList(peerIPList);
    std::string peerIP;
    do {
    std::getline(pList, peerIP);
    std::string message = "MSG_SEARCH,";
    message += std::to_string(ttl);
    strcpy(msg, message.c_str());
    strcpy(pIP, peerIP.c_str());

        int clientfd = socket(AF_INET, SOCK_STREAM, 0);
        connectHost(clientfd, pIP);
    send(clientfd, msg, 20, 0);
        send(clientfd, file, 20, 0);
        recv(clientfd, clientIP, 20, 0);

        std::cout << "[INFO] Searching for file in Client : " << std::string(clientIP) << std::endl;
        close(clientfd);
    } while(std::string(clientIP) == "000.000.00.000");
    return clientIP;
}

void serviceConn(int sockfd, char* peerIPList)
{
    struct sockaddr_in clientIP;
    socklen_t b = sizeof(clientIP);

    char buffer[50] = {0};
    char fileName[20] = {0};

    while (1)
    {
    std::unique_lock<std::mutex> ulock(service_mutex);
        int connfd = accept(sockfd, (struct sockaddr*)&clientIP, &b);
    recv(connfd, buffer, sizeof(buffer), 0);
    std::stringstream ss(buffer);
    std::string message;
    std::string time;
    std::getline(ss, message, ',');
    std::getline(ss, time, ',');

    if(message == "MSG_SEARCH"){
        int ttl = stoi(time);
        ulock.unlock();
        recv(connfd, buffer, sizeof(buffer), 0);
        ulock.lock();
        std::string file(buffer);
        std::string fileIP;
        char fip[20] = {0};
        if(fileMap.count(file)){
            fileIP = fileMap[file];
        }
        else {
        if(ttl > 0){
            char cfile[20];
            file.copy(cfile, file.size()+1);
            fileIP = getClientIP(cfile, peerIPList, ttl - 1);
        }
        else {
            std::cout << "[INFO] Time to live expired" << std::endl;
                fileIP = "000.000.00.000";
        }
        }
        std::cout << "[INFO] Time to live : " << ttl << std::endl;
        fileIP.copy(fip, fileIP.size() + 1);
        send(connfd, fip, strlen(fip), 0);
    }
    else if(message == "MSG_GET"){
            int filelen = read(connfd, fileName, sizeof(fileName));
            fileName[filelen] = 0;
            std::cout << "[INFO] " << fileName << ":" << "sent: " << " from " << std::this_thread::get_id() << std::endl;
            sendFile(connfd, fileName);
    }
        close(connfd);
    ulock.unlock();
    }
}

void downloadfile(char* file, char* peerList){
    int ttl = 5;
    char* clientIP = getClientIP(file, peerList, ttl);
    std::cout << "[INFO] Downloading file from : " << std::string(clientIP) << std::endl;
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    connectHost(clientfd, clientIP);
    send(clientfd, "MSG_GET", 20, 0);
    receiveFile(clientfd, file);
    close(clientfd);
    free(clientIP);
}

int main(int argc, char* argv[]){
    /*
    argv[1] = Peer IP Address
    argv[2] = File Location for registration
    argv[3] = Peers List
    argv[4] = Batch File location for download
    */

    int serverfd;
    char file[20] = {0};
    std::string fileName;
    std::thread connThreads[NO_CLIENTS];

    if(argc < 4){
        std::cout << "[ERROR] Invalid Arguments" << std::endl;
        return -1;
    }

    registerFiles(argv[1], argv[2]);
    for(auto file : fileMap){
    std::cout << file.first << "\t" << file.second << std::endl;
    }

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    bindSocket(serverfd);

    for(int i = 0; i < NO_CLIENTS; i++)        connThreads[i] = std::thread(serviceConn, serverfd, argv[3]);

    std::cout << "[INFO] Waiting for all servers to be up for batch run. Press any key to continue." << std::endl;
    std::cin.get();

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    std::ifstream listFile(argv[4]);
    while(std::getline(listFile, fileName)){
    std::cout << "[INFO] Downloading File:" << fileName << std::endl;
        strcpy(file, fileName.c_str());
        downloadfile(file, argv[3]);
    }

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "[INFO] Transfer Time : " << std::chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "[s]" << std::endl;

    std::cout << "[INFO] Waiting for all jobs to complete. Press any key to continue" << std::endl;
    std::cin.get();

    for(int i = 0; i < NO_CLIENTS; i++)        connThreads[i].join();

    close(serverfd);
}

