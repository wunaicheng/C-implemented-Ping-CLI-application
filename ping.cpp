#include "ping.h"

Ping::Ping(const char * ip, int k){
    this->input_domain = ip;

    this->max_wait_time = max_wait_time < 3 ? max_wait_time : 3;

    //reporting data
    this->send_pack_num = 0;
    this->recv_pack_num = 0;
    this->lost_pack_num = 0;
    this->TTL = k;
    this->min_time = 0;
    this->max_time = 0;
    this->sum_time = 0;
    this->max_wait_time = 1;
}

Ping::~Ping() {
    if(close(sock_fd) == -1) {
        fprintf(stderr, "Close socket error:%s \n\a", strerror(errno));
        exit(1);
    }
}

void Ping::CreateSocket(){
    struct protoent * protocol;             //For protocol
    unsigned long in_addr;                  //Save binary ip
    struct hostent host_info, * host_pointer; //Save IP information for gethostbyname_r
    char buff[2048];                         //Buffer
    int errnop = 0;                         //gethostbyname_r's error code space

    //Get protocol
    if((protocol = getprotobyname("icmp")) == NULL){
        fprintf(stderr, "Get protocol error:%s \n\a", strerror(errno));
        exit(1);
    }

    //Need root, please apply sudo before run this program.
    if((sock_fd = socket(AF_INET, SOCK_RAW, protocol->p_proto)) == -1){
        fprintf(stderr, "Greate RAW socket error:%s \n\a", strerror(errno));
        exit(1);
    }
    if(TTL > 0){
    setsockopt(sock_fd, IPPROTO_IP, IP_TTL, &TTL, sizeof(TTL));
    }

    setuid(getuid());

    //For now, only consider IPv4 case.
    send_addr.sin_family = AF_INET;

    //Judge the input
    if((in_addr = inet_addr(input_domain.c_str())) == INADDR_NONE){
        //Potentially domain name
        if(gethostbyname_r(input_domain.c_str(), &host_info, buff, sizeof(buff), &host_pointer, &errnop)){
            //Wrong Domain
            fprintf(stderr, "Get host by name error:%s \n\a", strerror(errno));
            exit(1);
        } else{
            //domain name
            this->send_addr.sin_addr = *((struct in_addr *)host_pointer->h_addr);
        }
    } else{
        // It's an IP address
        this->send_addr.sin_addr.s_addr = in_addr;
    }

    //Backpack ip
    this->backup_ip = inet_ntoa(send_addr.sin_addr);

    printf("PING %s (%s) %d(%d) bytes of data.\n", input_domain.c_str(),
            backup_ip.c_str(), PACK_SIZE - 8, PACK_SIZE + 20);

    gettimeofday(&first_send_time, NULL);
}
//Calculate checksum.
unsigned short Ping::CalculateCksum(unsigned short * send_pack, int pack_size){
    int check_sum = 0;
    int nleft = pack_size;
    unsigned short * p = send_pack;
    unsigned short temp;            //In case the length is odd.

    while(nleft > 1){
        check_sum += *p++;
        nleft -= 2;
    }


    if(nleft == 1){
        //Since char has 8 bits
        *(unsigned char *)&temp = *(unsigned char *)p;
        check_sum += temp;
    }

    check_sum = (check_sum >> 16) + (check_sum & 0xffff);
    check_sum += (check_sum >> 16);                         //Prevent overflow
    temp = ~check_sum;              //Final checksum

    return temp;
}

int Ping::GeneratePacket()
{
    int pack_size;
    struct icmp * icmp_pointer;
    struct timeval * time_pointer;


    icmp_pointer = (struct icmp *)send_pack;

    //echo type
    icmp_pointer->icmp_type = ICMP_ECHO;
    icmp_pointer->icmp_code = 0;
    icmp_pointer->icmp_cksum = 0;           //Zero the ckech sum
    icmp_pointer->icmp_seq = send_pack_num + 1; //Use this send_pack_num as ICMP's seq number
    icmp_pointer->icmp_id = getpid();       //Mark the IMCP by the Process identifier

    pack_size = PACK_SIZE;


    time_pointer = (struct timeval *)icmp_pointer->icmp_data;

    gettimeofday(time_pointer, NULL);

    icmp_pointer->icmp_cksum = CalculateCksum((unsigned short *)send_pack, pack_size);

    return pack_size;
}

void Ping::SendPacket() {
    int pack_size = GeneratePacket();

    if((sendto(sock_fd, send_pack, pack_size, 0, (const struct sockaddr *)&send_addr, sizeof(send_addr))) < 0){
        fprintf(stderr, "Sendto error:%s \n\a", strerror(errno));
        exit(1);
    }

    this->send_pack_num++;
}

