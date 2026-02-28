import socket
from kafka import KafkaProducer

producer = KafkaProducer(bootstrap_servers='localhost:9092')

server_socket = socket.socket()
server_socket.bind(('0.0.0.0', 9093))
server_socket.listen()

while True:
    client_socket, addr = server_socket.accept()
    data = client_socket.recv(1024).decode()
    producer.send('sensor-data', data.encode())
    client_socket.close()