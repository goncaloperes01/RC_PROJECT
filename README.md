# C-Cord 

**C-Cord** is a simplified, Discord-like client-server communication platform built in C. 
This project is being developed for the **Computer Networks (Redes de Computadores)** course at the University of Coimbra (2025/2026).

## Current State: Phase 1 (Blocking Sockets)
Currently, the project is in **Phase 1**, utilizing a **Sequential Server** architecture with a **Stop-and-Wait** model based on TCP Blocking Sockets. The server handles one client request at a time.

### Implemented Features
* **F1 - User Registration:** Clients can register. Credentials are saved persistently in a local database (`data/`).
* **F2 - Authentication:** Secure login verifying the username and password against the database.
* **F3 - Echo & Info Retrieval:** Post-login commands such as `ECHO <message>` to receive the same string back, and `GET_INFO` to retrieve server status.

## Repository Structure
```text
.
├── ccord/                           # Project source code and build files
│   ├── client/                      # Client-side source code
│   ├── server/                      # Server-side source code
│   ├── data/                        # Local database for user credentials
│   ├── .vscode/                     # VS Code configuration files
│   ├── Estrutura_CCord              # Project structure notes
│   └── Makefile                     # Makefile for easy compilation
├── docs/                            # Project documentation and planning
│   ├── CCord_Project_F1.pdf         # Phase 1 Project Report
│   ├── gant_chart_final.pdf         # Project Gantt Chart
│   └── Project_Planning_EN.pdf   # Phase 1 Project Planning
└── README.md
```

## How to Compile and Run

### 1. Compile the Source Code
Navigate to the `ccord` directory and compile the project using the provided `makefile`:
```bash
cd ccord
make
```

### 2. Run the Server
Start the server first. The server will listen for incoming client connections:
```bash
./server/server
```
*(Note: Adjust the executable path if your makefile outputs the binary to a different folder).*

### 3. Run the Client
Open a new terminal window, navigate to the `ccord` folder, and start the client:
```bash
cd ccord
./client/client
```
*(Follow the interactive terminal menu to Register, Login, and send commands).*

## The Team
This project is being developed by a 7-member team:
* **Team Manager:** Gonçalo Peres
* **Account Manager:** João Cascão
* **Software Manager:** Gonçalo Peres
* **Risk and Testing Manager:** Angeles de Jauregui
* **Quality Manager:** Flavia Salta
* **Development Team:** Dinis Madeira & Gabriel da Cruz

---
*University of Coimbra - DEEC*
