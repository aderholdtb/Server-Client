#include<iostream>
#include<sstream>
#include<cstdlib>
#include<fstream>
#include<string>
#include<vector>
#include<signal.h>
#include<netinet/in.h>
#include<sys/types.h> /* Prerequisite typedefs */
#include<errno.h> /* Names for "errno" values */
#include<sys/socket.h> /* struct sockaddr; system call*/
#include<netdb.h> /* network information lookup */
#include<netinet/in.h> /* struct sockaddr_in; byte */
#include<arpa/inet.h> /* utility function prototypes */
using namespace std;

void leave(int sig); /* Things to do after signal handler is called*/
void print(char []);// PRINTING THE MESSAGE RECEIVED

static void *do_recv(void *);//THREAD FOR RECEIVING MESSAGES
static void *do_send(void *);// THREAD FOR SENDING MESSAGES

void checkIfTerminate(char []);//CHECKS IF THE EXIT FUNCTION WAS CALLED
void signal_handler(int signal); //CHECKS IF THE PROGRAM TERMINATED

bool donePrinting = true;//CHECKS IF THE WHOLE MESSAGE WAS PRINTED BEFORE RECEIVING

int main(int argc, char *argv[]) {// THE USER ENTERS THE HOST NAME AND THE PORT NUMBER OF THE HOST
  signal(SIGINT,signal_handler);/* Signal handler */
  int len = 255;// THE FIXED LENGTH OF INCOMING MESSAGE
  int port;// THE PORT OF THE CLIENT
  struct sockaddr_in serv_addr;// THE INFO OF THE SERVER
  struct hostent *server;// THE SERVER INFO

  if(argc < 2){//CHECKS FOR AN ERROR WITH THE PORT NUMBER
    cout<<"error with port number"<<endl;
    exit(1);
  }

  port = atoi(argv[2]);// GETS THE PORT NUMBER

  int sockfd = socket(AF_INET, SOCK_STREAM, 0); //creating socket

  if(sockfd < 0){//CHECKS FOR A SOCKET ERROR
    cout<<"error with socket"<<endl;
    exit(1);
  }

  server = gethostbyname(argv[1]);// GETS THE HOST NAME

  if(server == NULL){// CHECKS IF THERES A HOST ERROR
    cout<<"That host doesnt exist"<<endl;
    exit(1);
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));// FREES MEMORY OF SERVER ADDRESS

  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);

  serv_addr.sin_port = htons(port);

  //Send a connection request to server//
  if(connect(sockfd, (struct sockaddr*) &serv_addr,  sizeof(serv_addr)) < 0){
    cout<<"error with connect"<<endl;
    exit(1);
  }

  char connected[255] = "New Connection!!!\n";

  send(sockfd, connected, strlen(connected) + 1, 0);// INITIAL CONNECTION TO THE SERVER

  //==================================================================
  pthread_t chld_thr1, chld_thr2;// CREATES TWO THREADS
  int *new_sock;// POINTER FOR THE SOCKET

  new_sock = (int*)malloc(1);// FREES THE POINTER
  *new_sock = sockfd;//// POINTS TO THE HOST SOCKET

      /* Create a thread to send data */
      pthread_create(&chld_thr1, NULL, do_recv, (void *) new_sock); 

      /* Create a thread to receive data */
      pthread_create(&chld_thr2, NULL, do_send, (void *) new_sock); 

      pthread_join(chld_thr1, NULL);// JOINS THE RECEIVE THREAD
      pthread_join(chld_thr2, NULL);//JOINS THE SEND THREAD

   //==================================================================
   // Close connection and exit
  
 return 0;
}
/****************************************************************************/
//PURPOSE: THIS IS A THREAD BECAUSE IT ALLOWS FOR CONSTANT SENDING, WITH NO
//         INTERUPTIONS SO THAT THE CHAT FUNCTION IS OPERATIONAL
/****************************************************************************/
//////////////////////////// SENDING /////////////////////////////////////////
static void *do_send(void *arg){ 
  int sockfd = *(int*)arg;// THE HOSTS SOCKET
  char payload1[512] = "";// THE PAYLOAD FOR SENDING MESSAGES
  string message; // THE MESSAGE ENTERED BY THE USER

  while(donePrinting == false){}// CHECKS IF THE PERVIOUS MESSAGE IS DONE PRINTING
  
    for( ; ; ){
      getline(cin, message);// GETS THE MESSAGE FROM THE USER
      
      for(int i = 0; i < message.size(); i++)// CONVERTS THE MESSAGE TO A CHAR ARRAY
	payload1[i] = message[i];
      
      send(sockfd, payload1, strlen(payload1), 0);// SENDS THE MESSAGE TO THE SERVER
      bzero(payload1, 512);// FREES THE MEMORY OF THE PAYLOAD
    }

  pthread_exit(NULL); /* Close the current thread. */
}
/********************************************************************************/
//PURPOSE: TO RECEIVE MESSAGES AT THE SAME TIME OF SENDING A MESSAGE USING A THREAD
//         RECEIVES THE MESSAGE FROM THE SERVER AND THEN PRINTS THE MESSAGE
/********************************************************************************/
////////////////////////////// RECEIVING /////////////////////////////////////////
static void *do_recv(void *arg) 
{ 
  char payload[512] = "";// THE INCOMING PAYLOAD FROM THE SERVER
  int sockfd = *(int*)arg;// THE SOCKET OF THE SERVER
  int len = 1000;// THE MAX LENGTH OF A MESSAGE

  while(donePrinting == false){}// CHECKS IF THE PREVIOUS MESSAGE IS DONE PRINTING

    for( ; ; ){
      if(recv(sockfd, payload, len, 0) == 0){//CHECKS IF THE HOST WAS DISCONNECTED
	cout<<"\nLost connection to the host!!!\nTerminating program!!!\n"<<endl;
	exit(1);
      }
      print(payload);//PRINTS THE RECEIVED MESSAGE
      checkIfTerminate(payload);// CHECKS IF THE CLIENT WANTS TO EXIT

      bzero(payload, 512);// RESETS THE PAYLOAD
    }
    pthread_exit(NULL); /* Close the current thread. */
}
/**********************************************************************************/
// PURPOSE: CHECKS IF THE USER HIT THE EXIT FUNCTION, IF THEY DID THE PROGRAM
//          WILL TERMINATE
/**********************************************************************************/
////////////////////// CHECK IF TERMINATE //////////////////////////////////////////
void checkIfTerminate(char payload[255]){
  string term = "terminate";// INITIALIZE THE STRING TO TERMINATE 

  if(payload == term){// IF THE MESSAGE FROM THE SERVER IS "terminate" THEN TERMINATE THE PROGRAM
    cout<<"\nYou have ended your chat session!!!"<<endl;
    exit(1);
    pthread_exit(NULL); /* Close the current thread. */
  }
}
/**********************************************************************************/
// PURPOSE: THE PURPOSE OF THIS FUNCTION IS TO PRINT ALL OF THE MESSAGES
/**********************************************************************************/
///////////////////////////////// PRINT ////////////////////////////////////////////
void print(char payload[512]){
  for(int i = 0; i < strlen(payload); i++){
    donePrinting = false;// THE FUNCTION IS NOT DONE PRINTING
    if(payload[i] == '%')// IF THERE IS A 5 SIGN THEN ADD A SPACE
      cout<<" ";
    else
      cout<<payload[i];// COUT THE LETTER OF THE PAYLOAD
  }
  donePrinting = true;// PRINTING IS DONE
}
/***********************************************************************************/
//PURPOSE: THE PURPOSE OF THIS FUNCTION IS TO CHECK IF THE USER TERMINATED THE PROGRAM
/***********************************************************************************/
////////////////////////////// SIGNAL HANDLER ///////////////////////////////////////
void signal_handler(int signal)
{
  /* Print status message to the user. */
  cout<<"\nYou have ended your chat session!!!"<<endl;

  exit(1);// EXIT THE PROGRAM
}
