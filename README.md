===== THREADS AND CONCURRENCY =====

(void*) pointers:
void* refers to a generic pointer that can point to any data type, as pthread doesn't know what type of data will be passed.
void* cannot be used directly, it must first be cast to the data struct being passed:
request_args *args = (request_args *)arg;
create new structures to pass necessary information to thread functions, as only one argument can be created.

pthread_detach() vs pthread_join():
pthread_detach() releases resources immediately upon thread exit, no status collected
(use for short-lived worker threads in the server, "fire and forget", main thread doesn't care when they finish)

pthread_join() the main program waits for the completion of the worker thread before continuing, and retrieves the thread's return value
(use for long-lived listener and sender threads, as the main thread must wait for them to finish before exiting)

pthread_rwlock_rdlock - reader lock, any number of threads can acquire read lock, calling thread proceeds if lock is held by 1 or more reader thread, but blocks if lock is held by a writer thread

pthread_rwlock_wrlock - writer lock, exclusive, only one writer thread can hold it at a time, calling thread blocks if lock is held by any other thread until the lock is completely free

pthread_rwlock_unlock - releases lock. for read lock, decreases the reader count, but lock will remain read-locked until reader count is 0. if the write lock releases, the lock is completely free

===== SERVER =====

request_args structure (to be passed to each request worker thread)
1. socket descriptor (int)
2. client_addr (struct sockaddr_in)
3. request contents (char)

server maintains a 2 way linked list of clients, which is locked with the reader writer lock. multiple threads can read at once, but only one thread can write to it when no other thread is reading

standard request handling each time a worker thread is called:
1. receive the information in arg and cast to data struct request_args
2. call a helper function for the given request type
3. response buffer stores response to client
4. call udp_socket_write to send the response back to client stored within client_addr



