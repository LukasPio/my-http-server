# 🔌 C HTTP Server — User CRUD API

A lightweight HTTP server written in **pure C**, built from scratch using POSIX sockets. Supports full user management with persistent binary storage, request parsing, and XOR-based password encryption.

---

## Features

- Raw TCP socket server (no external HTTP libraries)
- JSON request parsing from raw HTTP bodies
- Route dispatching by path + method
- Persistent user storage via binary file (`users.bin`)
- XOR + salt password encryption
- Proper HTTP status codes and responses (`200`, `201`, `400`, `401`, `404`, `409`)

---

## Endpoints

| Method | Path          | Description                          |
|--------|---------------|--------------------------------------|
| GET    | `/`           | Health check / default response      |
| GET    | `/user`       | List all registered users (JSON)     |
| POST   | `/user`       | Register a new user                  |
| POST   | `/user/login` | Authenticate with email + password   |

---

## Request Format

All POST endpoints expect a JSON body:

```json
{
  "email": "user@example.com",
  "password": "yourpassword"
}
```

---

## Getting Started

### Prerequisites

- GCC or any C99-compatible compiler
- Linux / macOS (POSIX sockets required)

### Build & Run

```bash
gcc -o server main.c
./server
```

The server starts on `127.0.0.1:8080`.

### Example Requests

**Register a user:**
```bash
curl -X POST http://localhost:8080/user \
  -H "Content-Type: application/json" \
  -d '{"email":"alice@mail.com","password":"secret123"}'
```

**Login:**
```bash
curl -X POST http://localhost:8080/user/login \
  -H "Content-Type: application/json" \
  -d '{"email":"alice@mail.com","password":"secret123"}'
```

**List users:**
```bash
curl http://localhost:8080/user
```

---

## Project Structure

```
.
├── main.c        # Full server implementation
└── users.bin     # Auto-generated persistent user storage
```

---

## Implementation Details

### Password Encryption
Passwords are encrypted using a XOR cipher with a concatenated `SECRET + SALT` key before being stored. The same encryption is applied at login for comparison.

> ⚠️ This is a learning project. The XOR cipher used here is **not suitable for production**. For real applications, use a proper hashing algorithm like `bcrypt` or `argon2`.

### Route Dispatching
Routes are defined as an array of `Route` structs containing path, HTTP method, and a handler function pointer. Incoming requests are matched by both path and method.

### Persistence
Users are stored as raw binary structs in `users.bin` using `fwrite`/`fread`. The file is appended on each new registration and fully read when listing or authenticating users.

---

## Constraints & Limits

| Constant              | Value   |
|-----------------------|---------|
| Server Port           | `8080`  |
| Max Request Size      | `8192`  |
| Max Response Size     | `4096`  |
| Max Email Length      | `30`    |
| Max Password Length   | `100`   |

---

## License

MIT
