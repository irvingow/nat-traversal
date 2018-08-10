#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include "natpunch.h"

using namespace std;

//const string server_ip = "45.78.62.188";

//#define DEBUG
#undef DEBUG

/*
 * 完成初始化工作
 *
 * */
void Natpunch::initialWork()
{
	//初始工作,清空错误消息
	errorMsg.clear();
	//默认情况下是没有得到自己的信息
	usedThreadNum = 0;
	selfInfoget = false;
	ifTimeout = false;
}

/*
 * 创建新的线程接收来自Stun服务器的消息
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
int Natpunch::clientConnect()
{
	pthread_t thread;
	int ret = pthread_create(&thread,NULL,threadRecvStunServerData,this);
	if(ret != 0)
	{
		errorMsg = "create thread failed";
		return -1;
	}
	return 0;
}

/*
 * 发送消息给信令服务器
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
int Natpunch::sendMsgToSigalServer()
{
	struct sockaddr_in clientaddr;
	return establishConnectionToHeartServer(sockfd,clientaddr);
}
/*
 * 创建一个新的线程从信令服务器阻塞接收消息,超时时间为30s
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
int Natpunch::RecvMsgFromSigalServer()
{
	//默认情况下是没有得到远程客户端的信息的
	remoteClientInfoGet = false;
	ifTimeout = false;
	//创建一个新的线程来接收来自signal服务器的数据
	pthread_t thread;
	setTimeout(100);
	int ret = pthread_create(&thread,NULL,threadRecvSignalServerData,this);
	if(ret != 0)
	{
		errorMsg = "create thread failed";
		return -1;
	}
	return 0;
}

//这个函数不再使用,功能被取代
void Natpunch::communication(int sockfd, struct sockaddr* addr, socklen_t *len)
{
	char buf[BUFSIZE];
	while(1)
	{
		bzero(buf,sizeof(buf));
		cout<<">> ";
		fflush(stdout);
		fgets(buf,sizeof(buf)-1,stdin);
		sendto(sockfd,buf,strlen(buf),0,addr,sizeof(struct sockaddr_in));
		if(strcmp(buf,"exit") == 0)
			break;
		bzero(buf,sizeof(buf));
		recvfrom(sockfd,buf,sizeof(buf)-1,0,addr,len);
		cout<<buf<<endl;
	}
}

/*
 * 开始尝试打洞,向打洞另一客户端发送心跳包的一个字节的数据
 *
 * @return 正确返回1,异常情况返回-1,并设置错误消息
 * */
int Natpunch::startNatPunch()
{
	struct sockaddr_in clientaddr;
	memset(&clientaddr,0,sizeof(clientaddr));
	clientaddr.sin_addr = remoteClientInfo.ip;
	clientaddr.sin_port = remoteClientInfo.port;
	clientaddr.sin_family = AF_INET;

	char ch;
	bzero(&ch,1);
	int n = sendto(sockfd, &ch, sizeof(ch), 0, (struct sockaddr *)&clientaddr, sizeof(struct sockaddr_in));
	if(n < 0)
	{
		errorMsg = "send heart beat data to remote client failed!";
		return n;
	}
	setTimeout(3);//设置超时时间为3s
#ifdef DEBUG
	cout<<"send heart beat IP: "<<inet_ntoa(clientaddr.sin_addr)<<"\t"<<ntohs(clientaddr.sin_port)<<endl;
#endif
	return n;
}

/*
 * 打洞成功之后发送数据 
 * @param data 用户希望发送的数据
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
int Natpunch::sendData(const string& data)
{
	const char *datas = data.c_str();

	static struct sockaddr_in clientaddr;
	memset(&clientaddr,0,sizeof(clientaddr));
	clientaddr.sin_addr = remoteClientInfo.ip;
	clientaddr.sin_port = remoteClientInfo.port;
	clientaddr.sin_family = AF_INET;
#ifdef DEBUG
	cout<<"send data IP: "<<inet_ntoa(clientaddr.sin_addr)<<"\t"<<ntohs(clientaddr.sin_port)<<endl;
#endif
	int n = sendto(sockfd, datas, data.size(), 0, (struct sockaddr *)&clientaddr, sizeof(struct sockaddr_in));
	if(n < 0)
	{
		errorMsg = "send data to remote client Failed!";
		return n;
	}
	return n;
}

//todo need to set socket blocking
/*void* Natpunch::threadRecvData(void *args)
  {
  Natpunch* np = (Natpunch*)args;
  np->recvData();
  pthread_exit(NULL);

 *int sockfd = *((int*)args);
 *struct sockaddr_in addr;
 *memset(&addr,0,sizeof(addr));
 *socklen_t addrlen = sizeof(struct sockaddr_in);
 *char buf[BUFSIZE];
 *while(true)
 *{
 *    bzero(buf,sizeof(buf));
 *    recvfrom(sockfd,buf,sizeof(buf)-1,0,(struct sockaddr*)&addr,&addrlen);
 *    string recvData(buf);
 *    if(!recvData.empty())
 *    {
 *        if(recvData == "exit")
 *            break;
 *        Natpunch::callbackFunc(recvData);
 *    }
 *}*
 }*/


