#include <iostream>
#include <math.h>
#include <cstdlib>
#include <cstdio>
#include <sys/socket.h> // socket(), bind(), connect(), listen()
#include <unistd.h> // close(), read(), write()
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h> // htons(), htonl()
#include <strings.h> // bzero()
#include <string.h>
#include <vector>
#include <algorithm>

#define PORT_NUM 3998
#define TIMEOUT 1
#define TIMEOUT_RECHARGING 5
#define BUFFER_SIZE 1024

#define LOGIN "100 LOGIN\r\n"
#define LOGIN_S 11
#define PASSWORD "101 PASSWORD\r\n"
#define PASSWORD_S 14
#define MOVE "102 MOVE\r\n"
#define MOVE_S 10
#define TURN_L "103 TURN LEFT\r\n"
#define TURN_L_S 15
#define TURN_R "104 TURN RIGHT\r\n"
#define TURN_R_S 16
#define GET_M "105 GET MESSAGE\r\n"
#define GET_M_S 17
#define OK "200 OK\r\n"
#define OK_S 8
#define LOGIN_FAIL "300 LOGIN FAILED\r\n"
#define LOGIN_FAIL_S 18
#define SYNTAX_ERR "301 SYNTAX ERROR\r\n"
#define SYNTAX_ERR_S 18
#define LOGIC_ERR "302 LOGIC ERROR\r\n"
#define LOGIC_ERR_S 17

#define CLIENT_RECHARGE "RECHARGING\r\n"
#define CLIENT_FULL "FULL POWER\r\n"

#define USER_LENGTH 100
#define PASSWORD_LENGTH 7
#define OK_LENGTH 12
#define MESSAGE_LENGTH 100
#define FULL_POWER_LENGTH 12

using namespace std;

bool recvCorrectData(int, vector<char> *, int, unsigned int);
bool retrieveNext(char *, vector<char> *, int);

int findRN(char * buf, int len)
{
    for(int i = 0; i < len - 1; i ++)
    {
        if(buf[i] == '\r' && buf[i+1] == '\n') return i+2;
    }
    return -1;
}

int findRN(vector<char> * bigBuffer)
{
    for(unsigned int i = 0; i < bigBuffer->size() - 1; i ++)
    {
        if(bigBuffer->at(i) == '\r' && bigBuffer->at(i+1) == '\n') return i+1;
    }
    return -1;
}

bool checkDataValidity(char * response)
{
    int len = strlen(response);
    if(len > 100 || (findRN(response, len) == -1)) return false;
    return true;
}

bool isMsg(char * buf, const char * pattern)
{
    if(strcmp(pattern, buf)==0) return true;
    return false;
}


bool handleRecharge(int c, vector<char> * bigBuffer)
{
    char buf[BUFFER_SIZE];
    cout << " Robot is recharging... ";
    if(!recvCorrectData(c, bigBuffer, TIMEOUT_RECHARGING, FULL_POWER_LENGTH)) return false; // Expecting Client full power msg
    if(!retrieveNext(buf, bigBuffer, c)) return false;
    if(!isMsg(buf, CLIENT_FULL))
    {
        send(c, LOGIC_ERR, LOGIC_ERR_S, 0);
        return false;
    }

    cout << "Recharging finished!" << endl;

    if(!recvCorrectData(c, bigBuffer, TIMEOUT, 100)) return false;

    return true;
}


bool retrieveNext(char * buf, vector<char> * bigBuffer, int c)
{
    int len = findRN(bigBuffer);

    for(int i = 0; i <= len; i++)
    {
        buf[i] = bigBuffer->at(i);
    }
    buf[len+1] = '\0';
    bigBuffer->erase(bigBuffer->begin(), bigBuffer->begin() + findRN(bigBuffer)+1 );

    if(isMsg(buf, CLIENT_RECHARGE))
    {
        if(!handleRecharge(c, bigBuffer)) return false;
        if(!retrieveNext(buf, bigBuffer, c)) return false;
    }
    return true;
}

