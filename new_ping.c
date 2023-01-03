#include "arpa/inet.h"
#include "errno.h"
#include "netinet/in.h"
#include "netinet/ip.h"
#include "netinet/ip_icmp.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/socket.h"
#include "sys/time.h" // gettimeofday()
#include "sys/types.h"
#include "unistd.h"
#include "time.h"
#include "fcntl.h"

#define IP4_HDRLEN 20
// ICMP header len for echo req
#define ICMP_HDRLEN 8
#define DESTINATION_IP "8.8.8.8"
#define SOURCE_IP "10.0.2.15"
#define SIZE 12
unsigned short calculate_checksum(unsigned short *paddress, int len);

// run 2 programs using fork + exec
int main()
{
    char* dest_ip = DESTINATION_IP;
// Checksum algo
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    char *args[2];
    // compiled watchdog.c by makefile
    args[0] = "./watchdog";
    args[1] = NULL;
    int status=0;
    int pid = fork();
    if (pid == 0)
    {
        printf("in child \n");
        execvp(args[0], args);
        status =1;
        printf("child exit status is: %d\n", status);
    }
    sleep(1);
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //creating a TCP socket for watchdog
    int newping_sock;
    newping_sock=socket(AF_INET, SOCK_STREAM, 0);
    if(newping_sock==-1){
        printf("there is a problem with initializing newping_sock.\n");
    }
    else{
        printf("-initialize newping_sock successfully.\n");
    }
    //--------------------------------------------------------------------------------
//initialize where to send
    struct sockaddr_in wdog_sock;//initialize where to send

    wdog_sock.sin_family = AF_INET;// setting for IPV4
    wdog_sock.sin_port = htons(3000);//port is 3000
    wdog_sock.sin_addr.s_addr = INADDR_ANY;//listening to all (like 0.0.0.0)
//---------------------------------------------------------------------------------
//connecting the new_ping and watchdog
    int connection_status = connect(newping_sock,(struct sockaddr *) &wdog_sock,sizeof(wdog_sock));
    if(connection_status==-1){
        printf("there is an error with the connection.\n");
    }
    else{
        printf("-connected.\n");
    }
//---------------------------------------------------------------------------------
// Create raw socket for IP-RAW 
    int RAW_SOCK = -1;
    if ((RAW_SOCK = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        fprintf(stderr, "socket() failed with error: %d", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        return -1;
    }
    int i=0;//int for the icmp_seq
    //-----------the start of the loop----------------------//
    while(1){
    struct icmp icmphdr; // ICMP-header
    char data[SIZE] = "sended ping";
    char data2[SIZE] = "sended pong";
    char timeout[8]={0};

    int datalen = strlen(data) + 1;

    //===================
    // ICMP header
    //===================

    // Message Type (8 bits): ICMP_ECHO_REQUEST
    icmphdr.icmp_type = ICMP_ECHO;

    // Message Code (8 bits): echo request
    icmphdr.icmp_code = 0;

    // Identifier (16 bits): some number to trace the response.
    // It will be copied to the response packet and used to map response to the request sent earlier.
    // Thus, it serves as a Transaction-ID when we need to make "ping"
    icmphdr.icmp_id = 18;

    // Sequence Number (16 bits): starts at 0
    icmphdr.icmp_seq = 0;

    // ICMP header checksum (16 bits): set to 0 not to include into checksum calculation
    icmphdr.icmp_cksum = 0;

    // Combine the packet
    char packet[IP_MAXPACKET];

    // Next, ICMP header
    memcpy((packet), &icmphdr, ICMP_HDRLEN);

    // After ICMP header, add the ICMP data.
    memcpy(packet + ICMP_HDRLEN, data, datalen);

    // Calculate the ICMP header checksum
    icmphdr.icmp_cksum = calculate_checksum((unsigned short *)(packet), ICMP_HDRLEN + datalen);
    memcpy((packet), &icmphdr, ICMP_HDRLEN);

    struct sockaddr_in dest_in;
    memset(&dest_in, 0, sizeof(struct sockaddr_in));
    dest_in.sin_family = AF_INET;

    // The port is irrelant for Networking and therefore was zeroed.
    // dest_in.sin_addr.s_addr = iphdr.ip_dst.s_addr;
    dest_in.sin_addr.s_addr = inet_addr(dest_ip);
    // inet_pton(AF_INET, DESTINATION_IP, &(dest_in.sin_addr.s_addr));
        struct timeval start, end;
        gettimeofday(&start, 0);

        // Send the packet using sendto() for sending datagrams.
        int bytes_sent = sendto(RAW_SOCK, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)&dest_in, sizeof(dest_in));//sending the ping
        if (bytes_sent == -1)
        {
            fprintf(stderr, "sendto() failed with error: %d", errno);
            return -1;
        }
        if(((send(newping_sock,data,SIZE,0))==-1)){//if send returned -1 there is an error.
            perror("error in sending data.\n");
            exit(1);
        }
        bzero(data,SIZE);//like memset, it deletes the first SIZE/2 bits
        printf("we sended a ping to watchdog ");

        // Get the ping response
        bzero(packet, IP_MAXPACKET);
        socklen_t len = sizeof(dest_in);
        ssize_t bytes_received = -1;
        while ((bytes_received = recvfrom(RAW_SOCK, packet, sizeof(packet), 0, (struct sockaddr *)&dest_in, &len)))
        {
            if (bytes_received > 0)
            {
                // Check the IP header
                struct iphdr *iphdr = (struct iphdr *)packet;
                struct icmphdr *icmphdr = (struct icmphdr *)(packet + (iphdr->ihl * 4));
                printf(" %ld bytes from %s: ", bytes_received, inet_ntoa(dest_in.sin_addr));
                printf("icmp_seq = %d ",icmphdr->un.echo.sequence+i);
                printf("ttl = %d ",iphdr->ttl);
                /*option for cheking*/
                //if(i==5){
                //sleep(10);//trying to get the timeout.//we will turn it on when we want to check if the watchdog is working.
                //}
        if(recv(newping_sock,&timeout, sizeof(timeout),MSG_DONTWAIT)>0){//getting the timeout from the watchdog.
            if(!strcmp("timeout",timeout))//if the watchdog sent the timeout.
            {
                printf("-The watchdog sent : %s .\n", timeout);
                printf("closing newping_sock and RAW_SOCK...\n"); 
                close(newping_sock);
                close(RAW_SOCK);
                return 0;
            }
        }
                if(((send(newping_sock,data2,SIZE,0))==-1)){//sending the pong.
                    perror("error in sending data.\n");//if send returned -1 there is an error.
                    exit(1);
                }
                bzero(data,SIZE);//like memset, it deletes the first SIZE bits
                break;
            }
        }
        gettimeofday(&end, 0);
        float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
        unsigned long microseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec);
        printf("RTT: %f milliseconds (%ld microseconds)\n", milliseconds, microseconds);//printing the time
        printf("we sended a pong to watchdog\n");
        sleep(1);
        i++;
    }
    //---------------the end of the loop-------------------//
    return 0;
}
unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}

