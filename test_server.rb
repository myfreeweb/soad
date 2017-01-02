require 'socket'

unless ENV['LISTEN_PID'].to_i == $$
  puts "Wrong LISTEN_PID: #{ENV['LISTEN_PID']}"
  exit 1
end

Socket.accept_loop(Socket.for_fd(3)) do |client_socket, addr|
  client_socket.send("Hi!\n", 0)
  while (data = client_socket.recv(1024)).size > 0
    puts "Client said #{data}"
    client_socket.send("You said #{data}", 0)
  end
end
