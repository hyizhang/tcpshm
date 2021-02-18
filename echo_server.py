import os
import sys
import ctypes


recv_msg=[]


def display_recv_num(v):
   recv_msg.append(v)


#在这里处理收到的消息
def process():  
   message= recv_msg[0]
   del recv_msg[0]
   return message


   
def display_OnSystemError(error_msg,sys_errno):
   print ("error_msg:"+str(error_msg))
   print ("syserrno:"+str(sys_errno))

def display_OnSeqNumberMismatch(RemoteName,PtcpFile,local_ack_seq,local_seq_start,
                               local_seq_end,remote_ack_seq,remote_seq_start,remote_seq_end):
    print("Seq number mismatch, name: "+str(RemoteName))
    print(" ptcp file: "+str(PtcpFile))
    print (" local_ack_seq: "+str(local_ack_seq))
    print (" local_seq_start: "+str(local_seq_start))
    print (" local_seq_end: "+str(local_seq_end))
    print (" remote_ack_seq: "+str(remote_ack_seq))
    print (" remote_seq_start: "+str(remote_seq_start))
    print (" remote_seq_end: "+str(remote_seq_end))


def display_OnClientDisconnected(RemoteName,reason,sys_errno):
    print ("Client disconnected,.name: "+str(RemoteName))
    print (" reason: "+str(reason))
    print (" syserrno: "+str(sys_errno))

def display_OnClientLogon(sin_addr,sin_port,RemoteName):
    print ("Client Logon from: "+str(sin_addr))
    print (":"+str(sin_port))
    print (", name: " +str(RemoteName))

def display_OnClientFileError(RemoteName,reason,sys_errno):
    print("Client file errno, name: "+str(RemoteName)) 
    print (" reason: "+str(reason))
    print (" syserrno: "+str(sys_errno))


def display_OnNewConnection(sin_addr,sin_port,client_name,use_shm):
    print ("New Connection from: "+str(sin_addr))
    print (":"+str(sin_port))
    print (", name: "+str(client_name))
    print (", use_shm: "+str(use_shm))


if __name__ == '__main__':

    #sys.argv[]
    
    echoserver=ctypes.cdll.LoadLibrary("")
    echoserver.get_instance("server","server")
    echoserver.run("0.0.0.0", 12345)