/*
 * 与Stun服务器通信的线程,调用Natpunch的成员函数 
 * @param args参数,用于类型转换之后调用类的成员函数完成功能
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
void* threadRecvStunServerData(void *args)
{
	Natpunch* np = (Natpunch*)args;
	np->connectToStunServer();
	pthread_exit(NULL);
}

/*
 * 与信令服务器通信的线程,调用Natpunch的成员函数 
 * @param args参数,用于类型转换之后调用类的成员函数完成功能
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
void *threadRecvSignalServerData(void *args)
{
	Natpunch* np = (Natpunch*)args;
	np->getRemoteClientInfo();
	pthread_exit(NULL);
}

/*
 * 接收来自另一客户端消息的线程,调用Natpunch的成员函数 
 * @param args参数,用于类型转换之后调用类的成员函数完成功能
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
void *threadRecvData(void *args)//使用一个新的线程来接收数据
{
	Natpunch* np = (Natpunch*)args;
	np->usedThreadNum += 1;
	np->recvData();
	pthread_exit(NULL);
}

/*
 * 创建接收来自远端客户端消息的线程, 最多可以创建recvThreadNum个,但若创建线程数量超过限制,
 * 设置错误信息但是仍然返回0,避免程序直接退出
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
int Natpunch::startRecvData()
{
	if(usedThreadNum >= 10)
	{
		errorMsg = "too much recvThread created";
		return 0;
	}
	ifTimeout = false;
	int ret = pthread_create(&recvThreadId[usedThreadNum],NULL,threadRecvData,this);
	if(ret != 0)
	{
		errorMsg = "create thread failed";
		return -1;
	}
	recvThread = true;
	return 0;
}


//注意buf的大小是BUFSIZE,所以一次能接收的数据是有限的,利用接收到的数据作为参数调用回调数据
/*
 * 接收来自远端客户端消息 
 *
 * @return 正确返回接收消息的字节数,异常情况返回-1,并设置错误消息
 * */
