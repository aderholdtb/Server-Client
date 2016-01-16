#include<iostream>
#include<stdio.h>
#include<cstdlib>
#include<fstream>
#include<string>
#include<vector>
#include<pthread.h>/* Prerequisite pthread */
#include<netinet/in.h>
#include<sys/types.h> /* Prerequisite typedefs */
#include<errno.h> /* Names for "errno" values */
#include<sys/socket.h> /* struct sockaddr; system call*/
#include<netdb.h> /* network information lookup */
#include<netinet/in.h> /* struct sockaddr_in; byte */
#include<arpa/inet.h> /* utility function prototypes */
using namespace std;

static void *do_child(void *);// THREAD FOR EACH CLIENT
int createUsername(int);// CREATES THE USERNAME OF A NEW CLIENT
void listUsers(int, bool);// LISTS ALL USERS, EVEN THE CURRENT CLIENT
int findChat(int);// LISTS AVAILABLE USERS, AND ASKS USER TO SELECT ONE TO CHAT WITH
void print(char []);// PRINTS THE MESSAGES
void changeToNotChatting(int);// CHANGES A USERS FLAGS TO NOT CHATTING
int chatting(int, int);//THE CHAT FUNCTION TO FORWARD AND RECEIVE MESSAGES FOR CHATTING
void quit(int);//ALLOWS A CLIENT TO QUIT FROM THE SERVER
int checkIfPending(int);//CHECKS IF THERE IS A PENDING INVITE FOR CHATTING

int sockfd;//THE SERVER SOCKET
string USERNAME;// THE CLIENTS USERNAME
string CHATUSER;// THE CHAT CLIENT OF THE USER
int USERSOCK;// THE CLIENTS SOCKET
char payload[512];// THE PAYLOAD FOR SENDING AND RECEIVING MESSAGES
int len = 512;// THE FIXED LENGTH OF AN INCOMING MESSAGE

