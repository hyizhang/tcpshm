#include "../tcpshm_server.h"
#include <bits/stdc++.h>
#include <python.h>
#include "timestamp.h"
#include "common.h"
#include "cpupin.h"

using namespace std;
using namespace tcpshm;


struct ServerConf : public CommonConf
{
  static const int64_t NanoInSecond = 1000000000LL;

  static const uint32_t MaxNewConnections = 5;
  static const uint32_t MaxShmConnsPerGrp = 4;
  static const uint32_t MaxShmGrps = 1;
  static const uint32_t MaxTcpConnsPerGrp = 4;
  static const uint32_t MaxTcpGrps = 1;

  // echo server's TcpQueueSize should be larger than that of client if client is in fast mode
  // otherwise server's send queue could be blocked and ack_seq can only be sent through HB which is slow
  static const uint32_t TcpQueueSize = 3000;       // must be a multiple of 8
  static const uint32_t TcpRecvBufInitSize = 1000; // must be a multiple of 8
  static const uint32_t TcpRecvBufMaxSize = 2000;  // must be a multiple of 8
  static const bool TcpNoDelay = true;

  static const int64_t NewConnectionTimeout = 3 * NanoInSecond;
  static const int64_t ConnectionTimeout = 10 * NanoInSecond;
  static const int64_t HeartBeatInverval = 3 * NanoInSecond;

  using ConnectionUserData = char;
};

class EchoServer;
using TSServer = TcpShmServer<EchoServer, ServerConf>;

class EchoServer : public TSServer
{
public:
    EchoServer(const std::string& ptcp_dir, const std::string& name)
        : TSServer(ptcp_dir, name) {
        // capture SIGTERM to gracefully stop the server
        // we can also send other signals to crash the server and see how it recovers on restart
        signal(SIGTERM, EchoServer::SignalHandler);
    }

    static void SignalHandler(int s) {
        stopped = true;
    }

    void Run(const char* listen_ipv4, uint16_t listen_port) {
        if(!Start(listen_ipv4, listen_port)) return;
        vector<thread> threads;
        // create threads for polling tcp
        for(int i = 0; i < ServerConf::MaxTcpGrps; i++) {
          threads.emplace_back([this, i]() {
            if (do_cpupin) cpupin(4 + i);
            while (!stopped) {
              PollTcp(now(), i);
            }
          });
        }

        // create threads for polling shm
        for(int i = 0; i < ServerConf::MaxShmGrps; i++) {
          threads.emplace_back([this, i]() {
            if (do_cpupin) cpupin(4 + ServerConf::MaxTcpGrps + i);
            while (!stopped) {
              PollShm(i);
            }
          });
        }

        // polling control using this thread
        while(!stopped) {
          PollCtl(now());
        }

        for(auto& thr : threads) {
            thr.join();
        }
        Stop();
        cout << "Server stopped" << endl;
    }

private:
    friend TSServer;

    // called with Start()
    // reporting errors on Starting the server
    void OnSystemError(const char* errno_msg, int sys_errno) {
        Py_Initialize();
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("sys.path.append('./')");
        PyObject* pModule = NULL;
        PyObject* pFunc = NULL;
        pModule = PyImport_ImportModule("echo_client");
        pFunc = PyObject_GetAttrString(pModule, "display_OnSystemError");
        PyObject* pParams = Py_BuildValue("ss", error_msg, sys_errno);
        PyEval_CallObject(pFunc, pParams);
        Py_Finalize();
        
        //display_OnSystemError(error_msg,sys_errno);
        cout << "System Error: " << errno_msg << " syserrno: " << strerror(sys_errno) << endl;
    }

