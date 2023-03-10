/*
This code is a C program that creates a raw socket and sends an ICMP
(Internet Control Message Protocol) echo request to a specified host IP address.
The program can be run from the terminal with the command ./partb <host_ip_address>,
where <host_ip_address> is the IP address of the host to which the echo request will be sent.
The program then creates a child process that runs the watchdog program and waits for a response from the host.
If no response is received within a certain amount of time,
the child process sends a message to the parent process to terminate.
The parent process then sends another echo request and the process repeats. The program also calculates the checksum of the
ICMP header and prints the round-trip time (RTT) of the echo request and response.
*/

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // gettimeofday()
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

// IPv4 header len without options
#define IP4_HDRLEN 20

// ICMP header len for echo req
#define ICMP_HDRLEN 8

// Checksum algo
unsigned short calculate_checksum(unsigned short *paddress, int len);

#define IP "127.0.0.1"
// i.e the gateway or ping to google.com for their ip-address

int main(char args, char *argv[])
{

    struct sockaddr_in dest_in, tcpStruct;
    struct timeval start, end;
    struct icmp icmphdr; // ICMP-header
    struct iphdr *iphdr_res;

    if (args != 2)
    { // Error in input from terminal
        printf("Error\n");
        return -1;
    }

    memset(&dest_in, 0, sizeof(struct sockaddr_in));
    dest_in.sin_family = AF_INET; // IPv4
    // The port is irrelevant for Networking and therefore was zeroed.
    // dest_in.sin_addr.s_addr = iphdr.ip_dst.s_addr;

    if (strcmp(argv[0], "./partb "))
    { // Checks if the first command in the terminal is ./ping and if so he inserts the ping from the terminal
        dest_in.sin_addr.s_addr = inet_addr(argv[1]);
    }
    else
    {
        printf("Error\n");
        return -1;
    }

    // Create raw socket for IP-RAW
    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        fprintf(stderr, "socket() failed with error: %d", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        return -1;
    }

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

    while (1)
    {
        int tcpSock = socket(AF_INET, SOCK_STREAM, 0);
        if (tcpSock == -1)
        {
            fprintf(stderr, "socket() failed with error: %d", errno);
            return -1;
        }
        memset(&tcpStruct, 0, sizeof(tcpStruct));
        tcpStruct.sin_port = htons(3000);
        tcpStruct.sin_addr.s_addr = INADDR_ANY; // Convert Internet host address from numbers-and-dots notation in CP into binary data in network byte order.
        tcpStruct.sin_family = AF_INET;

        char *command = "./watchdog";
        char *argument_list[] = {"./watchdog", NULL};
        int pid = fork();
        if (pid == 0)
        {
            // Newly spawned child Process. This will be taken over by "ls -l"
            int status_code = execvp(command, argument_list);

            printf("./watchdog has taken control of this child process. This won't execute unless it terminates abnormally!\n");

            if (status_code == -1)
            {
                printf("Terminated Incorrectly\n");
                return 1;
            }
        }
        usleep(500000);

        if (connect(tcpSock, (struct sockaddr *)&tcpStruct, sizeof(tcpStruct)) < 0)
        {
            printf("Couldn't create the connection correctly, error number: %d\n", errno);
            close(sock);
            exit(1);
        }

        if (send(tcpSock, argv[1], strlen(argv[1]) + 1, 0) < 0) // sending the ip
        {
            perror("send() failed");
            close(sock);
            close(tcpSock);
            exit(1);
        }

        char data[IP_MAXPACKET] = "Hello world we are Yuval and Maor.\n";
        int datalen = strlen(data) + 1;
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

        int bytes_sent = -1;
        // Send the packet using sendto() for sending datagrams.

        gettimeofday(&start, 0); // Start the timer

        bytes_sent = sendto(sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)&dest_in, sizeof(dest_in));
        if (bytes_sent == -1)
        {
            fprintf(stderr, "sendto() failed with error: %d\n", errno);
            return -1;
        }

        // Get the ping response
        bzero(packet, sizeof(packet));
        socklen_t len = sizeof(dest_in);
        int bytes_received = -1;

        while ((bytes_received = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest_in, &len)))
        {
            if (bytes_received > 0)
            {
                break;
            }
            else
            {
                printf("Error in recvfrom()\n");
            }
        }

        gettimeofday(&end, 0); // End the timer
        float milliseconds = 0.0f;
        milliseconds = ((end.tv_sec - start.tv_sec) * 1000) + (((double)end.tv_usec - start.tv_usec) / 1000);
        iphdr_res = (struct iphdr *)packet;
        printf("%ld bytes from %s: ", strlen(data) + 1, argv[1]);
        printf("icmp_seq=%d ttl=%d time=%.3f ms\n", icmphdr.icmp_seq++, iphdr_res->ttl, milliseconds);
        settimeofday(&start, 0);
        settimeofday(&end, 0);
        // Make the ping program sleep some time before sending another ICMP ECHO packet.
        usleep(500000);

        // Close the raw socket descriptor.

        char arr[BUFSIZ] = {0};
        if (send(tcpSock, arr, BUFSIZ, 0) < 0) // sending message to check if we need to stop the program.
        {
            perror("send() failed");
            close(sock);
            close(tcpSock);
            exit(errno);
        }
        close(tcpSock);
    }
    close(sock);

    return 0;
}

// Compute checksum (RFC 1071).
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