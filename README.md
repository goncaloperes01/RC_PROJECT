# C-Cord

**C-Cord** is a simplified, Discord-like client-server communication platform built in C. 
This project is being developed for the **Computer Networks (Redes de Computadores)** course at the University of Coimbra (2025/2026).

## Current State: Phase 2 (Command-Based Interaction)
Currently, the project has advanced to **Phase 2**. It maintains a **Multi-Client / Single-Server model** using **Blocking Sockets**, utilizing a strict request-response data flow to manage state, persistence, and basic message routing before transitioning to multiplexing in Phase 3.

### Implemented Features
* **F1 - User Registration:** Clients can register, but accounts are marked as 'pending' (state 0) by default.
* **F2 - Authentication:** Secure login. Access is granted only to users approved by an Administrator.
* **F3 - Echo & Info Retrieval:** `ECHO <msg>` and `GET_INFO` commands for testing the blocking data flow.
* **F4 - User Directory & Messaging:** Clients can retrieve a list of approved users (`LISTALL`). Messages can be sent to specific users (`SEND`) and are stored on the server until the recipient explicitly requests them (`CHECKINBOX`).
* **F5 - Admin Management (Approval):** New registrations require admin approval before they can authenticate.
* **F6 - Admin Management (Role):** The Admin user can bypass standard authentication. Once logged in, the Admin can approve pending accounts (`APPROVEUSER`) or permanently remove users from the system (`DELETEUSER`).

## 📂 Repository Structure
```text
.
├── ccord/                           # Phase 1: Project source code and build files
│   └── ...
├── ccord_f2/                        # Phase 2: Project source code and local database
│   ├── client.c                     # Updated Client-side source code
│   ├── server.c                     # Updated Server-side source code
│   ├── data/                        # Local database 
│   │   ├── users.txt                # Stores user credentials and approval state (0/1)
│   │   └── messages.txt             # Offline message storage (Sender | Receiver | Message)
│   └── Makefile                     # Makefile for Phase 2 compilation
├── docs/                            # Project documentation and planning
│   ├── CCord_Project_F1.pdf         # Phase 1 Project Report
│   ├── CCord_Project_F2.pdf         # Phase 2 Project Report
│   ├── gant_chart_final.pdf         # Project Gantt Chart
│   └── Project_Planning_EN.pdf      # Project Planning
└── README.md
```

## How to Compile and Run

### 1. Compile the Source Code
Navigate to the `ccord_f2` directory and compile the project:
```bash
cd ccord_f2
make
```
*(Alternatively, use: `gcc server.c -o server` and `gcc client.c -o client`)*

### 2. Run the Server
Start the server first. Ensure the `data` directory exists for the text files. The server listens on port 9000:
```bash
./server
```

### 3. Run the Client
Open a new terminal window, navigate to the `ccord_f2` folder, and start the client:
```bash
cd ccord_f2
./client
```
*(Follow the interactive terminal menu to Register, Login, and use the new F4/F5/F6 commands).*

## The Team
Following a team restructuring, this phase was developed by a 3-member team:
* **Team & Software Manager:** Gonçalo Peres (Architecture, Planning, Testing, Reporting)
* **Development Team:** Dinis Madeira & Gabriel da Cruz (C Implementation F4, F5, F6)

---
*Universidade de Coimbra - DEEC*
