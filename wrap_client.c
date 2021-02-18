#include "../tcpshm_client.h"
#include <bits/stdc++.h>
#include <python.h>
#include "timestamp.h"
#include "common.h"
#include "cpupin.h"


using namespace std;
using namespace tcpshm;

struct ClientConf : public CommonConf
{
  static const int64_t NanoInSecond = 1000000000LL;

  static const uint32_t TcpQueueSize = 2000;       // must be a multiple of 8
  static const uint32_t TcpRecvBufInitSize = 1000; // must be a multiple of 8
  static const uint32_t TcpRecvBufMaxSize = 2000;  // must be a multiple of 8
  static const bool TcpNoDelay = true;

  static const int64_t ConnectionTimeout = 10 * NanoInSecond;
  static const int64_t HeartBeatInverval = 3 * NanoInSecond;

  using ConnectionUserData = char;
};

class EchoClient;
using TSClient = TcpShmClient<EchoClient, ClientConf>;

class EchoClient : public TSClient
{
public:
    EchoClient(const std::string& ptcp_dir, const std::string& name)
        : TSClient(ptcp_dir, name)
        , conn(GetConnection()) {
        srand(time(NULL));
    }

    void Run(bool use_shm, const char* server_ipv4, uint16_t server_port) {
        if(!Connect(use_shm, server_ipv4, server_port, 0)) return;
        // we mmap the send and recv number to file in case of program crash
        string send_num_file =
            string(conn.GetPtcpDir()) + "/" + conn.GetLocalName() + "_" + conn.GetRemoteName() + ".send_num";
        string recv_num_file =
            string(conn.GetPtcpDir()) + "/" + conn.GetLocalName() + "_" + conn.GetRemoteName() + ".recv_num";
        const char* error_msg;
        send_num = my_mmap<int>(send_num_file.c_str(), false, &error_msg);
        recv_num = my_mmap<int>(recv_num_file.c_str(), false, &error_msg);
        if(!send_num || !recv_num) {
            cout << "System Error: " << error_msg << " syserrno: " << strerror(errno) << endl;
            return;
        }
        cout << "client started, send_num: " << *send_num << " recv_num: " << *recv_num << endl;
        if(use_shm) {
            thread shm_thr([this]() {
                if(do_cpupin) cpupin(7);
                start_time = now();
                while(!conn.IsClosed()) {
                    //int msg=get_msg(*send_num); //python 函数生成消息内容
                    if(PollNum()) {    //发送数字
                        stop_time = now();
                        conn.Close();
                        break;
                    }
                    PollShm();  //从共享内存中提取服务器发送回来的数字
                }
            });

            // we still need to poll tcp for heartbeats even if using shm
            while(!conn.IsClosed()) {
              PollTcp(now());    //确认主机还连接着，这个进程和上面的并列
            }
            shm_thr.join();
        }
        else {
            if(do_cpupin) cpupin(7);
            start_time = now();
            while(!conn.IsClosed()) {
                //int msg=get_msg(*send_num);
                if(PollNum()) {      //发送消息
                    stop_time = now();
                    conn.Close();
                    break;
                }
                PollTcp(now());    //从socket中提取消息
            }
        }
        uint64_t latency = stop_time - start_time;
        Stop();
        cout << "client stopped, send_num: " << *send_num << " recv_num: " << *recv_num << " latency: " << latency
             << " avg rtt: " << (msg_sent > 0 ? (double)latency / msg_sent : 0.0) << " ns" << endl;
    }
    

private:
    template<class T>
    bool PollNum() {
        if(*send_num < MaxNum) {
            // for slow mode, we wait to recv an echo msg before sending the next one
            if(slow && *send_num != *recv_num) return false;
            // we randomly send one of the 4 msgs
            int tp = rand() % 4 + 1;
            switch(tp) {
                case 1: TrySendMsg<Msg1>(); break;
                case 2: TrySendMsg<Msg2>(); break;
                case 3: TrySendMsg<Msg3>(); break;
                case 4: TrySendMsg<Msg4>(); break;
            }
        }
        else {
            // if all echo msgs are got, we are done
            if(*send_num == *recv_num) return true;
        }
        return false;
    }

    template<class T>
    bool TrySendMsg() {
        MsgHeader* header = conn.Alloc(sizeof(T));
        if(!header) return false;
        header->msg_type = T::msg_type;
        T* msg = (T*)(header + 1);
        for(auto& v : msg->val) {
            //调用python
            Py_Initialize();
            PyRun_SimpleString("import sys");
            PyRun_SimpleString("sys.path.append('./')");
            PyObject* pModule = NULL;
            PyObject* pFunc = NULL;
            pModule = PyImport_ImportModule("echo_client");
            pFunc = PyObject_GetAttrString(pModule, "get_msg");
            PyObject* pParams = Py_BuildValue("i",*send_num);
            //message=get_msg(*send_num);
            PyObject* pRet = PyObject_CallObject(pFunc, pParams);
            long message = PyLong_AsLong(pRet);
            //PyArg_Parse(pRet, "s", &result);
            Py_Finalize();
            
            (*send_num)++;
            v = Endian<ClientConf::ToLittleEndian>::Convert(message);
        }
        conn.Push();
        msg_sent++;
        return true;
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
            
            if(v != *recv_num) {
                cout << "bad: v: " << v << " recv_num: " << (*recv_num) << endl;
                exit(1);
            }
            (*recv_num)++;
        }
    }

