#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>

#include <map>
#include <chrono>
#include <deque>

#define PORT 8080
#define REQUEST_LIMIT 5
#define WINDOW_SIZE 10

using namespace std;

queue<int> task_queue;
mutex queue_mutex;
condition_variable condition;

map<string,deque<chrono::steady_clock::time_point>> request_map;
mutex rate_limit_mutex;

void handle_client(int socket_fd){
    char buffer[1024] = {0};
    const char *msg = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-length: 19\n\nHello from server";
    const char *rate_limit_msg = "HTTP/1.1 429 Too Many Requests\nContent-Type: text/plain\nContent-length: 30\n\nRate limit exceeded";
    
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    if(getpeername(socket_fd,(struct sockaddr*)&client_addr, &client_addr_len)==0){
        string client_ip = inet_ntoa(client_addr.sin_addr);
        cout<<"Client ip address: "<<client_ip<<endl;

        {
            lock_guard<mutex> lock(rate_limit_mutex);
            auto now = chrono::steady_clock::now();

            if(request_map.find(client_ip)!=request_map.end()){
                auto &timestamps = request_map[client_ip];
                while(!timestamps.empty() && chrono::duration_cast<chrono::seconds>(now-timestamps.front()).count()> WINDOW_SIZE){
                    timestamps.pop_front();
                }

                if(timestamps.size()>=REQUEST_LIMIT){
                    send(socket_fd, rate_limit_msg,strlen(rate_limit_msg),0);
                    cout<<"Blocked : "<<client_ip<<endl;
                    close(socket_fd);
                    return;
                }
                timestamps.push_back(now);
            }else{
                request_map[client_ip] = deque<chrono::steady_clock::time_point>{now};
            }
        }
    }else{
        cerr<<"Failed to fetch the IP address"<<endl;
    }

    ssize_t bytes_read = read(socket_fd,buffer,1024);
    if(bytes_read<0){
        cerr<<"Error reading from socket"<<endl;
        close(socket_fd);
        return;
    }
    cout<<"Message received: "<<buffer<<endl;

    send(socket_fd,msg,strlen(msg),0);
    cout<<"HTTP message sent to client: "<<msg<<endl;

    close(socket_fd);
}

void worker_thread(){
    while(true){
        int socket_fd;
        {
            unique_lock<mutex> lock(queue_mutex);
            condition.wait(lock,[]{return !task_queue.empty();});
            socket_fd = task_queue.front();
            task_queue.pop();
        }
        handle_client(socket_fd);
    }
}

int main(){
    int server_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    const int THREAD_POOL_SIZE = 4;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == 0){
        cerr<<"Socket creation failed"<<endl;
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(server_fd,(struct sockaddr*)&address, sizeof(address))<0){
        cerr<<"Build failed"<<endl;
        return -1;
    }

    if(listen(server_fd,3)<0){
        cerr<<"Listen failed"<<endl;
        return -1;
    }
    cout<<"Server is listening to port "<<PORT<<endl;

    vector<thread> threads;
    for(int i=0;i<THREAD_POOL_SIZE;i++){
        threads.emplace_back(worker_thread);
    }

    while(true){
        int socket_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if(socket_fd < 0){
            cerr<<"Accept failed"<<endl;
            continue;
        } 
        {
            lock_guard<mutex> lock(queue_mutex);
            task_queue.push(socket_fd);
        }
        condition.notify_one();
    }

    for(thread &t:threads){
        t.join();
    }

    close(server_fd);

    return 0;
}