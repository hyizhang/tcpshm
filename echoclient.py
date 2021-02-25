import os
import sys
import ctypes
import threading
import inspect
from ctypes import *
import ctypes

 

def _async_raise(tid, exctype):
    tid = ctypes.c_long(tid)
    if not inspect.isclass(exctype):
        exctype = type(exctype)
    res = ctypes.pythonapi.PyThreadState_SetAsyncExc(tid, ctypes.py_object(exctype))
    if res == 0:
        raise ValueError("invalid thread id")
    elif res != 1:
        ctypes.pythonapi.PyThreadState_SetAsyncExc(tid, None)
        raise SystemError("PyThreadState_SetAsyncExc failed")

 
def stop_thread(thread):
    _async_raise(thread.ident, SystemExit)






if __name__ == '__main__':
    
    echoclient=ctypes.cdll.LoadLibrary("./libclient.so")
    def send_receive():
        while(1):
           #receive message
           receive=echoclient.c_wrap_receive 
           receive.restype=c_char_p
           result=receive() 
           print (result)
           #send message
	       echoclient.c_wrap_send()

          
   
    
    echoclient.c_wrap_get_instance()

    # start  thread
    thread = threading.Thread(target=send_receive)
    thread.start()

    # run connection
    echoclient.c_wrap_run()
    #stop thread
    stop_thread(thread)

                               