void Natpunch::recvData()
{
	struct sockaddr_in addr;
	memset(&addr,0,sizeof(addr));
	socklen_t addrlen = sizeof(struct sockaddr_in);
	char buf[BUFSIZE];
	int count = 0;
	while(1)
	{
		bzero(buf,sizeof(buf));
		recvfrom(sockfd,buf,sizeof(buf)-1,0,(struct sockaddr*)&addr,&addrlen);
		string recvData(buf);
		if(!recvData.empty())
		{
			if(recvData != "exit")
				callbackFunc(&recvData);
			else
				break;
		}
		//睡眠1s,免得无谓的空转浪费cpu时间
		sleep(1);
		++count;
		//每隔20s发送一次心跳包
		if(count == 30)
		{
			char heartBeat;
			bzero(&heartBeat,1);
			sendto(sockfd, &heartBeat, 1, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
			count = 0;
		}
	}
}

//不再使用
int Natpunch::initialConnect()
{
	/*struct sockaddr_in clientaddr;
	 *socklen_t addrlen = sizeof(struct sockaddr_in);*/
	int n = connectToStunServer();
	if(n < 0)
	{
		errorMsg = "connect to StunServer failed";
		return -1;
	}
	return 0;

	//这个函数调用放在sendMsgToSignalServer中了
	/*establishConnectionToHeartServer(sockfd, clientaddr);*/

	/*clientInfo remoteClientInfo = getRemoteClientInfo(sockfd);
	 *getSelfInfo(sockfd);*/

	//这个函数调用放在RecvMsgFromSignalServer中了
	/*getRemoteClientInfo(sockfd);*/
	/*clientaddr.sin_addr = remoteClientInfo.ip;
	 *clientaddr.sin_port = remoteClientInfo.port;
	 *clientaddr.sin_family = AF_INET;*/

	//向打洞另一客户端发送心跳包的一个字节的数据
	/*char ch = 'a';
	 *sendto(sockfd, &ch, sizeof(ch), 0, (struct sockaddr *)&clientaddr, sizeof(struct sockaddr_in));*/
	/*printf("send heart beat IP: %s\tPort: %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));*/
	/*#ifdef DEBUG
	 *    cout<<"send heart beat IP: "<<inet_ntoa(clientaddr.sin_addr)<<"\t"<<ntohs(clientaddr.sin_port)<<endl;
	 *#endif*/
	/*communication(sockfd, (struct sockaddr *)&clientaddr, &addrlen);*/
}

/*
 * 建立与信令服务器的连接,向发送数据包 
 * @param sockfd 与Stun服务器通信的套接字
 * @param clientaddr 储存服务器地址信息的结构体 
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
int Natpunch::establishConnectionToHeartServer(int sockfd,struct sockaddr_in& clientaddr)
{
	/*    int sockfd = socket(AF_INET,SOCK_DGRAM,0);
	 *    if(sockfd == -1)
	 *        err_exit("SOKCET");
	 *
	 *    struct sockaddr_in myaddr;
	 *    memset(&myaddr, 0, sizeof(myaddr));
	 *    myaddr.sin_port = htons(local_port);
	 *    myaddr.sin_addr.s_addr = inet_addr("0.0.0.0");
	 *    myaddr.sin_family = AF_INET;
	 *    if(bind(sockfd,(struct sockaddr*)(&myaddr),sizeof(myaddr)) == -1)
	 *        err_exit("BIND");
	 **/
	memset(&clientaddr, 0, sizeof(clientaddr));
	clientaddr.sin_port = htons(server_port);
	clientaddr.sin_addr.s_addr = inet_addr(server_ip.c_str());
	clientaddr.sin_family = AF_INET;

	/* 向服务器S发送数据包 */
#ifdef DEBUG
	cout<<"send information to heart server:"<<inet_ntoa(clientaddr.sin_addr)<<"\t"<<ntohs(clientaddr.sin_port)<<endl;
	cout<<"information:"<<inet_ntoa(selfInfo.ip)<<"\t"<<ntohs(selfInfo.port)<<endl;
#endif
	if(sendto(sockfd, &selfInfo, sizeof(selfInfo), 0, (struct sockaddr *)&clientaddr, sizeof(struct sockaddr_in)) < 0)
	{
		errorMsg = "send data to HeartServer failed";
		return -1;
	}
	return 0;
}

/*
 * 从信令服务器阻塞接收消息,获得远端客户端的信息,超时时间为30s
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
int Natpunch::getRemoteClientInfo()
{
	struct sockaddr_in remoteClientAddr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	/* 接收B的ip+port */
	bzero(&remoteClientInfo, sizeof(remoteClientInfo));
	int n = recvfrom(sockfd, &remoteClientInfo, sizeof(clientInfo), 0, (struct sockaddr *)&remoteClientAddr, &addrlen);
	remoteClientInfoGet = true;
	if(n < 0)
	{
		if(errno == EAGAIN || EWOULDBLOCK)
			ifTimeout = true;
		errorMsg = "recvfrom Signalserver Failed";
		return -1;
	}
	remoteClientInfoGet = true;
#ifdef DEBUG
	printf("remote client IP: %s\tPort: %d\n", inet_ntoa(remoteClientInfo.ip), ntohs(remoteClientInfo.port));
#endif
	return 0;
}

//不应该被使用的函数,之前测试所用
void Natpunch::getSelfInfo(int sockfd)
{
	struct sockaddr_in selfAddr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	clientInfo info;//用来储存ip地址和端口信息
	bzero(&info, sizeof(info));
	recvfrom(sockfd, &info, sizeof(clientInfo), 0, (struct sockaddr *)&selfAddr, &addrlen);
#ifdef DEBUG
	printf("My IP: %s\tPort: %d\n", inet_ntoa(info.ip), ntohs(info.port));
#endif
}