//read the pack，check ICMP，get time stamp
int Ping::ResolvePacket(int pack_size) {
    int icmp_len, ip_header_len;
    struct icmp * icmp_pointer;
    struct ip * ip_pointer = (struct ip *)recv_pack;
    double rtt;
    struct timeval * time_send;

    ip_header_len = ip_pointer->ip_hl << 2;                     //*4
    icmp_pointer = (struct icmp *)(recv_pack + ip_header_len);  //Ignore the head
    icmp_len = pack_size - ip_header_len;                       //Total length

    //ICMP size < head of pack
    if(icmp_len < 8) {
        printf("received ICMP pack lenth:%d(%d) is error!\n", pack_size, icmp_len);
        lost_pack_num++;
        return -1;
    }
    if((icmp_pointer->icmp_type == ICMP_ECHOREPLY) &&
        (backup_ip == inet_ntoa(recv_addr.sin_addr)) &&
        (icmp_pointer->icmp_id == getpid())){

        time_send = (struct timeval *)icmp_pointer->icmp_data;

        if((recv_time.tv_usec -= time_send->tv_usec) < 0) {
            --recv_time.tv_sec;
            recv_time.tv_usec += 10000000;
        }

        rtt = (recv_time.tv_sec - time_send->tv_sec) * 1000 + (double)recv_time.tv_usec / 1000.0;

        if(rtt > (double)max_wait_time * 1000)
            rtt = max_time;

        if(min_time == 0 | rtt < min_time)
            min_time = rtt;
        if(rtt > max_time)
            max_time = rtt;

        sum_time += rtt;
        if(TTL > 0 && ip_pointer->ip_ttl > TTL){
            printf("Reply from %s : TTL expired in transit.\n",
                    inet_ntoa(recv_addr.sin_addr));
        }
        else{
            printf("%d byte from %s : icmp_seq=%u ttl=%d time=%.1fms.\n",
                    icmp_len,
                    inet_ntoa(recv_addr.sin_addr),
                    icmp_pointer->icmp_seq,
                    ip_pointer->ip_ttl,
                    rtt);
        }
        recv_pack_num++;
    } else{
        printf("throw away the old package %d\tbyte from %s\ticmp_seq=%u\ticmp_id=%u\tpid=%d\n",
               icmp_len, inet_ntoa(recv_addr.sin_addr), icmp_pointer->icmp_seq,
               icmp_pointer->icmp_id, getpid());

        return -1;
    }
    return 0;
}

void Ping::RecvPacket() {
    int recv_size, fromlen;
    fromlen = sizeof(struct sockaddr);

    while(recv_pack_num + lost_pack_num < send_pack_num) {
        fd_set fds;
        FD_ZERO(&fds);              //Clear set
        FD_SET(sock_fd, &fds);      // Add sock_fd to the set

        int maxfd = sock_fd + 1;
        struct timeval timeout;
        timeout.tv_sec = this->max_wait_time;
        timeout.tv_usec = 0;

        //For non-blocking IO
        int n = select(maxfd, NULL, &fds, NULL, &timeout);

        switch(n) {
            case -1:
                fprintf(stderr, "Select error:%s \n\a", strerror(errno));
                exit(1);
            case 0:
                printf("select time out, lost packet!\n");
                lost_pack_num++;
                break;
            default:
                //Judge if sock_fd still in the set
                if(FD_ISSET(sock_fd, &fds)) {
                    //Received a looped package (sent package)
                    if((recv_size = recvfrom(sock_fd, recv_pack, sizeof(recv_pack),
                            0, (struct sockaddr *)&recv_addr, (socklen_t *)&fromlen)) < 0) {
                        fprintf(stderr, "packet error(size:%d):%s \n\a", recv_size, strerror(errno));
                        lost_pack_num++;
                    } else{
                        //Receive potential correct package
                        gettimeofday(&recv_time, NULL);

                        ResolvePacket(recv_size);
                    }
                }
                break;
        }
    }
}

//Report the final Statistic
void Ping::statistic() {
    double total_time;
    struct timeval final_time;
    gettimeofday(&final_time, NULL);

    if((final_time.tv_usec -= first_send_time.tv_usec) < 0) {
        --final_time.tv_sec;
        final_time.tv_usec += 10000000;
    }
    total_time = (final_time.tv_sec - first_send_time.tv_sec) * 1000 + (double)final_time.tv_usec / 1000.0;

    printf("\n--- %s ping statistics ---\n",input_domain.c_str());
    printf("%d packets transmitted, %d received, %.0f%% packet loss, time %.0f ms\n",
            send_pack_num, recv_pack_num, (double)(send_pack_num - recv_pack_num) / (double)send_pack_num,
            total_time);
    printf("rtt min/avg/max = %.3f/%.3f/%.3f ms\n", min_time, (double)sum_time / recv_pack_num, max_time);
    return;
}