struct sockaddr_in cli_addr, serv_addr;//INFO FOR THE CURRENT CLIENT AND SERVER
//===================================================================
//===================== Main ========================================
//===================================================================
int main(int argc, char *argv[]) {// USER NEEDS TO ENTER THE PORT NUMBER FOR THE SERVER TO BE ON
  int port;//THE CHOSEN PORT NUMBER
  int newsockfd;// THE NEW CLIENTS SOCKETS
  socklen_t clientLength;// THE CLIENTS LENGTH
  int MAXCONNECT = 5;// ALLOWS UP TO 5 CLIENTS
  int tempfd;// THE CLIENTS SOCKET
  
  remove("address.txt");//DELETES THE TXT FILE FOR THE PREVIOUS RUNNING OF THE PROGRAM
  
  if(argc < 2)// CHECKS FOR A PORT NUMBER ERROR
    cout<<"Error with port number"<<endl;
  
  sockfd = socket(AF_INET, SOCK_STREAM, 0);// THE SERVER SOCKET

  if(sockfd < 0){// CHECKS IF THERE IS A SOCKET ERROR
    cout<<"socket error"<<endl;
    exit(1);
  }
  
  bzero((char *) &serv_addr, sizeof(serv_addr));// CLEARS THE SERVER INFO
  
  port = atoi(argv[1]);//GETS THE PORT NUMBER
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){// BINDS THE SERVER INFO
    cout<<"bind error"<<endl;
    exit(1);
  }

  MAXCONNECT = 5;// MAX AMOUNT OF USERS ON THE SERVER

  listen(sockfd, MAXCONNECT);
  clientLength = sizeof(cli_addr);

  cout << "Waiting on client...." << endl;
  int *new_sock;//POINTER FOR THE SOCKETS
  
  while(tempfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clientLength)){
    pthread_t chld_thr;// CREATES A NEW CLIENT THREAD
    new_sock = (int*)malloc(1);// FREES THE POINTER FOR A NEW CLIENT
    *new_sock = tempfd;// THE CLIENTS SOCKETS

    /*Create a new thread to handle the connections. */
    if(pthread_create(&chld_thr, NULL, do_child, (void *) new_sock) < 0)
      cout<<"Error creating Thread"<<endl;
    
    cout<<"Handler assigned!!!"<<endl;
  }
  //Close listen connection and exit.
  close(tempfd);
  close(sockfd);
  return 0;
}
//===================================================================
//===================== do_child=====================================
//===================================================================
/*******************************************************************/
// PURPOSE: ALLOWS FOR THE USER TO SELECT FROM A LIST OF FEATURES
//          1. WILL ALLOW THE USER TO LIST ALL USERS 2. WILL ALLOW 
//          THE USER TO SELECT FROM THE AVAILABLE USERS TO CHAT WITH
//          3. WILL EXIT THE USER
/*******************************************************************/
////////////////////////// USERS MAIN ///////////////////////////////
/*Function executed by the new thread */
static void *do_child(void *arg) {
  int msg_type; // THE CHOICE SELECTED BY THE USER
  int tempfd = *(int*)arg;// THE CLIENTS SOCKET
  const int list = 1;// LISTS ALL USERS
  const int chat = 2;// CHAT FUNCTION
  const int exit = 3;// USER EXITS
  const char terminate[20] = "terminate";// THE TERMINATE FUNCTION
  const char menu[512] = "\n==== MENU ====\n1. List users\n2. Chat\n3. Exit\nEnter your choice: \n";// THE MENU FOR THE USER
  bool skip = false;//IF THE USER QUIT UNEXPECTEDLY
 
  recv(tempfd, payload, len, 0);//RECEIVES THE INITIAL MESSAGE FROM THE USER
  print(payload); // PRINTS THE USER CONNECTION IS FINE
    
  createUsername(tempfd);// CREATES A USERNAME FOR THE NEW CLIENT
   
    for( ; ; ){
      if(skip == false){// IF THERE WAS NOT AN UNEXPECTED QUIT
	msg_type = checkIfPending(tempfd);// IF THERE IS A PENDING INVITE
	if(msg_type != -1)//THE USER ENTERED THE MESSAGE SELECTION INTHE CATTING MENU
	  skip = true;//THERE WAS AN UNEXPECTED QUIT
	}

      bzero(payload, 512);// RESETS THE MESSAGE

      if(skip == false){// IF THERE WAS NOT AN UNEXPECTED QUIT
	if(send(tempfd, menu, strlen(menu), 0) < 0)// SEND THE MENU TO THE USER
	  cout<<"error"<<endl;

	if(recv(tempfd, payload, len, 0) == 0)// RECEIVE THE MESSAGE TYPE OF THE USER
	  quit(tempfd);
	msg_type = payload[0] - '0';// CONVERT THE CHAR TO AN INT
      }
      else
	skip = false;
            
      if(msg_type == list){// IF THE MESSAGE TYPE WAS ONE
	 cout<<"Listing all Users..."<<endl;
	 listUsers(*(int*)arg, true);
       }
      if(msg_type == chat){// IF THE MESSAGE TYPE WAS TWO
	 cout<<"finding users to chat with..."<<endl;
	 msg_type = findChat(*(int*)arg);

	 if(msg_type != -1)//IF THERE WAS AN UNEXPECTED QUIT
	   skip = true;
       }
      if(msg_type == exit){// IF THE MESSAGE TYPE WAS 3
	send(tempfd, terminate, strlen(terminate), 0);// SENDS MESSAGE TO USER
	quit(tempfd);// QUITS THE THREAD
       }  
    }
}

