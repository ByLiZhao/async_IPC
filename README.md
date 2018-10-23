# async_IPC
An easy non-blocking IPC library (single-user-single-client) built upon boost asio's asynchronous TCP facility.
The lib is written in favor of ease of usage, that one can simply use it wihout worrying about
1. asynchronous IO.
2. TCP communication
3. liftetime management of underlying objects.
4. thread saftey.
The server and the client are communicating in terms of messages, the lib guarantee that
1. A message is either delivered or dropped (in case of unexpected disconnection), no half baked message is sent.
2. Multiple threads can access the message quene without extra work of synchronization.
3. If there is unread message in the incoming message queue, it can be read enven after the underlying TCP connect has been lost.

