import socket
import sys


def client_program():
    if(len(sys.argv)!=3):
        print("Please enter a address and a port number as argument")
        sys.exit()
    port = int(sys.argv[2])
    server_ip = socket.gethostbyname(sys.argv[1])
    server_addr = (server_ip,port)
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect(server_addr)

    message = input(" -> ")
    while(message.lower().strip() != ";;;"): #client enters ;;; to end connection
        client_socket.send(message.encode())
        in_data = client_socket.recv(1024).decode()
        print("Recieved from server:", str(in_data))
        message = input(" -> ")

    client_socket.close()


if __name__ == '__main__':
    client_program()