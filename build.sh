g++ -std=c++11 -fPIC -O3 -shared wrap_client.cc -o libclient.so -I/usr/include/python2.7/ -I/usr/include/python2.7/include -lpython2.7 -export-dynamic -lrt -lpthread
g++ -std=c++11 -fPIC -O3 -shared wrap_server.cc -o libserver.so -lrt -lpthread
