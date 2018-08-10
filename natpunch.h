#ifndef NATPUNCH_H_
#define NATPUNCH_H_

#include <string>
#include <unistd.h>
#include <cstdio>
#include <sys/socket.h>
#include <cstdlib>
#include <arpa/inet.h>
#include <functional>
#include <pthread.h>

typedef struct
{
	struct in_addr ip;
	int port;
}clientInfo;//用来储存客户端的ip地址和端口

/*
 * 回调函数的类型声明
 * */
typedef std::function<void (void*)> callback;

/*
 * 错误处理函数
 * */
static void err_exit(std::string s)
{
	perror(s.c_str());
	exit(1);
}

//接收数据的线程运行的函数
/*
 * 接收数据的线程运行的函数
 * @param args 类的this指针,用于调用类的成员函数
 * */
void* threadRecvStunServerData(void *args);
void *threadRecvSignalServerData(void *);
void *threadRecvData(void *);

/*
 * 默认的本地绑定端口,如果绑定失败会改变端口进行绑定,如果失败100次就报错
 * */
const int local_port = 18789;

/*
 * 默认的接收缓冲区大小
 * */
const int BUFSIZE = 10240;

/*
 * 接收数据线程的最大数量
 * */
const int recvThreadNum = 10;

class Natpunch
{
	public:
		Natpunch(){}
		~Natpunch(){
			closeConnection();
		}//等待接收线程退出
		void initialWork();
		int clientConnect();//提供给用户的接口,只有基础的连接到stun服务器获取自己ip和端口的功能
		int sendMsgToSigalServer();//将自己的ip及port发送给signal服务器
		int RecvMsgFromSigalServer();//从signal服务器获得打洞另一端的ip及端口
		int startNatPunch();//开始尝试打洞
		int sendData(const std::string& data);//打洞成功之后发送数据

		int startRecvData();//设置当到接收数据时的回调函数
		std::string getErrorMsg()
		{
			return errorMsg;
		}
		void closeConnection()//
		{
			if(openSocket)
			{
				close(sockfd);
				openSocket = false;
			}
			if(recvThread)
			{
				while(usedThreadNum > 0)
				{
					--usedThreadNum;
					pthread_cancel(recvThreadId[usedThreadNum]);
				}
				/*pthread_exit(NULL);*/
				recvThread = false;
			}
		}
		void setHeartServerDomain(std::string heartServer_ip)
		{
			server_ip = heartServer_ip;
		}
		void setHeartServerPort(int heartServer_port)
		{
			server_port = heartServer_port;
		}
		void setSTUNServerDomain(std::string STUNServerDomain)
		{
			domain = STUNServerDomain;
		}
		void setSTUNServerPort(unsigned short STUNServerPort)
		{
			stun_server_port = STUNServerPort;
		}
		void setCallbackFunc(callback func)
		{
			callbackFunc = func;
		}
		friend void* threadRecvStunServerData(void *args);
		friend void* threadRecvSignalServerData(void *args);
		friend void* threadRecvData(void *args);
		bool ifRemoteClientInfoGet()
		{
			return remoteClientInfoGet;
		}
		bool ifSelfInfoGet()
		{
			return selfInfoget;
		}
		bool getIfTimeout()
		{
			return ifTimeout;
		}
	private:
		std::string errorMsg;
		//是否开启接收数据的线程的标志
		bool recvThread = false;
		//接收数据的线程的id
		pthread_t recvThreadId[recvThreadNum];
		unsigned int usedThreadNum = 0;
		//是否打开了套接字的标志
		bool openSocket = false;
		//下面这两个布尔值只代表状态,并不代表结果,如果正在与服务器通信就是false,否则就是true
		bool selfInfoget = true;
		bool remoteClientInfoGet = true;
		//接收是否超时的标志
		bool ifTimeout = false;
		int sockfd;
		//信令服务器的地址和端口
		int server_port;
		std::string server_ip;
		/*std::string domain = "stun.seewo.com";*/
		//STUN服务器的ip以及端口
		std::string domain = "stun.xten.com";
		unsigned short stun_server_port = 3478;
		//打洞另一客户端的信息
		clientInfo remoteClientInfo;
		//自身的信息
		clientInfo selfInfo;
		callback callbackFunc;//用户的注册回调函数,在接收到数据的时候使用
		void recvData();//接收数据的函数
		int initialConnect();//初始情况下与远端服务器相连接,获得另一客户端的ip地址以及端口信息
		/*int connectToStunServer(std::string domain = "stun.seewo.com", unsigned short stun_server_port = 3478);//辅助initialConnect,尝试与STUN服务器通信*/
		int connectToStunServer();//辅助initialConnect,尝试与STUN服务器通信
		int stun_xor_addr(int sockfd,char * stun_server_ip,unsigned short stun_server_port,char * return_ip, unsigned short * return_port);
		void communication(int sockfd, struct sockaddr*,socklen_t * );//尝试与另一客户端打洞通信,如果成功即开始通信,如果失败就需要服务器进行转发通信
		void indirectCommunication();//由于NAT类型原因不能进行打洞通信,需要服务器进行转发消息
		int establishConnectionToHeartServer(int sockfd, struct sockaddr_in&);
		int getRemoteClientInfo();
		void getSelfInfo(int sockfd);//不应该被使用的函数,之前测试所用
		void setNonblocking(int sockfd);//设置套接字为非阻塞的
		void setTimeout(unsigned short t = 3);//设置套接字超时
};


#endif