    // called by CTL thread
    // if accept the connection, set user_data in login_rsp and return grpid(start from 0) with respect to tcp or shm
    // else set error_msg in login_rsp if possible, and return -1
    // Note that even if we accept it here, there could be other errors on handling the login,
    // so we have to wait OnClientLogon for confirmation
    int OnNewConnection(const struct sockaddr_in& addr, const LoginMsg* login, LoginRspMsg* login_rsp) {
        Py_Initialize();
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("sys.path.append('./')");
        PyObject* pModule = NULL;
        PyObject* pFunc = NULL;
        pModule = PyImport_ImportModule("echo_client");
        pFunc = PyObject_GetAttrString(pModule, "display_OnNewConnection");
        PyObject* pParams = Py_BuildValue("ssss",inet_ntoa(addr.sin_addr),ntohs(addr.sin_port),login->client_name,(bool)login->use_shm);
        PyEval_CallObject(pFunc, pParams);
        Py_Finalize();
        //display_OnNewConnection(inet_ntoa(addr.sin_addr),ntohs(addr.sin_port),login->client_name,(bool)login->use_shm);
        cout << "New Connection from: " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port)
             << ", name: " << login->client_name << ", use_shm: " << (bool)login->use_shm << endl;
        // here we simply hash client name to uniformly map to each group
        auto hh = hash<string>{}(string(login->client_name));
        if(login->use_shm) {
            if(ServerConf::MaxShmGrps > 0) {
                return hh % ServerConf::MaxShmGrps;
            }
            else {
                strcpy(login_rsp->error_msg, "Shm disabled");
                return -1;
            }
        }
        else {
            if(ServerConf::MaxTcpGrps > 0) {
                return hh % ServerConf::MaxTcpGrps;
            }
            else {
                strcpy(login_rsp->error_msg, "Tcp disabled");
                return -1;
            }
        }
    }

    // called by CTL thread
    // ptcp or shm files can't be open or are corrupt
    void OnClientFileError(Connection& conn, const char* reason, int sys_errno) {
        Py_Initialize();
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("sys.path.append('./')");
        PyObject* pModule = NULL;
        PyObject* pFunc = NULL;
        pModule = PyImport_ImportModule("echo_client");
        pFunc = PyObject_GetAttrString(pModule, "display_OnClientFileError");
        PyObject* pParams = Py_BuildValue("sss", conn.GetRemoteName(), reason,strerror(sys_errno));
        PyEval_CallObject(pFunc, pParams);
        Py_Finalize();
        
        //display_OnClientFileError(conn.GetRemoteName(),reason,strerror(sys_errno));
        cout << "Client file errno, name: " << conn.GetRemoteName() << " reason: " << reason
             << " syserrno: " << strerror(sys_errno) << endl;
    }

    // called by CTL thread
    // server and client ptcp sequence number don't match, we need to fix it manually
    void OnSeqNumberMismatch(Connection& conn,
                             uint32_t local_ack_seq,
                             uint32_t local_seq_start,
                             uint32_t local_seq_end,
                             uint32_t remote_ack_seq,
                             uint32_t remote_seq_start,
                             uint32_t remote_seq_end) {
        Py_Initialize();
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("sys.path.append('./')");
        PyObject* pModule = NULL;
        PyObject* pFunc = NULL;
        pModule = PyImport_ImportModule("echo_client");
        pFunc = PyObject_GetAttrString(pModule, "display_OnSeqNumberMismatch");
        PyObject* pParams = Py_BuildValue("ssssssss",conn.GetRemoteName(),conn.GetPtcpFile(),local_ack_seq,local_seq_start,local_seq_end,remote_ack_seq,remote_seq_start,remote_seq_end);
        PyEval_CallObject(pFunc, pParams);
        Py_Finalize();
        //display_OnSeqNumberMismatch(conn.GetRemoteName(),conn.GetPtcpFile(),local_ack_seq,local_seq_start,local_seq_end,remote_ack_seq,remote_seq_start,remote_seq_end);
        cout << "Client seq number mismatch, name: " << conn.GetRemoteName() << " ptcp file: " << conn.GetPtcpFile()
             << " local_ack_seq: " << local_ack_seq << " local_seq_start: " << local_seq_start
             << " local_seq_end: " << local_seq_end << " remote_ack_seq: " << remote_ack_seq
             << " remote_seq_start: " << remote_seq_start << " remote_seq_end: " << remote_seq_end << endl;
    }

    // called by CTL thread
    // confirmation for client logon
    void OnClientLogon(const struct sockaddr_in& addr, Connection& conn) {
        Py_Initialize();
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("sys.path.append('./')");
        PyObject* pModule = NULL;
        PyObject* pFunc = NULL;
        pModule = PyImport_ImportModule("echo_client");
        pFunc = PyObject_GetAttrString(pModule, "display_OnClientLogon");
        PyObject* pParams = Py_BuildValue("sss", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),conn.GetRemoteName());
        PyEval_CallObject(pFunc, pParams);
        Py_Finalize();
        //display_OnClientLogon(inet_ntoa(addr.sin_addr),ntohs(addr.sin_port),conn.GetRemoteName());
        cout << "Client Logon from: " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port)
             << ", name: " << conn.GetRemoteName() << endl;
    }

    // called by CTL thread
    // client is disconnected
    void OnClientDisconnected(Connection& conn, const char* reason, int sys_errno) {
        Py_Initialize();
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("sys.path.append('./')");
        PyObject* pModule = NULL;
        PyObject* pFunc = NULL;
        pModule = PyImport_ImportModule("echo_client");
        pFunc = PyObject_GetAttrString(pModule, "display_OnClientDisconnected");
        PyObject* pParams = Py_BuildValue("sss", conn.GetRemoteName(),reason,strerror(sys_errno));
        PyEval_CallObject(pFunc, pParams);
        Py_Finalize();
        //display_OnClientDisconnected(conn.GetRemoteName(),reason,strerror(sys_errno));
        cout << "Client disconnected,.name: " << conn.GetRemoteName() << " reason: " << reason
             << " syserrno: " << strerror(sys_errno) << endl;
    }

    // called by APP thread
    void OnClientMsg(Connection& conn, MsgHeader* recv_header) {
        OnServerMsg(recv_header);
        //msg=process();
        auto size = recv_header->size - sizeof(MsgHeader);
        MsgHeader* send_header = conn.Alloc(size);
        if(!send_header) return;
        send_header->msg_type = recv_header->msg_type;
            
        //T* msg = (T*)(header + 1);
        for(auto& v : (send_header+1)->val) {
            Py_Initialize();
            PyRun_SimpleString("import sys");
            PyRun_SimpleString("sys.path.append('./')");
            PyObject* pModule = NULL;
            PyObject* pFunc = NULL;
            pModule = PyImport_ImportModule("echo_client");
            pFunc = PyObject_GetAttrString(pModule, "process");
            PyObject* pParams = Py_BuildValue("");
            //message=get_msg(*send_num);
            PyObject* pRet = PyObject_CallObject(pFunc, pParams);
            long message = PyLong_AsLong(pRet);
            
            //message=process();  //python
            v = Endian<ClientConf::ToLittleEndian>::Convert(message);
        }
        //msg=process(); //python 语句处理收到的消息，并制作要发送出去的消息
        //memcpy(send_header + 1, recv_header + 1, size);
        // if we call Push() before Pop(), there's a good chance Pop() is not called in case of program crash
        conn.Pop();
        conn.Push();
        //MsgHeader* header = conn.Alloc(sizeof(T));
        //if(!header) return false;
        /*
        header->msg_type = T::msg_type;
        T* msg = (T*)(header + 1);
        for(auto& v : msg->val) {
            // convert to configurated network byte order, don't need this if you know server is using the same endian
            v = Endian<ClientConf::ToLittleEndian>::Convert(msg);
        }
        */
    }
    
    
    void OnServerMsg(MsgHeader* header) {
        // auto msg_type = header->msg_type;
        switch(header->msg_type) {
            case 1: handleMsg((Msg1*)(header + 1)); break;
            case 2: handleMsg((Msg2*)(header + 1)); break;
            case 3: handleMsg((Msg3*)(header + 1)); break;
            case 4: handleMsg((Msg4*)(header + 1)); break;
            default: assert(false);
        }
        //conn.Pop();
    }
    
    template<class T>
    void handleMsg(T* msg) {
        for(auto v : msg->val) {
            // convert from configurated network byte order
            Endian<ClientConf::ToLittleEndian>::ConvertInPlace(v);
            Py_Initialize();
            PyRun_SimpleString("import sys");
            PyRun_SimpleString("sys.path.append('./')");
            PyObject* pModule = NULL;
            PyObject* pFunc = NULL;
            pModule = PyImport_ImportModule("echo_client");
            pFunc = PyObject_GetAttrString(pModule, "display_recv_num");
            PyObject* pParams = Py_BuildValue("i",v);
            PyEval_CallObject(pFunc, pParams);
            Py_Finalize();
            //display_recv_num(v); //python 函数 将c得到的结果v传到python中
        }
    }
    
    static volatile bool stopped;
    // set do_cpupin to true to get more stable latency
    bool do_cpupin = true;
};

volatile bool EchoServer::stopped = false;


EchoServer* server;

void get_instance(const std::string& ptcp_dir, const std::string& name)
{
    server=new EchoServer(ptcp_dir,name);
}

void Run(const char* listen_ipv4, uint16_t listen_port)
{
   client->Run(listen_ipv4,listen_port);
}