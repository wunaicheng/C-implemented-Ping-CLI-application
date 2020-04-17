#ifndef PING_H
#define PING_H

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string>
#include <string.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>

#define PACK_SIZE 32                //Smallest ICMP pack size

class Ping {
private:
    std::string input_domain;       //Store the input domain/IP
    std::string backup_ip;          //Backup

    int sock_fd;
    int TTL;                        //TTL (optional)
    int max_wait_time;

    int send_pack_num;
    int recv_pack_num;
    int lost_pack_num;

    struct sockaddr_in send_addr;   //For pack sending
    struct sockaddr_in recv_addr;   //For pack receiving

    char send_pack[PACK_SIZE];      //Keep sent pack
    char recv_pack[PACK_SIZE + 20];      //Keep received pack

    struct timeval first_send_time; //First time time stamp
    struct timeval recv_time;       //Receiving time stamp

    double min_time;
    double max_time;
    double sum_time;


    int GeneratePacket();
    int ResolvePacket(int pack_szie);

    unsigned short CalculateCksum(unsigned short * send_pack, int pack_size);

public:
    Ping(const char * ip, int k);
    ~Ping();

    void CreateSocket();

    void SendPacket();
    void RecvPacket();

    void statistic();
};


#endif