//===================================================================
//===================== create username =============================
//===================================================================
//PURPOSE: THIS IS TO ALLOW THE USER TO CREATE A USERNAME, IF
//         THE USERNAME WAS TAKEN IT WILL PROMPT THE USER TO ENTER 
//         ANOTHER ONE
/*******************************************************************/
///////////////////////// CREATE USERNAME ///////////////////////////
int createUsername(int tempfd){
  ifstream inFile;// THIS IS FOR TAKING OF INFO FROM THE FILE
  ofstream outFile;// ENTERING THE USERS INFO TO THE TXT FILE
  
  char selection[512] = "\nPlease select a username: \n";//PROMPT FOR USER
  char success[512] = "\nYou have successfully created the username: \n";//SUCCESS
  char taken[512] = "\nThat username has been taken!!!\n%";//DENIED
  bool check = false;//IF THE USERNAME ALREADY EXISTS
  string temp;// THE TEMP USERNAME
  
  while(check == false){// IF THE USERNAME WAS TAKEN
    inFile.open("address.txt");// OPENS THE FILE
    inFile.seekg (0, ios::beg);// STARTS FROM THE BEGINNING OF THE FILE
    
    check = true;// RESETS THE CHECK BOOL
    send(tempfd, selection, strlen(selection) + 1, 0);//SENDS USER PROMPT
    
    bzero(payload, 255);// RESETS THE PAYLOAD
    if(recv(tempfd, payload, len, 0) == 0)// RECEIVES THE USERNAME FROM HE USER
      quit(tempfd);
    
    USERNAME = payload;// THE GLOBAL CLIENT USERNAME IS THEN SET

    while(inFile>>temp){// GOES INTO THE FILE
      inFile>>temp;// GETS THE USERNAME
      
      if(temp == USERNAME){//IF THE USERNAME IS TAKEN
	send(tempfd, taken, strlen(taken) + 1, 0);// SENDS THE DENIED MESSAGE
	check = false;// GOES THROUGH THE LOOP AGAIN
      }
      for(int i = 0; i < 2; i++)// FAST FORWARDS THROUGH DATA
	inFile>>temp;
    }
    inFile.close();// CLOSES THE FILE
  }
  
  outFile.open("address.txt", std::ios::app);//add to the back of the file
  
  outFile<<tempfd<<" ";//OUTPUTS THE USER SOCKET
  outFile<<payload<<" ";// OUTPUTS THE USERNAME
  outFile<<inet_ntoa(cli_addr.sin_addr)<<" ";// OUTPUTS THE IP ADDRESS
  outFile<<"not_chatting"<<endl;// SETS THE FLAG
    
  strcat(success,payload);//CONCATINATES THE USERNAME TO THE SECCESS STRING
    
  send(tempfd, success, strlen(success) + 1, 0);// SENDS THE SUCCESS MESSAGE
    
  outFile.close();// CLOSES THE OUT FILE
    return 1;
}
/**************************************************************************
PURPOSE: DELETES ALL OF THE USER INFO UPON QUITTING FROM THE SERVER, THEN IT 
         CLOSES THE THREAD
***************************************************************************/
////////////////////////////// QUIT ////////////////////////////////////////
void quit(int tempfd){
  vector<string> temp;// HOLDS ALL OF THE DATA FOR OUTPUT
  string getInfo;// THE HOLDER FOR THE DATA FROM THE FILE
  ifstream inFile;// FOR GETTING THE DATA FROM THE FILE
  ofstream outFile;// OUTPUTS THE UPDATED INFO

  cout<<"Client disconnected..."<<endl;//OUTPUTS THE CLIENTS BEEN DISCONNECTED ON SERVER SIDE
  cout<<"Removing clients information..."<<endl;

  inFile.open("address.txt");//OPENS THE FILE

  while(inFile>>getInfo){//GETS THE SOCKET

    if(atoi(getInfo.c_str()) == tempfd){// IF THE SOCKET MATCHES THE QUITTING USER
      for(int i = 0; i < 3; i++)// PASSES ALL OF ITS DATA
        inFile>>getInfo;
    }
    else{// IF IT DOSENT MATCH THEN ENTER ALL THE DATA INTO THE VECTOR
      temp.push_back(getInfo);

      for(int i = 0; i < 3; i++){
        inFile>>getInfo;
        temp.push_back(getInfo);
      }
    }
  }

  inFile.close();// CLOSES THE TXT FILE
  outFile.open("address.txt");// OPENS THE OUTPUT FILE

  for(int i = 0; i < temp.size(); i++){//OUTPUTS ALL THE DATA IN THE VECTOR INTO THE TXT DOC
    outFile<<temp[i];
    outFile<<" ";

    if(i % 4 == 3)
      outFile<<"\n";
  }

  outFile.close();// CLOSES THE OUTPUT FILE
  pthread_exit((void *)0);//CLOSES THE THREAD
}
/*****************************************************************************
PURPOSE: ALLOWS FOR THE USER TO WAIT FOR A REPONSE ON SENDING AN INVITE
         IF THE USER ACCEPTS THE MESSAGE A SUCCESS MESSAGE WILL BE SENT
         ALONG WITH CALLING THE CHATTING FUNCTION FOR CHATTING, IF THE USER 
         LOGGED OFF OR DENIES THE INVITE THEN THE USER WILL RECEIVE A DENIED 
         MESSAGE
*****************************************************************************/
//////////////////////// WAIT FOR A RESPONSE /////////////////////////////////
int waitForResponse(int tempfd, int userSock){
  ifstream inFile;// OPENS THE FILE
  string info;//INFO INT HE FILE
  int tempSock;// THE TEMPORARY SOCKET INT H FILE
  const char decline[255] = "Declined the invite";// THE DENIED PROMPT
  const char accept[255] = "Accepted the invite";// THE SUCCESS PROMPT
  string currentUsername = USERNAME;// THE CURRENT USERNAM OF THE CLIENT
  bool checkIfOnline = true;// CHECKS IF THAT USER IS STILL ONLINE

  for( ; ; ){
    inFile.open("address.txt");// OPENS THE FILE OF THE USERS
    checkIfOnline = true;// INITIALIZES THE CHECK TO TRUE UNTIL PROVEN OTHERWISE
    while(inFile>>info){// GETS THE SOCKETS NUMBER
      tempSock = atoi(info.c_str());// CONVERTS THE SOCKET NUMBER
      if(tempSock == userSock){// IF THE SOCKET MATCHES
	checkIfOnline = false;// THE USER IS STILL ONLINE
        for(int i = 0; i < 3; i++)// SKIPS TO THE NEXT USER
          inFile>>info;

        if(info == "pending" + currentUsername){//IF THE INVITE IS STILL PENDING
          continue;
	}
        else if(info == currentUsername){// IF THE USER ACCEPTED
          cout<<"accepted"<<endl;
	  return chatting(userSock, tempfd);//START CHATTING WITH THE CLIENT
	}
        else if(info == "not_chatting"){// THE USER DENIED THE MESSAGE
          cout<<"declined"<<endl;
          send(tempfd, decline, strlen(decline), 0);// SEND THE DENIED MESSAGE
          return -1;
        }
      }
      else{
        for(int i = 0; i < 3; i++)
          inFile>>info;
      }
    }
    if(checkIfOnline == true){//IF THE USER IS OFFLINE
      send(tempfd, decline, strlen(decline), 0);// SEND THE DENIED MESSAGE
      return -1;
    }
    inFile.close();// CLOSE THE FILE
  }//RECYCLE THROUGH THE LOOP TO CHECK AGAIN
}
/******************************************************************************
PURPOSE: CHANGE THE USERS FLAG TO CHATTING, SET TO THE CHATTING USERS NAME
******************************************************************************/
////////////////////////// CHANGE TO CHATTING /////////////////////////////////
int changeToChatting(int tempfd){
  ifstream inFile;// OPENS THE FILE FOR RECEIVING THE DATA FROM THE FILE
  ofstream outFile;// FOR INPUTING THE UPDATED DATA
  string tempusers;// THE TEMPORARY USERS FROM THE FILE(HOLDER)
  vector<string> temp;// ALL THE DATA FROM THE FILE TO BE OUTPUTED
  string change = "";// CHECKS IF THE FLAG IS PENDING
  string tempName;// THE TEMPORARY NAME IN THE FILE(HOLDER)
  int chatSock;// THE CHATTERS SOCKET TO CHAT

  inFile.open("address.txt");// OPENS THE FILE

  while(inFile>>tempusers){// GETS ALL DATA FROM THE FILE
    temp.push_back(tempusers);
  }

  for(int i = 0; i < temp.size(); i++){// GOES THROUGH ALL THE DATA
    if(i % 4 == 0){//IF TEMP IS A SOCKET NUMEBER
      if(atoi(temp[i].c_str()) == tempfd){//CHECKS IF THE SOCKET MATCHES THE PARTNERS SOCKET
        USERNAME = temp[i + 1];//GETS THE USERNAME
        tempName = temp[i + 3];//GETS THE PARTNERS NAME

        for(int j = 7; j < tempName.size(); j++){//THE FIRST 7 CHARACTER ARE FRO "PENDING"
                                                 //SO IT GETS ALL CHARS AFTER THAT
          change += tempName[j];
	}

        temp[i + 3] = change;//THE FLAG IS THEN SET TO THE USERNAME
        break;
      }
    }
  }
  for(int i = 0; i < temp.size(); i++){// GOES THROUGH THE LOOP AGAIN TO CHANGE THE CURRENT SER FLAG
    if(i % 4 == 1){

      if(change == temp[i]){//IF THE USERNAME IS THE FLAG
        chatSock = atoi(temp[i - 1].c_str());//GET THAT SOCKET NUMBER

        temp[i + 2] = USERNAME;// SET THE FLAG TO THE USERNAME OF THE CURRENT USER
        break;
      }
    }
  }

  inFile.close();// CLOSE THE FILE
  outFile.open("address.txt");// OPEN THE OUPTU FILE

  for(int i = 0; i < temp.size(); i++){// ENTERS ALL DATA INTO THE FILE
    outFile<<temp[i];
    outFile<<" ";
    if(i % 4 == 3)
      outFile<<"\n";
  }

  outFile.close();// CLOSE THE OUTPUT FILE
  return chatting(chatSock, tempfd);// STARTS THE CHAT
}
/*************************************************************************
PURPOSE: CHECKS IF THE CHAT WAS ENDED BY ONE OF THE USERS, RETURN A BOOL
*************************************************************************/
//////////////////////// CHECK IF THE CHAT HAS ENDED /////////////////////
bool checkIfChatEnded(int thisSock, int tempSock){
  ifstream inFile;// OPEN THE TXT FILE
  string userInfo;// THE TEMPORARY USER INFO(HOLDER)
  bool thisSockCheck = false, tempSockCheck = false;//IF THE USERS LEFT THE CHAT
  bool checkIfExists1 = false, checkIfExists2 = false;// IF THEY TERMINATED THE PROGRAM

  inFile.open("address.txt");// OPEN THE FILE

  while(inFile>>userInfo){// GO THROUGH THE DATA

    if(atoi(userInfo.c_str()) == thisSock){// IF THE SOCKET MATCHS ONE OF THE SOCKETS
      checkIfExists1 = true;//THE USER EXISTS

      for(int i = 0; i < 3 ; i++)//GO TO THE FLAG
	inFile>>userInfo;

      if(userInfo == "not_chatting")//IF THE USER HAS EXITED THE CHAT
	thisSockCheck = true;
    }
    else if(atoi(userInfo.c_str()) == tempSock){//IF THE SOCKET MATCHS A USER SOCKET
      checkIfExists2 = true;// THEN THE OTHER USER EXISTS AND DID NOT TERMINATE

      for(int i = 0; i < 3 ; i++)// GOES TO THE FLAG
        inFile>>userInfo;

      if(userInfo == "not_chatting")// CHECKS THE FLAG TO SEE IF THE USER EXITD THE CHAT
        tempSockCheck = true;
    }
    else{
      for(int i = 0; i < 3 ; i++)// GOES TO THE NEXT USER
        inFile>>userInfo;
    }
  }

  if((thisSockCheck == true) || (tempSockCheck == true) ||
     (checkIfExists1 == false) || (checkIfExists2 == false))// IF  THE CHAT HAS ENDED
    return true;
  else
    return false;// THE CHAT IS STILL GOING
}
/**************************************************************************
PURPOSE: THIS IS THE CHATTING FUNCTION FOR THE USERS TO CHAT WITH ONE ANOTHER
         WILL ASK THE USER TO TYPE A "-1" TO END THE CHAT
***************************************************************************/
//////////////////////////// CHATTING //////////////////////////////////////
int chatting(int thisSock, int tempfd){
  const char nowChatting[255] = "\n\n=======================\nYOU ARE NOW CHATTING!!!\n";// YOU ARE NOW CHATTING PROMPT
  const char menu[512] = "\n==== MENU ====\n1. List users\n2. Chat\n3. Exit\nEnter your choice: \n";// MENU FOR EARLY TERMINATION
  const char exit[255] = "\nAT ANY TIME TYPE -1 TO QUIT THE CHAT!!!\n=======================================\n";//USER PROMPT
  const char ended[255] = "\nThe chat has ended!!!\n%";// THE USER PROMPT THAT THE CHAT HAS ENDED
  int str_len; // THE STRING LENGTH OF THE MESSAGE

  send(tempfd, nowChatting, strlen(nowChatting), 0);// SENDS THE CHAT HAS STARTED
  send(tempfd, exit, strlen(exit), 0);// INFORMS THE USER TO ENTER -1 TO QUIT
  
  for( ; ; ){
    
    bzero(payload, 255);// RESET THE PAYLOAD
      while(1){
	if(recv(tempfd, payload, len, 0) == 0){// RESEIVE THE MESSAGE FROM THE USERS
	  send(thisSock, ended, strlen(ended), 0);//IF TERMNATE THEN SHOW LEAVE MESSAGE
          send(thisSock, menu, strlen(menu), 0);//SHOW THE MAIN MENU FOR THE USER
	  changeToNotChatting(tempfd);// CHANGE THE FLAGS
	  changeToNotChatting(thisSock);// CHANGE THE FLAGS
      	  quit(tempfd);// TERMINATE THE THREAD OF THE USER
	}

	if(checkIfChatEnded(tempfd, thisSock) == true)// IF THE CHAT HAS ENDED
	  return payload[0] - '0';// CONVERT THE PAYLOAD TO AN INT

	char message[255] = "\nFrom User: ";//REINITIALIZES THE MESSAGE

	str_len = strlen(message);      //GETS THE MESSAGE LENGTH
	for(int i = str_len; i < strlen(payload) + str_len; i++)// ADDS THE PAYLOAD TO THE END OF THE STRING MESSAGE
	  message[i] = payload[i - str_len];
      
	message[strlen(message)] = '\n';//ADDS A NEW LINE TO HE END OF THE MESSAGE

	if(send(thisSock, message, strlen(message), 0) < 1){// SEND THE MESSAGE TO THE OTHER USER
        cout<<"error with send"<<endl;
      }
      
	if((payload[0] == '-') && (payload[1] == '1') && (strlen(payload) < 3)){//IF A NEGATIVE ONE WAS ENTERED
	  send(tempfd, ended, strlen(ended), 0);// SEND THE END MESSAGE
     	send(thisSock, ended, strlen(ended), 0);
	send(thisSock, menu, strlen(menu), 0);// SEND THE MAIN MENU TO THE OTHER USER
	changeToNotChatting(thisSock);//CHANGE THE FLAGS
	changeToNotChatting(tempfd);
	return -1;
      }
	bzero(payload, 255);// RESET THE PAYLOAD
	bzero(message, 255);// RESET THE MESSAGE
      }
  }
}
/*************************************************************************
PURPOSE: TO CHECK IF THE CURRENT USERS FLAG IS SET TO PENDING, MEANING
         THERE IS AN INVITE
*************************************************************************/
/////////////////////// CHECK IF PENDING /////////////////////////////////
int checkIfPending(int tempfd){
  ifstream inFile;// OPENS THE FILE FOR THE DATA
  inFile.open("address.txt");
  string usernames;//THE TEMPORARY USERNAME(HOLDERS)
  string chatter;// THE CHATTING USER
  char decline[255] = "Declined the invite";// USER PROMPT FOR DECILENED MESSAGE
  char accept[255] = "Accepted the invite";// USER PROMPT FOR SUCCESS
  char request[255] ="\nsomeone wants to chat with you Accept? (y/n) : \n";//SOMEONE WANTS TO CHAT
  int tempSock;// TEMPORARY SOCKET(HOLDER)
  char choice;//THE CHOICE OF THE USER

  while(inFile>>usernames){//GETS THE SOCKET NUMEBER

    tempSock = atoi(usernames.c_str());// CONVERTS THE SOCKET

    if(tempSock == tempfd){// IF THE SOCKET MATCHES

      for(int i = 0; i < 3; i++)//SKIPS TO THE FLAG
        inFile>>usernames;

      if(usernames != "not_chatting"){//IF THE FLAG IS POSITIVE
        for(int i = 7; i < usernames.size(); i++)//GET THE USERNAME OF THE USER
          chatter += usernames[i];

        send(tempfd, request, strlen(request), 0);//SEND THE REQUEST TO THE USER

	if(recv(tempfd, payload, len, 0) == 0)// RECEIVE THE ANSWER
	  quit(tempfd);

        choice = payload[0];// CONVERTS THE MESSAGE

        if((choice == 'y') || (choice == 'Y')){
          send(tempfd, accept, strlen(accept), 0);// Accepted the message
          return changeToChatting(tempfd);
        }
        else{
          send(tempfd, decline, strlen(decline), 0);//declined the message
          changeToNotChatting(tempfd);
          return -1;
        }
      }
    }

    for(int i = 0; i < 3; i++)// SKIPS TO THE NEXT SOCKET
      inFile>>usernames;
  }
  return -1;
}
/*******************************************************************************
PURPOSE: CHANGES THE USERS FLAG TO NOT CHATTING
*******************************************************************************/
///////////////////// CHANGE TO NOT CHATTING ///////////////////////////////////
void changeToNotChatting(int tempfd){
  ifstream inFile;//GETS THE INFO
  ofstream outFile;//OUTPUTS THE NEW DATA
  string tempusers;// HOLDER FOR THE USERS
  vector<string> temp;// TH DATA FROM THE FILE

  inFile.open("address.txt");// OPENS THE FILE

  while(inFile>>tempusers){// GETS ALL DATA
    temp.push_back(tempusers);
  }

  for(int i = 0; i < temp.size(); i++){//GOES THROUGH THE DATA
    if(i % 4 == 0){
      if(atoi(temp[i].c_str()) == tempfd){// IF THE SOCKET MATCHES THEN CHANGE TO NOT CHATTING
        temp[i + 3] = "not_chatting";
        break;
      }
    }
  }

  inFile.close();// CLOSE THE FILE
  outFile.open("address.txt");//OPENS THE OUT FILE

  for(int i = 0; i < temp.size(); i++){//OUTPUTS ALL THE DATA TO THE FILE
    outFile<<temp[i];
    outFile<<" ";
    if(i % 4 == 3)
      outFile<<"\n";
  }
  outFile.close();// CLOSES THE OUTFILE
}
/***********************************************************************
PURPOSE: CHANGES A USER FLAG TO PENDING + THE USERNAME WHO WISHES TO
         CHAT
***********************************************************************/
////////////////////// CHANGE TO PENDING ///////////////////////////////
void changeToPending(string username, string user){
  ifstream inFile;// THE INPUT FILE
  ofstream outFile;// THE OUTPUT FILE
  string tempusers;// THE HOLDER FOR THE USERS
  vector<string> temp;// TEH VECTOR FRO HOLDING ALL DATA

  inFile.open("address.txt");// OPENS THE TXT FILE

  while(inFile>>tempusers){// GETS ALL THE DATA
    temp.push_back(tempusers);
  }

  for(int i = 0; i < temp.size(); i++){// GOES THROUGH THE DATA
    if(temp[i] == username){// IF THE USERNAME MATCHES
      temp[i + 2] = "pending" + user;// CHANGE TO PENDING + THE USERNAME
      break;
    }
  }

  inFile.close();// CLOSE THE FILE
  outFile.open("address.txt");// OPEN THE OUTPUT FILE

  for(int i = 0; i < temp.size(); i++){// INPUT THE NEW DATA INTO THE FILE
    outFile<<temp[i];
    outFile<<" ";

    if(i % 4 == 3)
      outFile<<"\n";
  }

  outFile.close();// CLOSE THE OUTPUT FILE
}
/****************************************************************************
PURPOSE: FINDS THE SOCKET MATCHING THE USERNAME
****************************************************************************/
/////////////////////////// FIND SOCKET /////////////////////////////////////
int findSocket(string username){
  ifstream inFile;// THE IN FILE
  inFile.open("address.txt");// OPENS THE FILE
  string temp;// THE TEMPORARY USERS(HOLDER)
  int socket;// THE SOCKET MATCHING THE USERNAME

  while(inFile>>socket){// GO THROUGH THE FILE
    inFile>>temp;

    if(temp == username)// IF IT MATCHES
      return socket;

    for(int i = 0; i < 2; i++)// GO TO THE NEXT SOCKET
      inFile>>temp;
  }
}
/****************************************************************************
PURPOSE: LIST ALL OF THE USERS AND RETURNS THEM TO THE USER
****************************************************************************/
void listUsers(int tempfd, bool checkSend){
  ifstream inFile;// THE IN FILE
  string usernames = "";// THE TEMP USERS(HOLDER)
  char noUsers[512] = "There are no users online...\n";//USER PROMPT
  char users[512] = "Usernames\n---------\n";//USER PROMPT
  string tempUser;// THE TEMPORARY USERS(HOLDER)
  int str_len;// THE STRING LENGTH
  inFile.open("address.txt");// OPENS THE FILE
  
  while(inFile>>tempUser){// GETS THE USERNAME
    inFile>>tempUser;
    usernames+= tempUser + '%';//ADDS A % TO THE END OF A USERNAME
    
    for(int i = 0; i < 2 ; i++){// GO TO THE NEXT USERNAME
      inFile>>tempUser;
    }
  }
  str_len = strlen(users);// THE STRING LENGTH OF ALL THE USERS STRING
  
  usernames += "\n";// ADDS A NEWLINE AFTER
  
  if(usernames.size() < 2)// IF THERE ARE NO USERS
    send(tempfd, noUsers, strlen(noUsers) + 1, 0);
  else{
    for(int i = str_len; i < usernames.size() + str_len; i++)
      users[i] = usernames[i - str_len];
    
    send(tempfd, users, strlen(users) + 1, 0);// SENDS THE USERS TO THE CLIENT
  }
  inFile.close();// CLOSE THE INPUT FILE
  
  return;
}
//==================================================================================
//===================== find chat ==================================================
//==================================================================================
/***********************************************************************************
PURPOSE: LIST ALL THE NOT CHATTING CLIENTS AND RETURNS THEM TO THE USER TO PICK FROM
         AFTER PICKING ONE THIS FUNCTION WILL CHECK IF THAT USER EXISTS, AND THEN
         IT WILL SEND AN INVITE TO THE USER
***********************************************************************************/
/////////////////////////////// FIND CHAT /////////////////////////////////////////
int findChat(int tempfd){
  char available[512] = "\nThe users are: \n";// THE AVAILABLE USERS
  char chatting[512] = "You are now chatting with: ";// YOU ARE NOW CHATTING WITH PROMPT
  char typeNew[512] = "\nThat username is invalid, choose a different one!!!\n%";//INVALID USERNAME ENTERED
  char enterUserName[512] = "Please enter a username to connect to: ";// ENTER A USERNAME PROMPT
  char noUsers[512] = "\nThere are no users available to chat with\n%";// NO AVAILABLE USERS PROMPT
  char exists[512] = "That user exists, Trying to connect....\n";// USER EXISTS
  string select = "\nplease select a user: \n";// PLEASE SELECT A USERNAME

  string availableUsers;// ALL AVAILABLE USERNAMES
  string nextUser;// THE NEXT USERNAME(HOLDER)
  string checkAval;// TEMP CHECK IF THE USER ID CHATTING(HOLDER)
  int size = 0;
  string pending = "";//CHECKS IF USERNAME IS PENDING
  ifstream inFile;// THE IN FILE
  string username, tempUser;// THE HOLDERS FOR THE USERNAMES
  string chosenUser;// TEH CHOSEN USER ENTERED BYT HE USER
  bool findUser = false;// IF THE USER EXISTS
  int userSock;// THE SOCKET OF THE USER SELECTED USERNAME
  inFile.open("address.txt");// OPEN THE FILE

  while(inFile>>tempUser){// GO THROUG THE DATA
    userSock = atoi(tempUser.c_str());// GET THE FIRST SOCKET
    inFile>>tempUser;// GET THE USERNAME OF THAT SOCKET
    
    for(int i = 0; i < 2; i++){
      inFile>>username;
    }
    
    if((username == "not_chatting") && (userSock != tempfd))// IF TEH USERNAME IS PENDING OR CHATTING
      availableUsers += tempUser + "%\n";
  }
  
  inFile.close();
  
  size = strlen(available);// THE SIZE OF THE STRING LENGTH
  
  if(availableUsers.size() == 0){
    send(tempfd, noUsers, strlen(noUsers), 0);// THERE ARE NO AVAILABLE USERS
    return -1;
  }
  
  for(int i = size; i < size + availableUsers.size(); i++)// CONVERTS TO A CHAR ARRAY
    available[i] = availableUsers[i - size];
  
  //==================================================================
  // receives the username from the client to chat with, has to check if that user exists
  size  = strlen(available);// GETS THE STRING SIZE
  
  for(int i = size; i < size + select.size(); i++){// CONVERTS TO A CHAR ARRAY
    available[i] =  select[i - size];
  }
  
  while(findUser == false){// IF THE USER DOESNT EXIST
    send(tempfd, available, strlen(available) + 1, 0);// sends the available users to the client
    bzero(payload, 512);// RESETS THE PAYLOAD

    if(recv(tempfd, payload, len, 0) == 0) // receives the username selected
      quit(tempfd);

    chosenUser = payload;// TEH CHOSEN USER BY THE CLIENT
    
    for(int i = 0; i < availableUsers.size(); i++){// checks if that user exists
      if(availableUsers[i] == '%'){// ID THERE IS A % THEN SKIP IT
	i++;
	if(nextUser == chosenUser){// IF THE USER IS FOUND
	  findUser = true;
	  break;
	}
	nextUser = "";
      }
      else
	nextUser += availableUsers[i];
    }
    
    if(findUser == false)// IF THE USER DOESNT EXISTS
      send(tempfd, typeNew, strlen(typeNew), 0);// asks them to type the name again
  }
  
  send(tempfd, exists, strlen(exists), 0);// THAT USER EXISTS TRYING TO CONNECT TO THE THEM
  //==================================================================
  // Send the invitation to the client the user wants to chat with
  
  userSock = findSocket(nextUser);// FINDS THE SOCKET NUMBER OF THAT USER
  changeToPending(chosenUser, USERNAME);// SEND THE INVITE

  return waitForResponse(tempfd, userSock);// WAIT ROOM FOR WAITING FOR THE USER TO RESPOND
}

//===================================================================
//===================== printing ====================================
//===================================================================
/********************************************************************
PURPOSE: PRINTS ALL MESSAGES
********************************************************************/
///////////////////////// PRINT /////////////////////////////////////
void print(char payload[256]){
  cout<<"\nMessage: ";// PRINTS THE USER PAYLOAD
  for(int i = 0; i < strlen(payload); i++){
    if(payload[i] == '%')
      cout<<" ";
    else
      cout<<payload[i];
  }
}
