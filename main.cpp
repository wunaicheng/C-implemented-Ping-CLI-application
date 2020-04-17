#include <signal.h>
#include "ping.h"

using namespace std;
void SingnalHandler(int signo) {

    p->statistic();

    exit(0);
}

int main(int argc, char * argv[]) {
    int k = 0;
    if(argc <2 || argc >3){
        cout<<"Wrong input argument number, you must enter an IPv4 address or Valid Domain name as the first argument and optionally enter a TTL value between 1 and 255 as a second argument.";
        exit(1);
    } 
    if(argc == 3){
        if(stoi(argv[2])<1 ||stoi (argv[2])>255) {
            cout<<"Wrong TTL value, a valid TTL value should between 1 and 255.";
        }
        else k = stoi(argv[2]);
    } 
    struct sigaction action;

    action.sa_handler = SingnalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGINT,&action,NULL);
    cout<<"... Ping Program started, to stop, please use Ctrl+C ...\n";

    Ping ping(argv[1], k);
    ping.CreateSocket();
    while(true)
    {
        ping.SendPacket();
        ping.RecvPacket();
        sleep(1);
    }
    return 0;
}