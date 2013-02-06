/*
* Program: 
*            Election.c
*
* Written by Li Jialong 
*/

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>



#define  MAXLINE  100
#define  DEFAULT_DNS_IP  "8.8.8.8"
#define  DEFAULT_LOCAL_NAME  "jialong"
#define  DEFAULT_DNS_PORT  50489

/*
*Global variables
*/
int n;//the total client number is n.
int tcp_port,udp_port,remote_tcp_port;
int i=0,j=0;
int conn_no = 0,token_num = 0,broadcast_num = 0;//connection # ,received token #,received broadcast token #.
int flag,notseen = 1;//some flags
int nbytes;//Received data number.
int leader;//leader index

char msg[100];//Message
char line[100];//command line
char *ip_address;
char token[10],peer_token[10];//peer_token is the client's final token.
char *T[10];//store the every token client receive 
char *BT[10];//store the broadcasted token
char buf[100];
char ipvfour[INET_ADDRSTRLEN];

struct hostent *host,*he;
struct sockaddr_in local_address;




/*
* Record the connected peer IP and TCP.
*/
struct connected_table {

  int cnnID;
  int fd;
  char *ip;
  char *hostname;
  unsigned short int local_tcp;
  unsigned short int remote_tcp;

};

/*
* Token table
*/
struct token_table{
 
   char *IP;
   int remote_port;
   char tk[10];
   
};

/*
* Broadcast table
*/
struct broadcast_table{

    char MsgID[8];
  char tok[10];
	unsigned int UDPPort;
	char ip_addr[5];
	
	
};


/*
* Parse function
*/
void parseLine(char *line, char *command_array[])
 {
	char *p;
	int count=0;
	p=strtok(line, " ");
	while(p != 0)
	{
	 command_array[count]=p;
	 count++;
	 p=strtok(NULL," ");
	}
	
} 


