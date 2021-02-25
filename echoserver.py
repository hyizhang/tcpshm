import ctypes


if __name__ == '__main__':

    
    echoserver=ctypes.cdll.LoadLibrary("./libserver.so")
    echoserver.c_wrap_get_instance()
    echoserver.c_wrap_run()
