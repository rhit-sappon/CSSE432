import socket
import sys
import threading

def server_thread(client_sock, client_num):
    while(1):
        data = client_sock.recv(1024).decode()
        if not data: #times out or connection closed
            break
        print("Data from client", client_num, ": ", str(data))
        data = str(data).upper
        client_sock.send(data.encode())
    client_sock.close()

def server_program():
    host = socket.gethostname()
    host_ip = socket.gethostbyname(host)
    if(len(sys.argv)!=2):
        print("Please enter a port number as argument")
        sys.exit()
    port = int(sys.argv[1])

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind(('',port))
    server_socket.listen(5) #Up to 5 queue clients 
    while(1):
        connected_socket, address = server_socket.accept() #halts until client attempts to connect, will not continue while loop
        print("Connection made from ", str(address))
        t = threading.Thread(server_thread,args=(connected_socket,client_num,))
        t.start()
        client_num += 1



if __name__ == '__main__':
    server_program()