/*
 * 建立与Stun服务器的连接,获得自己的公网ip和端口
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
int Natpunch::connectToStunServer()
{
	/*if(domain.size() < 4 || domain.substr(0,4) != string("stun"))
	 *{
	 *    cerr<<"StunServer domain must start with 'stun'!"<<endl;
	 *    exit(1);
	 *}*/
	hostent* host = gethostbyname(domain.c_str());
	char str[32];
	if(host == NULL)
	{
		errorMsg = "StunServer Domain Resolution Failed";
		selfInfoget = true;
		return -1;
	}
	inet_ntop(host->h_addrtype, (host->h_addr_list[0]), str, sizeof(str));
	//实际上我们不需要这一步,我们只需要知道一个ip地址即可
	/*for(int i=0;;i++){
	 *    if(host->h_addr_list[i] != NULL){[> 不是IP地址数组的结尾 <]
	 *        cout<<str<<endl;
	 *    }
	 *    else{[>达到结尾<]
	 *        break;  [>退出for循环<]
	 *    }
	 *}*/
	struct sockaddr_in localaddr;
	char return_ip[32];
	unsigned short return_port = 0;

	sockfd = socket(AF_INET,SOCK_DGRAM,0);
	if(sockfd == -1)
		err_exit("SOCKET");
	openSocket = true;

	bzero(&localaddr,sizeof(localaddr));
	localaddr.sin_family = AF_INET;
	localaddr.sin_port = htons(local_port);
	localaddr.sin_addr.s_addr = inet_addr("0.0.0.0");
	int n = -1;
	int loop = 0;
	while(n == -1)
	{
		localaddr.sin_port = htons(local_port + loop);
		n = ::bind(sockfd, (struct sockaddr*)&localaddr,sizeof(localaddr));
		++loop;
		//不做过多尝试,如果一百次都失败就报错退出
		if(loop > 1000)
		{
			errorMsg = "Bind Failed";
			selfInfoget = true;
			return -1;
		}
	}

	setTimeout(5);
	n = stun_xor_addr(sockfd,str,stun_server_port,return_ip,&return_port);
	//不论连接失败还是成功都重置selfInfoget
	selfInfoget = true;
	if(n < 0)
	{
		errorMsg = "Connection To STUN Server FAILED!";
		return -1;
	}

	selfInfo.ip.s_addr = inet_addr(return_ip);
	selfInfo.port = htons(return_port);
#ifdef DEBUG
	cout<<return_ip<<" "<<return_port<<endl;
#endif
	return 0;
}

/*
 * 解析来自Stun服务器的消息,获得自己的公网ip和端口 
 * @param sockfd 与STUN服务器通信的套接字
 * @param stun_server_ip STUN服务器的ip地址
 * @param stun_server_port STUN服务器的端口
 * @param return_ip 从STUN服务器处获得的自身的ip 
 * @param return_port 从STUN服务器处获得的自身的port 
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
int Natpunch::stun_xor_addr(int sockfd,char * stun_server_ip,unsigned short stun_server_port,char * return_ip, unsigned short * return_port)
{
	struct sockaddr_in servaddr;
	unsigned char buf[300];
	int i;
	unsigned char bindingReq[20];

	int stun_method,msg_length;
	short attr_type;
	short attr_length;
	short port;
	short n;

	// server
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	inet_pton(AF_INET, stun_server_ip, &servaddr.sin_addr);
	servaddr.sin_port = htons(stun_server_port);


	//## first bind
	* (short *)(&bindingReq[0]) = htons(0x0001);    // stun_method
	* (short *)(&bindingReq[2]) = htons(0x0000);    // msg_length
	* (int *)(&bindingReq[4])   = htonl(0x2112A442);// magic cookie

	*(int *)(&bindingReq[8]) = htonl(0x63c7117e);   // transacation ID
	*(int *)(&bindingReq[12])= htonl(0x0714278f);
	*(int *)(&bindingReq[16])= htonl(0x5ded3221);



	//printf("Send data ...\n");
	n = sendto(sockfd, bindingReq, sizeof(bindingReq),0,(struct sockaddr *)&servaddr, sizeof(servaddr)); // send UDP
	if (n == -1)
	{
		return -1;
	}
	// time wait
	usleep(1000 * 100);

	//printf("Read recv ...\n");
	n = recvfrom(sockfd, buf, 300, 0, NULL,0); // recv UDP
	if (n == -1)
	{
		if(errno == EAGAIN || EWOULDBLOCK)
			ifTimeout = true;
		return -2;
	}
	//printf("Response from server:\n");
	//write(STDOUT_FILENO, buf, n);

	//0x0101 means binding success response
	if (*(short *)(&buf[0]) == htons(0x0101))
	{
		//printf("STUN binding resp: success !\n");
		// parse XOR
		n = htons(*(short *)(&buf[2]));
		i = 20;
		while(i<sizeof(buf))
		{
			attr_type = htons(*(short *)(&buf[i]));
			attr_length = htons(*(short *)(&buf[i+2]));
			//attr_type: xor_mapped_address(0x0020)
			if (attr_type == 0x0020)
			{
				// parse : port, IP

				port = ntohs(*(short *)(&buf[i+6]));
				port ^= 0x2112;
				//printf("@port = %d\n",(unsigned short)port);



				/*printf("@ip   = %d.",buf[i+8] ^ 0x21);
				  printf("%d.",buf[i+9] ^ 0x12);
				  printf("%d.",buf[i+10] ^ 0xA4);
				  printf("%d\n",buf[i+11] ^ 0x42);
				  */

				// return
				*return_port = port;
				sprintf(return_ip,"%d.%d.%d.%d",buf[i+8]^0x21,buf[i+9]^0x12,buf[i+10]^0xA4,buf[i+11]^0x42);


				break;
			}
			i += (4  + attr_length);


		}

	}


	// do not close the socket !!!

	return 0;
}