bool receive(int c, vector<char> * bigBuffer, int timeout)
{
    fd_set fds;
    int n;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(c, &fds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    n = select(c+1, &fds, NULL, NULL, &tv);
    if (n == 0 || n == -1) return false;

    char buffer[BUFFER_SIZE];
    int bytesRead = recv(c, buffer, BUFFER_SIZE, 0);
    for(int i = 0; i<bytesRead; i++)
        bigBuffer->push_back(buffer[i]);

    return true;
}


bool recvCorrectData(int c, vector<char> * bigBuffer, int timeout, unsigned int maxSize)
{
    const char * pattern = "\r\n";

    while((search(bigBuffer->begin(), bigBuffer->end(), pattern, pattern+2)) == bigBuffer->end())
    {
        if((bigBuffer->size()>=maxSize))
        {
            send(c, SYNTAX_ERR, SYNTAX_ERR_S, 0);
            return false;
        }
        if(!receive(c, bigBuffer, timeout)) return false;
    }
    return true;
}

bool checkPasswd(char * userName, char * passwd, int c) // Checks if password is correct
{

    if(!checkDataValidity(passwd))
    {
        send(c, SYNTAX_ERR, SYNTAX_ERR_S, 0);
        return false;

    }
    char strSum[BUFFER_SIZE];
    int sum = 0;
    int i=0;
    while(userName[i]!='\r')
    {
        sum += (int) userName[i];
        i++;
    }

    sprintf(strSum, "%d\r\n", sum);

    i=0;
    while(passwd[i]!='\0')
    {
        if(strSum[i]!=passwd[i])
        {
            send(c, LOGIN_FAIL, LOGIN_FAIL_S, 0);
            return false;
        }
        i++;
    }

    return true;
}

bool turnRobot(char dir, pair<int, int> first, pair<int, int> second, int c, vector<char> * bigBuffer)
{
    char buffer_loc[BUFFER_SIZE];

    switch(dir)
    {
        case 'L':
            if(first.first > second.first) break; // Robot is facing LEFT
            if(first.first < second.first) // Robot is facing RIGHT
            {
                for(int i=0; i<2; i++)
                {
                    send(c, TURN_L, TURN_L_S, 0);
                    if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
                    if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
                }
                break;
            }
            if(first.second < second.second) // Robot is facing UP
            {
                send(c, TURN_L, TURN_L_S, 0);
                if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
                if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
                break;
            }
            else // Robot is facing DOWN
            {
                send(c, TURN_R, TURN_R_S, 0);
                if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
                if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
            }

            break;

        case 'R':
            if(first.first > second.first) // Robot is facing LEFT
            {
                for(int i=0; i<2; i++)
                {
                    send(c, TURN_L, TURN_L_S, 0);
                    if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
                    if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
                }
                break;
            }
            if(first.first < second.first) break; // Robot is facing RIGHT

            if(first.second < second.second) // Robot is facing UP
            {
                send(c, TURN_R, TURN_R_S, 0);
                if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
                if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
                break;
            }
            else // Robot is facing DOWN
            {
                send(c, TURN_L, TURN_L_S, 0);
                if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
                if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
            }

            break;

        case 'U':
            if(first.first > second.first) // Robot is facing LEFT
            {
                send(c, TURN_R, TURN_R_S, 0);
                if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
                if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
                break;
            }
            if(first.first < second.first) // Robot is facing RIGHT
            {
                send(c, TURN_L, TURN_L_S, 0);
                if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
                if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
                break;
            }

            if(first.second < second.second) break; // Robot is facing UP

            else // Robot is facing DOWN
            {
                for(int i=0; i<2; i++)
                {
                    send(c, TURN_L, TURN_L_S, 0);
                    if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
                    if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
                }
            }
            break;

        case 'D':
            if(first.first > second.first) // Robot is facing LEFT
            {
                send(c, TURN_L, TURN_L_S, 0);
                if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
                if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
            }
            if(first.first < second.first)  // Robot is facing RIGHT
            {
                send(c, TURN_R, TURN_R_S, 0);
                if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
                if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
            }

            if(first.second < second.second) // Robot is facing UP
            {
                for(int i=0; i<2; i++)
                {
                    send(c, TURN_L, TURN_L_S, 0);
                    if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
                    if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
                }
            }

            break;
    }
    return true;
}

pair<int, int> getLoc(char * response, bool * status, int c)
{
    pair<int, int> ret;
    ret.first = 0;
    ret.second = 0;
    char dummy;

    if(response[strlen(response)-1]!='\n' || response[strlen(response)-2]!='\r')
    {
        *status = false;
        send(c, SYNTAX_ERR, SYNTAX_ERR_S, 0);
        return ret;
    }
    response[strlen(response)-2] = '\0';

    if(sscanf(response, "OK %d %d%c", &ret.first, &ret.second, &dummy)!=2)
    {
        *status = false;
        send(c, SYNTAX_ERR, SYNTAX_ERR_S, 0);
        return ret;
    }
    *status = true;
    return ret;
}

bool authentificate(int c, vector<char> * bigBuffer)
{
    cout << "Authentificating... ";
    send(c, LOGIN, LOGIN_S, 0); // Sending login packet
    char buffer_user[BUFFER_SIZE];
    if(!recvCorrectData(c, bigBuffer, TIMEOUT, USER_LENGTH)) return false;
    if(!retrieveNext(buffer_user, bigBuffer, c)) return false;
    if(!checkDataValidity(buffer_user)) // Checking message validity
    {
        send(c, SYNTAX_ERR, SYNTAX_ERR_S, 0);
        return false;
    }

    send(c, PASSWORD, PASSWORD_S, 0); // Sending password request
    char buffer_passwd[BUFFER_SIZE];
    if(!recvCorrectData(c, bigBuffer, TIMEOUT, PASSWORD_LENGTH)) return false;
    if(!retrieveNext(buffer_passwd, bigBuffer, c)) return false;

    if(!checkPasswd(buffer_user, buffer_passwd, c))
    {
        cout << "Authentification failed!" << endl;
        return false;
    }
    else
    {
        send(c, OK, OK_S, 0);
        cout << "Authentification successfull!" << endl;
        return true;
    }
}

bool getMessage(int c, vector<char> * bigBuffer)
{
    char buffer[BUFFER_SIZE];
    send(c, GET_M, GET_M_S, 0);
    if(!recvCorrectData(c, bigBuffer, TIMEOUT, MESSAGE_LENGTH)) return false;
    if(!retrieveNext(buffer, bigBuffer, c)) return false;
    if(!checkDataValidity(buffer))
    {
        send(c, SYNTAX_ERR, SYNTAX_ERR_S, 0);
        return false;
    }
    send(c, OK, OK_S, 0);

    cout << "Message: " << buffer << endl;

    return true;
}

bool navigateToXZero(int c, vector<char> * bigBuffer)
{
    pair<int, int> loc;
    char buf[BUFFER_SIZE];
    bool stat;
    do
    {
        send(c, MOVE, MOVE_S, 0);
        if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
        if(!retrieveNext(buf, bigBuffer, c)) return false;
        loc = getLoc(buf, &stat, c);
    }
    while(loc.first!=0);
    return true;
}

bool navigateToYZero(int c, vector<char> * bigBuffer)
{
    pair<int, int> loc;
    char buf[BUFFER_SIZE];
    bool stat;
    do
    {
        send(c, MOVE, MOVE_S, 0);
        if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
        if(!retrieveNext(buf, bigBuffer, c)) return false;
        loc = getLoc(buf, &stat, c);
    }
    while(loc.second!=0);
    return true;
}

bool navigate(int c, vector<char> * bigBuffer)
{
    bool stat;
    pair<int, int> firstLoc, secondLoc;
    char buffer_loc[BUFFER_SIZE];

    cout << "Starting navigation... ";
/// --- Start of first move ---
    send(c, MOVE, MOVE_S, 0);
    if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
    if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
    firstLoc = getLoc(buffer_loc, &stat, c);
    if(!stat)
    {
        return false;
    }
/// --- End of first move ---

    if(firstLoc.first==0 && firstLoc.second==0) // Check if we are at [0,0]
    {
        cout << "Navigation finished!" << endl;
        if(getMessage(c, bigBuffer)) return true;
        return false;
    }

    do
    {
/// --- Start of second move ---
        send(c, MOVE, MOVE_S, 0);
        if(!recvCorrectData(c, bigBuffer, TIMEOUT, OK_LENGTH)) return false;
        if(!retrieveNext(buffer_loc, bigBuffer, c)) return false;
        secondLoc = getLoc(buffer_loc, &stat, c);
        if(!stat)
        {
            return false;
        }
    } while ((firstLoc.first == secondLoc.first) && (firstLoc.second == secondLoc.second));
/// --- End of second move ---
/// - now we are sure, that firstLoc and secondLoc are different

    pair<int, int> temp1, temp2; // Need these to pass current orientation to navigateToYZero()
    temp1.first = 0;
    temp1.second = 0;
    temp2.first = 0;
    temp2.second = 0;

    if(secondLoc.first < 0)
    {
        temp2.first = 1;
        if(!turnRobot('R', firstLoc, secondLoc, c, bigBuffer)) return false;
    }
    else
    {
        temp2.first = -1;
        if(!turnRobot('L', firstLoc, secondLoc, c,bigBuffer)) return false;
    }
    if(!navigateToXZero(c, bigBuffer)) return false;

    if(secondLoc.second < 0)
    {
        if(!turnRobot('U', temp1, temp2, c, bigBuffer)) return false;
    }
    else
    {
        if(!turnRobot('D', temp1, temp2, c, bigBuffer)) return false;
    }
    if(!navigateToYZero(c, bigBuffer)) return false;

    cout << "Navigation finished!" << endl;

    if(!getMessage(c, bigBuffer))
        return false;
    return true;
}


int main(void)
{
	// Vytvoreni koncoveho bodu spojeni
	int l = socket(AF_INET, SOCK_STREAM, 0);
	if(l < 0){
		perror("Nemohu vytvorit socket!");
		return -1;
	}

	int port = PORT_NUM;

	struct sockaddr_in adresa;
	bzero(&adresa, sizeof(adresa));
	adresa.sin_family = AF_INET;
	adresa.sin_port = htons(port);
	adresa.sin_addr.s_addr = htonl(INADDR_ANY);


    int yes = 1;
    if (setsockopt(l,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }

	if(bind(l, (struct sockaddr *) &adresa, sizeof(adresa)) < 0){
		perror("Problem s bind()!");
		close(l);
		return -1;
	}

	// Oznacim socket jako pasivni
	if(listen(l, 10) < 0){
		perror("Problem s listen()!");
		close(l);
		return -1;
	}

	struct sockaddr_in vzdalena_adresa;
	socklen_t velikost = 0;

	while(true)
	{
		// Cekam na prichozi spojeni
		int c = accept(l, (struct sockaddr *) &vzdalena_adresa, &velikost);
		if(c < 0)
		{
			perror("Problem s accept()!");
			close(l);
			return -1;
		}
		else
		{
			pid_t pid = fork();
			if(pid == 0)
			{
				fd_set sockets;
                FD_ZERO(&sockets);
                FD_SET(c, &sockets);
                vector<char> bigBuffer;
                cout << endl;

                if(!authentificate(c, &bigBuffer))
                    break;

                if(!navigate(c, &bigBuffer))
                    break;



			}
			close(c);
		}
	}

	close(l);

    return 0;
}
