import SocketServer

class TCPHandler(SocketServer.BaseRequestHandler):

    def handler(self):
        self.data = self.request.recv(10a
        self.request.sendall(self.data.upper())
if __name__ == "__main__":
    HOST, PORT = "localhost", 60001

    # Creating server to bind to port 60001
    server = SocketServer.TCPServer((HOST, PORT), TCPHandler)

    # Activate server: run until interrupted by keyboard
    server.serve_forever()