/*
 * 信令服务器的一个sample实现,实际上只完成了基础的部分,缺少错误处理,超时处理等 
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
void heartServer()
{
	//用于存放两个客户端外网的ip和port
	clientInfo info[2];
	char str[10] = {0};

	int sockfd = socket(AF_INET,SOCK_DGRAM,0);
	if(sockfd == -1)
		err_exit("SOCKET");

	struct sockaddr_in serveraddr;
	memset(&serveraddr,0,sizeof(serveraddr));
	serveraddr.sin_addr.s_addr = inet_addr("0.0.0.0");
	serveraddr.sin_port = htons(3478);
	serveraddr.sin_family = AF_INET;

	int ret = ::bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if(ret == -1)
		err_exit("BIND");

	while(1)
	{
		bzero(info, sizeof(clientInfo)*2);
		/* 接收两个心跳包并记录其与此链接的ip+port */
		socklen_t addrlen = sizeof(struct sockaddr_in);
		/* recvfrom(sockfd, &ch, sizeof(ch), 0, (struct sockaddr *)&serveraddr, &addrlen); */
		recvfrom(sockfd, &info[0], sizeof(clientInfo), 0, (struct sockaddr *)&serveraddr, &addrlen);
		/*memcpy(&info[0].ip, &serveraddr.sin_addr, sizeof(struct in_addr));
		 *info[0].port = serveraddr.sin_port;*/

#ifdef DEBUG
		printf("A client IP:%s \tPort:%d creat link OK!\n", inet_ntoa(info[0].ip), ntohs(info[0].port));
#endif

		/* recvfrom(sockfd, &ch, sizeof(ch), 0, (struct sockaddr *)&serveraddr, &addrlen); */
		recvfrom(sockfd, &info[1], sizeof(clientInfo), 0, (struct sockaddr *)&serveraddr, &addrlen);
		/*memcpy(&info[1].ip, &serveraddr.sin_addr, sizeof(struct in_addr));
		 *info[1].port = serveraddr.sin_port;*/

#ifdef DEBUG
		printf("B client IP:%s \tPort:%d creat link OK!\n", inet_ntoa(info[1].ip), ntohs(info[1].port));

		/* 分别向两个客户端发送对方的外网ip+port */
		printf("start informations translation...\n");
#endif
		serveraddr.sin_addr = info[0].ip;
		serveraddr.sin_port = info[0].port;
		sendto(sockfd, &info[1], sizeof(clientInfo), 0, (struct sockaddr *)&serveraddr, addrlen);
		/*sendto(sockfd, &info[0], sizeof(clientInfo), 0, (struct sockaddr *)&serveraddr, addrlen);*/

		serveraddr.sin_addr = info[1].ip;
		serveraddr.sin_port = info[1].port;
		sendto(sockfd, &info[0], sizeof(clientInfo), 0, (struct sockaddr *)&serveraddr, addrlen);
		/*sendto(sockfd, &info[1], sizeof(clientInfo), 0, (struct sockaddr *)&serveraddr, addrlen);*/
#ifdef DEBUG
		printf("send informations successful!\n");
#endif
	}
	close(sockfd);
}

/*
 * 设置套接字为非阻塞的 
 * @param sockfd 设置的套接字
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
void Natpunch::setNonblocking(int sockfd)
{
	int flag = fcntl(sockfd, F_GETFL, 0);
	if(flag < 0)
	{
		err_exit("fcntl F_GETFL FAILED!");
	}
	if(fcntl(sockfd, F_SETFL, flag | O_NONBLOCK) < 0)
		err_exit("fcntl F_SETFL FAILED!");
}

/*
 * 设置套接字为非阻塞的 
 * @param t 设置的超时时间
 *
 * @return 正确返回0,异常情况返回-1,并设置错误消息
 * */
void Natpunch::setTimeout(unsigned short t)
{
	struct timeval tv;
	tv.tv_sec =	t;
	tv.tv_usec = 0;
	if(setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
		err_exit("SET SOCKET TIMEOUT");
}