private:
    friend TSClient;
    // called within Connect()
    // reporting errors on connecting to the server
    void OnSystemError(const char* error_msg, int sys_errno) {
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
        cout << "System Error: " << error_msg << " syserrno: " << strerror(sys_errno) << endl;
    }

    // called within Connect()
    // Login rejected by server
    void OnLoginReject(const LoginRspMsg* login_rsp) {
        Py_Initialize();
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("sys.path.append('./')");
        PyObject* pModule = NULL;
        PyObject* pFunc = NULL;
        pModule = PyImport_ImportModule("echo_client");
        pFunc = PyObject_GetAttrString(pModule, "display_OnLoginReject");
        PyObject* pParams = Py_BuildValue("s", login_rsp->error_msg);
        PyEval_CallObject(pFunc, pParams);
        Py_Finalize();
        //display_OnLoginReject(login_rsp->error_msg);
        cout << "Login Rejected: " << login_rsp->error_msg << endl;
    }

    // called within Connect()
    // confirmation for login success
    int64_t OnLoginSuccess(const LoginRspMsg* login_rsp) {
        Py_Initialize();
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("sys.path.append('./')");
        PyObject* pModule = NULL;
        PyObject* pFunc = NULL;
        pModule = PyImport_ImportModule("echo_client");
        pFunc = PyObject_GetAttrString(pModule, "display_OnLoginSuccess");
        PyObject* pParams = Py_BuildValue("");
        PyEval_CallObject(pFunc, pParams);
        Py_Finalize();
        //display_OnLoginSuccess();
        cout << "Login Success" << endl;
        return now();
    }

    // called within Connect()
    // server and client ptcp sequence number don't match, we need to fix it manually
    void OnSeqNumberMismatch(uint32_t local_ack_seq,
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
        cout << "Seq number mismatch, name: " << conn.GetRemoteName() << " ptcp file: " << conn.GetPtcpFile()
             << " local_ack_seq: " << local_ack_seq << " local_seq_start: " << local_seq_start
             << " local_seq_end: " << local_seq_end << " remote_ack_seq: " << remote_ack_seq
             << " remote_seq_start: " << remote_seq_start << " remote_seq_end: " << remote_seq_end << endl;
    }

    // called by APP thread
    void OnServerMsg(MsgHeader* header) {
        // auto msg_type = header->msg_type;
        switch(header->msg_type) {
            case 1: handleMsg((Msg1*)(header + 1)); break;
            case 2: handleMsg((Msg2*)(header + 1)); break;
            case 3: handleMsg((Msg3*)(header + 1)); break;
            case 4: handleMsg((Msg4*)(header + 1)); break;
            default: assert(false);
        }
        conn.Pop();
    }

    // called by tcp thread
    void OnDisconnected(const char* reason, int sys_errno) {
        Py_Initialize();
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("sys.path.append('./')");
        PyObject* pModule = NULL;
        PyObject* pFunc = NULL;
        pModule = PyImport_ImportModule("echo_client");
        pFunc = PyObject_GetAttrString(pModule, "display_OnDisconnected");
        PyObject* pParams = Py_BuildValue("ss", reason,strerror(sys_errno));
        PyEval_CallObject(pFunc, pParams);
        Py_Finalize();
        //display_OnDisconnected(reason,strerror(sys_errno));
        cout << "Client disconnected reason: " << reason << " syserrno: " << strerror(sys_errno) << endl;
    }

private:
    static const int MaxNum = 10000000;
    Connection& conn;
    int msg_sent = 0;
    uint64_t start_time = 0;
    uint64_t stop_time = 0;
    // set slow to false to send msgs as fast as it can
    bool slow = true;
    // set do_cpupin to true to get more stable latency
    bool do_cpupin = true;
    int* send_num;
    int* recv_num;
};

EchoClient* client;

void get_instance(const std::string& ptcp_dir, const std::string& name)
{
    client=new EchoClient(ptcp_dir,name);
}

void Run(bool use_shm, const char* server_ipv4, uint16_t server_port)
{
   client->Run(use_shm,server_ipv4,server_port);
}
/*
bool Connect(bool use_shm,
                 const char* server_ipv4,
                 uint16_t server_port,
                 const typename Conf::LoginUserData& login_user_data)
{
     obj->Connect(bool use_shm,
                 const char* server_ipv4,
                 uint16_t server_port,
                 const typename Conf::LoginUserData& login_user_data);

    
}

                 
void PollTcp(int now)
{
    obj->PollTcp(now);
}

void PollShm()
{
    obj->PollShm();
}

void PollShm()
{
    obj->Stop();
}

void GetConnection()
{
    conn=obj.GetConnection(); 
}
string GetPtcpDir()
{
    conn.GetPtcpDir();
}
string GetLocalName()
{
    conn.GetLocalName();
}

string GetRemoteName()
{
    conn.GetRemoteName();
}
int get_send_num(string send_num_file,  bool flag, char* &error_msg)
{
    return *(my_mmap<int>(send_num_file.c_str(), false, &error_msg));
}

int get_receive_num(string send_num_file,  bool flag, char* &error_msg)
{
    return *(my_mmap<int>(recv_num_file.c_str(), false, &error_msg););
}

bool inclosed()
{
   return conn.IsClosed();
}

void Close()
{
    conn.Close();
}
*/


