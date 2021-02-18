import os
import sys
import ctypes

def display_recv_num(recv_num):
   print (recv_num)

def get_msg(send_num):
   return (send_num+1)

def display_OnSystemError(error_msg,sys_errno):
   print ("error_msg:"+str(error_msg))
   print ("syserrno:"+str(sys_errno))

def display_OnLoginSuccess():
   print ("Login Success")

def display_OnLoginReject(error_msg):
   print ("Login Rejected: "+str(error_msg))

def display_OnDisconnected(reason,sys_errno):
   print ("Client disconnected reason: "+str(reason))
   print (" syserrno: " +str(sys_errno))

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


if __name__ == '__main__':
    name = sys.argv[1];
    server_ip = sys.argv[2];
    use_shm = sys.argv[3][0] != '0';
    #sys.argv[]
    
    echoclient=ctypes.cdll.LoadLibrary("")
    echoclient.get_instance(name,name)
    echoclient.run(use_shm,server_ip, 12345)
                               