/*
* Function: Main function.
*/
int main(int argc,char *argv[])
{
   socklen_t len;
   int socktcp,sockudp,client_sockfd;
   char *command_array[100];
   struct connected_table peer_conn[100];
   struct token_table tt[100];
   struct broadcast_table bt[100];
   struct sockaddr_in serv_addr,localAddr,serv_addr_tcp,serv_addr_udp,client_addr;
   struct in_addr ipv4addr;
   
   printf("Election> ");
   
   //get command line.
   gets(line);
   parseLine(line,command_array);
   
   //command: citizen <n> <tcp_port> <udp_port>
   if(strcmp(command_array[0],"citizen") == 0)
   {
      n = atoi(command_array[1]);
      tcp_port = atoi(command_array[2]);
      udp_port = atoi(command_array[3]);
   }
   else
   {
     printf("UNKNOWN COMMAND.\n");
   }
   
   //create tcp socket
   socktcp = socket(AF_INET, SOCK_STREAM, 0);
   if (socktcp < 0) 
   {
    perror("ERROR opening socket.\n"); 
    exit(1);
   }
   
   int optval = 1;
   setsockopt(socktcp, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int)); 
	     
   //create udp socket
   sockudp = socket(AF_INET, SOCK_DGRAM, 0);
   if (sockudp < 0) 
   {
    perror("ERROR opening socket.\n"); 
    exit(2);
   }
   
   setsockopt(sockudp, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int)); 
   
   serv_addr_tcp.sin_family = AF_INET;
   serv_addr_tcp.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr_tcp.sin_port = htons(tcp_port);
   
   serv_addr_udp.sin_family = AF_INET;
   serv_addr_udp.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr_udp.sin_port = htons(udp_port);
   
   if ( bind(socktcp, (struct sockaddr *) &serv_addr_tcp, sizeof(serv_addr_tcp)) < 0 )
   {
    printf("Error binding tcp socket!!!\n");
   }
   
   if ( bind(sockudp, (struct sockaddr *) &serv_addr_udp, sizeof(serv_addr_udp)) < 0 )
   {
    printf("Error binding udp socket!!!\n");
   }
   
   //listen to the local tcp port.
   if (listen(socktcp,5) < 0 )
   {
     perror("Listen error.\n");
   }
   
   printf("Election> ");
   fflush(stdout);
   
   int fmax;
   fd_set read_fds;
   
   if(socktcp > sockudp)
   {
     fmax = socktcp;
   }
   else
   {
     fmax = sockudp;
   }
   
   while(1)
{
    FD_ZERO(&read_fds);
   
    //add TCP socket fd
    FD_SET(socktcp,&read_fds);
    //add UDP socket fd
    FD_SET(sockudp,&read_fds);
    //add stdin fd
    FD_SET(0,&read_fds);
   
	
	for(int i = 0; i < conn_no; i++)
	{
	  FD_SET(peer_conn[i].fd,&read_fds);
	}

    if( select(fmax+1,&read_fds,NULL,NULL,NULL) < 0)
	{
	   perror("Select error.\n");
	   exit(1);
	}	
	
	if( FD_ISSET(0,&read_fds) )
	{
	  //get command line.
	  gets(line);
	  parseLine(line,command_array);
	  
	        //command: connect <ip-address> <tcp-port>
            if(strcmp(command_array[0],"connect") == 0)
			{
				struct sockaddr_in serv_addr;

				ip_address = command_array[1];
				remote_tcp_port = atoi(command_array[2]);
	
				int sockfd = socket(AF_INET, SOCK_STREAM, 0);
				if (sockfd < 0) 
				{
				 perror("ERROR opening socket.\n");
				 exit(2);
				}
	
				serv_addr.sin_family = AF_INET;
				host = gethostbyname(ip_address);
				memcpy(&serv_addr.sin_addr.s_addr, host->h_addr, host->h_length);
				serv_addr.sin_port = htons(remote_tcp_port);

				if( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0 )
				{
					perror("Connection Failed!!!\n");
					exit(3);
				}
				else
				{
				    
					if(sockfd > fmax)
					{
		                 fmax = sockfd;
					}
					
					socklen_t length = sizeof localAddr;
					getsockname(sockfd, (struct sockaddr *) &localAddr, &(length));
					int localPort = ntohs(localAddr.sin_port);
 
					printf("Connection has been established to host %s , port %d !\n"
					,inet_ntoa(serv_addr.sin_addr),ntohs(serv_addr.sin_port) );
		
					inet_pton(AF_INET,inet_ntoa(serv_addr.sin_addr),&ipv4addr);
					he = gethostbyaddr(&ipv4addr,sizeof(ipv4addr),AF_INET);
		 
					//record the peer's IP address and TCP port
					peer_conn[conn_no].cnnID = conn_no+1;
					peer_conn[conn_no].ip = inet_ntoa(serv_addr.sin_addr);
					peer_conn[conn_no].hostname = he->h_name;
					peer_conn[conn_no].local_tcp = localPort;
					peer_conn[conn_no].remote_tcp = ntohs(serv_addr.sin_port);
					peer_conn[conn_no].fd = sockfd;
 					conn_no++;
				
	
					printf("Election> ");
					fflush(stdout);
				}
			}
			
			
	        //command : ready
			else if(strcmp(command_array[0],"ready") == 0)
			{
				printf("Network is ready,please initialize the tokens: ");
				gets(token);
		
				//Initial MessageID
				srandom( time(0) );
	    
				for(int i = 0; i < 7 ; i++ )
				{
					msg[i] = (random() % 10 + 48);
				}
         
				msg[7] = '0';
				//MessageType
				msg[8] = '0';
				//PayloadLength
				msg[9] = '1';
				msg[10] = '0';
				//Payload:token[0..10]->msg[11-21]
				for(int j = 0; j < 10; j++)
				{
					msg[j+11] = token[j];
				}

		       
				//send private message to all its peer
		
				for(int i = 0;i < conn_no; i++)
				{ 
				  send(peer_conn[i].fd,msg,strlen(msg),0);
				}
	
				printf("Election> ");
				fflush(stdout);
				
			}
			
			
			//command : info
			else if(strcmp(command_array[0],"info") == 0)
			{
				struct sockaddr_in server_address;
				int sock_fd = 0, pton_ret, sock_name_no, host_name_no;
				char server_ip[MAXLINE] = DEFAULT_DNS_IP;
				char local_name[MAXLINE] = DEFAULT_LOCAL_NAME;
				uint16_t server_port = DEFAULT_DNS_PORT;
				socklen_t len;
	 
				memset((void *) &server_address, 0, sizeof(server_address));
				server_address.sin_family = AF_INET;	
				server_address.sin_port = htons(server_port);
				pton_ret = inet_pton(AF_INET, server_ip, &server_address.sin_addr);
				if (pton_ret <= 0)
				{
					printf("inet_pton() failed\n");
				}
	 
				//create a UDP socket
				if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
				{
					printf("socket() failed\n");
				}
	 
				if (connect(sock_fd, (struct sockaddr *) &server_address,sizeof(server_address)) == -1)
				{
					printf("connect() failed\n");
					exit(0);
				}
	 
				len = sizeof(local_address);
				sock_name_no = getsockname(sock_fd, (struct sockaddr *) &local_address,&len);
				if (sock_name_no < 0)
				{
					printf("getsockname() error\n");
				}
	 
				printf("IP address | hostname | TCP port | UDP port \n");
				printf("--------------------------------------------------\n");
				printf("%10s",inet_ntoa(local_address.sin_addr));
				printf("|");
				host_name_no = gethostname(local_name, MAXLINE);
				if (host_name_no < 0)
				{
					printf("gethostname() error\n");
				}	
				printf("%20s", local_name);
				printf(" |");
	 
				printf("%6d", tcp_port);
				printf(" | ");
	 
				printf("%6d", udp_port);
				printf(" | \n");
	 
				printf("\n");
	 
				printf("Election> ");
				fflush(stdout);
			}
			
			
			//command : show-conn
			else if(strcmp(command_array[0],"show-conn") == 0)
			{
				printf("cnnID | IP | hostname | local port | remote port \n");
				printf("------------------------------------------------------\n");
				for(int i = 0; i < conn_no;i++)
				{
					printf("%2d",peer_conn[i].cnnID);
					printf(" | ");
		
					printf("%10s",peer_conn[i].ip);
					printf(" | ");
		
					printf("%16s",peer_conn[i].hostname);
					printf(" | ");
		
					printf("%6d",peer_conn[i].local_tcp);
					printf(" | ");
		
					printf("%6d\n",peer_conn[i].remote_tcp);
				}
	  
					printf("\n");
	  
					printf("Election> ");
					fflush(stdout);
			}
			
			
			//command : self-token
			else if(strcmp(line,"self-token") == 0)
			{
				if(flag > 0)
				{
					printf("Self-token is : ");
					for(int count = 0; count < 10 ; count++)
					{
					  printf("%c",peer_token[count]);
					}
					printf("\n");
				}
				else
				{
					printf("WAITING ON PEER_TOKEN.\n");
				}
	 
				printf("Election> ");
				fflush(stdout);
	 
			}
			
			
			//command : all-tokens
			else if(strcmp(command_array[0],"all-tokens") == 0)
			{
			    
				printf("IP | remote port | token \n");
				printf("------------------------------------------\n");
	 
	            for(int i = 0; i < token_num ; i++)
				{
				    
					printf("%10s",tt[i].IP);
					printf(" | ");
	   
					printf("%6d",tt[i].remote_port);
					printf(" | ");
	   
					for(int j = 0; j < 10; j++)
					{
					  printf("%c",tt[i].tk[j]);
					}
					
					printf("\n");
				}
				
				for(int i = 0;i < broadcast_num;i++)
				{   
				    inet_ntop(AF_INET,bt[i].ip_addr,ipvfour,INET_ADDRSTRLEN);
					
					printf("%10s",ipvfour);
					printf(" | ");
	   
					printf("%6d",bt[i].UDPPort);
					printf(" | ");
	   
					for(int j = 0; j < 10; j++)
					{
					  printf("%c",bt[i].tok[j]);
					}
					
					printf("\n");
				}
				
					printf("Election> ");
					fflush(stdout);
	 
			}
			
			
			//command : exit
			else if(strcmp(command_array[0],"exit") == 0)
			{
				exit(0);
			}
   
			//other command will display error.
			else
			{
				printf("UNKNOWN COMMAND.\n");
	 
				printf("Election> ");
				fflush(stdout);
			}
			
	} 
	
	//receice new connection
	if( FD_ISSET(socktcp,&read_fds) )
	{
	   len = sizeof(client_addr);
	   client_sockfd = accept(socktcp, (struct sockaddr *) &client_addr, &len);
	   if (client_sockfd < 0) 
	   {
		  perror("ERROR opening client socket.\n"); 
		  exit(3);
	   }
	    
	   if(client_sockfd > fmax)
	   {
		  fmax = client_sockfd;
	   }
		
		
	   printf("Received the connection from host %s ,port %d .\n",inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port) );
	   inet_pton(AF_INET,inet_ntoa(client_addr.sin_addr),&ipv4addr);
	   he = gethostbyaddr(&ipv4addr,sizeof(ipv4addr),AF_INET);	  
		
	   peer_conn[conn_no].cnnID = conn_no+1;
	   peer_conn[conn_no].ip = inet_ntoa(client_addr.sin_addr);
	   peer_conn[conn_no].hostname = he->h_name;
	   peer_conn[conn_no].local_tcp = tcp_port;
	   peer_conn[conn_no].remote_tcp = ntohs(client_addr.sin_port);
	   peer_conn[conn_no].fd = client_sockfd;
	   conn_no++;		
		   
	   printf("Election> ");
       fflush(stdout);
	}
	
	//receive UDP data 
	if( FD_ISSET(sockudp,&read_fds) )
	{
	   struct sockaddr rv_addr;
	   len = sizeof rv_addr;
		   
	   //received salute message
	   recvfrom(sockudp,buf,sizeof buf,0,&rv_addr,&len);
				
	   
		char temp_token[10];
				   
		for(int i = 0; i < 10; i++)
		{
		   temp_token[i] = buf[i+11];
		}
				   
		printf( "Leader received the salute message from peer : ");
					
		for(int i = 0 ; i < 10 ; i++)
		{
		   printf("%c",temp_token[i]);  
		}
		  
		   printf("\n");
		   
	}
	

    //receive user's TCP data.
    for(int k = 0; k < conn_no ; k++)
	{
		 struct sockaddr_storage addr;
		 char ipstr[1000];
		 int temp_port;
		 len = sizeof(struct sockaddr_in); 
		
	    if( FD_ISSET(peer_conn[k].fd,&read_fds) ) 
		{
		  
		  //receive TCP data 
			
			if(nbytes = recv(peer_conn[k].fd,buf,100,0) <= 0)    
			{
			  if(nbytes == 0)
			  {
			    printf("selectserver: socket %d hung up\n", peer_conn[k].fd);
			  }
			  else
			  {
			    perror("Recv.\n");
				exit(0);
			  }
			  close(peer_conn[k].fd);
			  FD_CLR(peer_conn[k].fd, &read_fds);
			  
			}
			
			else
			{
			   //receive private message
				if(buf[8] == '0')
				{
					char tmp[10];
					for(int i = 0; i < 10; i++)
					{
						tmp[i] = buf[i+11]; 
					}
	   
					getpeername(peer_conn[k].fd, (struct sockaddr*)&addr, &len);
					struct sockaddr_in *s = (struct sockaddr_in *)&addr;
					temp_port = ntohs(s->sin_port);
					inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
		            
					printf("Received the token: ");
					for(int i = 0; i < 10; i++)
					{
					  printf("%c",tmp[i]);
					}
					printf(" from host %s ,port %d .\n",ipstr,temp_port);
	     
					tt[token_num].IP = ipstr;
					tt[token_num].remote_port = temp_port;
					strcpy(tt[token_num].tk,tmp);
					token_num++;		
	   
					//Determine its own token
					if(token_num == conn_no)
					{
						
						char max[10]; 
						strcpy(max,tt[0].tk);
					   
						for(int i = 1; i < token_num;i++)
						{
						  if( strcmp(max,tt[i].tk) < 0 )
						  {
						    strcpy(max,tt[i].tk);
						  }
						}
						
						strcpy(peer_token,max);
						
						printf("My own token is:");
						for(int i = 0; i < 10; i++)
						{
						  printf("%c",peer_token[i]);
						}
						printf("\n");
						
						//If determine the peer token ,then flag = 1.
		                flag = 1;
						
						//broadcast its own token to all its peers
						
		                //Initial MessageID
				         srandom( time(0) );
	    
						for(int i = 0; i < 7 ; i++ )
						{
							msg[i] = (random() % 10 + 48);
						}
         
						msg[7] = '0';
						//MessageType
						msg[8] = '1';
						//PayloadLength
						msg[9] = '1';
						msg[10] = '6';
				           
						for(int i = 0;i < 10;i++)
						{
						  msg[i+11] = peer_token[i];
					    }
					
						memcpy(&msg[21],&local_address.sin_addr.s_addr,4*sizeof(char) );
                        memcpy(&msg[25],&udp_port,2*sizeof(char));
					
						
						//send its own token to all its peers
						for(int i = 0; i < conn_no ; i++)
						{
						   send(peer_conn[i].fd,msg,strlen(msg),0);
						}
							
					}
					
					printf("Election> ");
				    fflush(stdout);
	  
				}
				
				
				//receive broadcast message
				if(buf[8] == '1')
				{
				    char msgID[8],t[10],port[2],ip[4];
					unsigned short int port_num;
					
					//Get the MessageID
	                for(int i = 0;i < 8; i++)
					{
					  msgID[i] = buf[i];
					}
					
					//Get the token
					for(int i = 0; i < 10; i++)
					{
						t[i] = buf[i+11]; 
					}
					
					//Get the IPAddress
					for(int i = 0; i < 4;i++)
					{
				      ip[i] = buf[i+21];
			        }
					
					//Get the UDPPort
					for(int i = 0; i < 2;i++)
					{
					  port[i] = buf[i+25];
					}
					memcpy( &port_num,port,2*sizeof(char) );
					
					for(int i = 0;i < broadcast_num; i++)
					{
					  //If it has already seen the message,discard it
					  if(strcmp(bt[i].MsgID,msgID) == 0)
					  {
					    printf("Discard this broadcast token!\n");
						notseen = 0;
					  }
					   
					}
					
					//If the message has not been seen,save and forward it.
					if(notseen > 0)
					{
					  strcpy(bt[broadcast_num].MsgID,msgID);
					  strcpy(bt[broadcast_num].tok,t);
					  printf( "Received the broadcast peer token: ");
					  for(int i = 0; i < 10; i++)
					  {
					    printf("%c",bt[broadcast_num].tok[i]);
					  }
					  printf("\n");
					  
					  memcpy(&bt[broadcast_num].ip_addr,ip,4*sizeof(char) );
					  memcpy(&bt[broadcast_num].UDPPort,&port_num,2*sizeof(char) );
					  
					  broadcast_num++;
					  
					  //Forward the message to all its peers except initializer.
					  for(int j = 0;j < conn_no;j++)
					  {		
					    if( j != k )
						{
						  send(peer_conn[j].fd,buf,strlen(buf),0);	
						}
					    
					  }
					  
					}
				
				}
				
				//If receive all other's peer token.
				if( ( broadcast_num == n-1 )&&( flag == 1 ) )
				{
					   char max[10];
					   
					   strcpy(max,bt[0].tok);
					   leader = 0;
					   
					   for(int i = 1;i < broadcast_num;i++)
					   {
					     if( strcmp(max,bt[i].tok) < 0 )
						 {
						   leader = i;
						   strcpy(max,bt[i].tok);
						 }
						 
					   }
					   
					   
					   //send the salute message
					   if ( strcmp(max,peer_token) > 0 )
					   {
					            int sock;
								char saluteMsg[100];
							    struct sockaddr_in dest;
							
								//send the UDP message to the leader.
								
								sock = socket(AF_INET,SOCK_DGRAM,0);
						  
								dest.sin_family = AF_INET; 
								inet_ntop(AF_INET,bt[leader].ip_addr,ipvfour,INET_ADDRSTRLEN);
								inet_pton(AF_INET,ipvfour,&dest.sin_addr);
								dest.sin_port = htons(bt[leader].UDPPort);
                                
								//printf("destination is %s ,port is %d ",ipvfour,bt[leader].UDPPort);
								srandom( time(0) );
	    
								for(int i = 0; i < 7 ; i++ )
								{  
									saluteMsg[i] = (random() % 10 + 48);
								}
         
								saluteMsg[7] = '0';
								//MessageType
								saluteMsg[8] = '2';
								//PayloadLength
								saluteMsg[9] = '3';
								saluteMsg[10] = '6';
						   
								//msg[11..20]
								for(int i = 0; i < 10 ;i++)
								{	
									saluteMsg[i+11] = peer_token[i];
								}
						   
								//msg[21..36]
								char MessageText[100] = "ALL HAIL THE MIGHTY LEADER";
	
								for(int i = 0; i < 26 ; i++)
								{
									saluteMsg[i+21] = MessageText[i];
								}
						   
								if ( sendto(sock,saluteMsg,strlen(saluteMsg),0,(struct sockaddr *)&dest,sizeof dest) < 0)
								{
									perror("Send fail!!!\n");
								}
								else
								{
									printf("Send the salute message to the leader!!!\n");
								}
						   
					 }
					 
					 printf("Election> ");
				     fflush(stdout);
					    
				}
			}
		
		}
             
						
    }
  
 }	
 
}
