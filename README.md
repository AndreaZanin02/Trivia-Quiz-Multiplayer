# Trivia Quiz Multiplayer

## Overview
This is a client-server distributed application that implements a multiplayer trivia game. It was developed as a university project for the Computer Networks (Reti Informatiche) course at University of Pisa. 

## Architecture
The application is composed of two main modules:
* **Client Module:** Designed with low computational complexity to reach the widest possible audience. It manages the user experience, allowing players to register, select quiz themes, and answer questions.
* **Server Module:** Concentrates most of the computational complexity. It provides themes and questions, validates user answers, and organizes the leaderboards for various topics. The server is configured to listen on port 12345.

## Key Technical Features
* **Multithreading:** To handle a large number of concurrent clients, the server utilizes threads, avoiding the overhead associated with standard process context switching. When a new client connects, the server enters a loop and assigns a dedicated thread to manage that specific user's game experience.
* **Concurrency Control:** Mutexes (mutual exclusion semaphores) are implemented to allow multiple threads to safely access shared data structures without conflicts.
* **Dynamic Memory Management:** User and theme information is organized using linked lists. This prevents the need to pre-allocate memory for a hypothetical maximum number of users, saving resources if participation is lower. To avoid client-side delays, potentially slow list traversal operations are executed exclusively on the server.
* **Custom TCP Communication Protocol:** The application uses the TCP protocol to guarantee that packets arrive in order and without loss, which is critical since a single missing character would result in an incorrect quiz answer. Data exchange uses a custom text format where the length of the message is communicated first, followed by the actual payload. This approach handles variable-length messages without defining a standard packet size, thus preventing resource waste.
* **Robust Disconnection Handling:** A specific `ges Thread` data structure maintains information about the currently executing thread. This is crucial for handling forced disconnections: if a `SIGPIPE` signal arrives, the associated handler can safely terminate the user's session as if they had manually typed the `endquiz` command.
* **High Configurability:** The number of implemented themes, the listening port, and the IP address can be easily modified within the `Costanti.h` file. New questions can be added without modifying the code by simply placing correctly formatted `.txt` files in the corresponding folders (using the `&` character to separate questions and answers, ending with a carriage return). Maximum message lengths are also bounded to avoid creating unnecessarily large `char` arrays.

## Compilation and Execution

This project is designed specifically for **Linux/Unix-based systems**. If you are on Windows, it is highly recommended to use **WSL (Windows Subsystem for Linux)** to ensure compatibility with POSIX headers like `<arpa/inet.h>` and `<pthread.h>`.

### Prerequisites
* **GCC Compiler**
* **Make** utility
* **Pthread library** (usually included in `build-essential` on Ubuntu/WSL)

### How to Build
To compile both the server and the client, navigate to the project root and run:
```bash
make
