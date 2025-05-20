# Project Description

This project simulates the SIP-based IMS (IP Multimedia Subsystem) registration process using a set of C programs. It implements the communication between the IMS-Terminal (client) and four IMS core components: P-CSCF, I-CSCF, HSS, and S-CSCF, across five separate processes via TCP sockets.

The project supports:
- REGISTER message with SIP-compliant headers (per assignment spec 3.1)
- ACK timer mechanism with retransmission (max 3 retries)
- Content-Length validation at P-CSCF
- Dynamic 200 OK and 401 Unauthorized responses from HSS and S-CSCF
- User expiration countdown in S-CSCF

# Directory Structure
assignment1/
│ src/            # Source code (.c files)
│   │── client.c
│   │── pcscf.c
│   │── icscf.c
│   │── hss.c
│   └── scscf.c
│- bin/            # Compiled executables
│
│─ output/         # Optional: captured logs or response examples
└─ Makefile        # For compiling all modules

# Compilation

Make sure you have gcc installed. Run the following command from the root folder:
```ssh
make
```
This will compile all source files under src/ and place the executables in bin/.

# How to Run
1. Start servers (in this order):
```ssh
./bin/scscf
./bin/hss
./bin/icscf
./bin/pcscf
```
Each will listen on its respective port and wait for connections.

2. Run the client with a valid or invalid user ID:
```
./bin/client JohnMiller@university.edu        # Should return 200 OK
./bin/client InvalidUser@university.edu       # Should return 401 Unauthorized
```

# Input/Output Files

No external input files are required; all message formats are generated dynamically in code.
Example responses can be captured and stored under the output/ folder if desired.

# Notes
- Ports used: P-CSCF (6000), I-CSCF (5001), HSS (5002), S-CSCF (5003)
- Timeout for each REGISTER retransmission is 3 seconds (ACK timer)
- Valid users are hardcoded in hss.c
- Registration expiration is implemented via countdown threads in scscf.c


# Test
### HSS Server Unavailable (Retransmission Mechanism Validation)

**Objective:**
To verify that the IMS client correctly implements the ack_timer mechanism. When the HSS server is not running and no response is received, the client should automatically retry sending the REGISTER message up to 3 times with a 3-second timeout between attempts.

**Test Steps:**
1. Do not start the HSS server.
Ensure `./bin/hss` is not running.

2. Start the remaining IMS servers in order:
```
./bin/scscf
./bin/icscf
./bin/pcscf
```
3. Run the client with a valid user ID:
```
./bin/client JohnMiller@university.edu
```
4. Output:
![alt text](/assignment1/output/retransmission.png)

### Successful Registration — Receive 200 OK
**Objective:**
To verify that when the IMS-Terminal (client) sends a properly formatted SIP `REGISTER` request using a valid user ID, the system correctly processes the registration and returns a `200 OK` response with the required SIP headers

**Valid user:**
`JohnMiller@university.edu` (predefined in the `hss.c`database)
**Test Steps:**
1. Start all IMS server components
2. In a separate terminal, run the client with the valid user ID:
```
./bin/client JohnMiller@university.edu
```
3. Output
![alt text](/assignment1/output/200OK.png)


### Invalid User Registration — Receive 401 Unauthorized
**Objective:**
To verify that when the IMS-Terminal (client) sends a SIP REGISTER request using a user ID not found in the HSS database, the system returns a correctly formatted 401 Unauthorized response per the assignment slide (Section 3.3).
**Valid user:**
`FakeUser@university.edu` (not present in `valid_users[]` in the `hss.c`database)
**Test Steps:**
1. Start all IMS server components
2. In a separate terminal, run the client using an invalid user:
```
./bin/client FakeUser@university.edu
```
3. Output
![alt text](/assignment1/output/401.png)


### Registration Expiration with Short Expires Value
**Objective:**
To verify that the registration entry stored in the S-CSCF expires and is automatically removed after the number of seconds specified in the `Expires`: field of the SIP REGISTER message.
**Valid user:**
`JohnMiller@university.edu` (predefined in the `hss.c`database)
**Test Adjustment:**
For testing purposes, the Expires: value in the client’s REGISTER message was changed from 3600 to 20 seconds to allow faster validation of the expiration mechanism.
**Test Steps:**
1. Start all IMS servers
2. Run the client:
```
./bin/client JohnMiller@university.edu
```
3. Output
![alt text](/assignment1/output/expiration.png)
