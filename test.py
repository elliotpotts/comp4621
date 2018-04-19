import socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("localhost", 9999))
s.send(b"Hello\r\nWorld!\r\nTHer!");
s.close()