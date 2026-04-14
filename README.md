# C-Cord 💬

**C-Cord** is a simplified, Discord-like client-server communication platform built in C. 
This project is being developed for the **Computer Networks (Redes de Computadores)** course at the University of Coimbra (2025/2026).

## 📌 Current State: Phase 1 (Blocking Sockets)
Currently, the project is in **Phase 1**, utilizing a **Sequential Server** architecture with a **Stop-and-Wait** model based on TCP Blocking Sockets. The server handles one client request at a time.

### 🚀 Implemented Features
* **F1 - User Registration:** Clients can register. Credentials are saved persistently in a local database (`data/users.txt`).
* **F2 - Authentication:** Secure login verifying the username and password against the database.
* **F3 - Echo & Info Retrieval:** Post-login commands such as `ECHO <message>` to receive the same string back, and `GET_INFO` to retrieve server status.

## 📂 Repository Structure
```text
.
├── src/
│   ├── server.c         # Server-side source code
│   └── client.c         # Client-side source code
├── data/
│   └── users.txt        # Local database for user credentials
├── .vscode/             # VS Code configuration files
│   ├── launch.json
│   ├── settings.json
│   └── c_cpp_properties.json
├── docs/                # Project documentation and reports
│   └── relatorio_fase1.pdf
└── README.md
```

## ⚙️ How to Compile and Run

### 1. Compile the Source Code
Navigate to the root directory and compile the server and client programs using `gcc`:
```bash
gcc src/server.c -o server
gcc src/client.c -o client
```

### 2. Run the Server
Start the server first. Ensure the `data` directory exists for the `users.txt` file. The server will listen on port 9000:
```bash
./server
```

### 3. Run the Client
Open a new terminal window and start the client:
```bash
./client
```
*(Follow the interactive terminal menu to Register, Login, and send commands).*

## 👥 The Team
This project is being developed by a 7-member team:
* **Team Manager:** Gonçalo Peres
* **Account Manager:** João Cascão
* **Software Manager:** Gonçalo Peres
* **Risk and Testing Manager:** Angeles de Jauregui
* **Quality Manager:** Flavia Salta
* **Development Team:** Dinis Madeira & Gabriel da Cruz

---
*University of Coimbra - DEEC*
