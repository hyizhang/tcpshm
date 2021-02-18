import threading
tcpshm=...


tcpshm.get_instance()
send_num_file=tcpshm.GetPtcpDir() + "/" + tcpshm.GetLocalName() + "_" + tcpshm.GetRemoteName() + ".send_num";
recv_num_file=tcpshm.GetPtcpDir() + "/" + tcpshm.GetLocalName() + "_" + tcpshm.GetRemoteName() + ".recv_num";
char* error_msg;
send_num=tcpshm.get_send_num(send_num_file,false,error_msg);
receive_num=tcpshm.get_receive_num(send_num_file,false,error_msg);

开一个进程  //python写
{
if(do_cpupin) cpupin(7); //python写pincpu的
 start_time = now(); //python写 一个计时的
    while(!tcpshm.inclosed()) { //so调函数，已改
        if(PollNum()) {   //python写
            stop_time = now();//python写 一个计时的
            tcpshm.Close();  //调so函数，已改
            break;
        }
        tcpshm.PollShm();  //调so函数
}
}
while(!tcpshm.inclosed()) {  //调so函数，已改
  tcpshm.PollTcp(now());  //调so函数，已改
}
shm_thr.join(); //threading来写
//主线程等待子线程的终止。也就是说主线程的代码块中，如果碰到了t.join()方法，此时主线程需要等待（阻塞），等待子线程结束了(Waits for this thread to die.),才能继续执行t.join()之后的代码块。