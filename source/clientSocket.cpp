#include "library.h"

clientSocket::clientSocket() {
    int iResult;
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        cout << "WSAStartup failed with error: "  << iResult << endl;
        exit(225);
    }
}

clientSocket::~clientSocket() {
    free(recvbuf);
    closeConnection();

    // cleanup
    WSACleanup();
    cout << "destructured !" << endl;
}

void clientSocket::connectToServer(char *serverName) {
    cout << "connecting to server" << endl;

    int iResult;
    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;   
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(serverName, DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) {
        cout << "getaddrinfo failed with error: " << iResult << endl;
        WSACleanup();
        exit(225);
    }

    // Attempt to connect to an address until one succeeds
    for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            cout << "socket failed with error: " << WSAGetLastError() << endl;
            freeaddrinfo(result);
            WSACleanup();
            exit(225);
        }

        // set the socket to non-blocking
        // u_long iMode = 1;  // 0 = blocking mode, 1 = non-blocking mode
        // ioctlsocket(ConnectSocket, FIONBIO, &iMode); // set non-blocking mode so that the program will not hang

        // if (iResult != 0) {
        //     cout << "ioctlsocket failed with error: " << iResult << endl; 
        //     cout << "Error code : " << WSAGetLastError()  << endl;
        // }       

        // Connect to server.
        iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);
    freeaddrinfo(ptr);

    if (ConnectSocket == INVALID_SOCKET) {
        cout << " Unable to connect to server!\n";
        WSACleanup();
        exit(225);
    }
    cout << "Connected to server ! \n";
    
}

void clientSocket::handleRequest(char *host, char *path, char *fileName, char *folderName, char *dir) {
    cout << "handling request" << endl;

    // nếu là file 
    if (strlen(fileName) > 0) {
        // chỉnh tên file theo cấu trúc: host_fileName, lưu vào dir releases
        char *newFileName = createNewFName(fileName, host, dir);

        downloadFile(newFileName, host, path);

    } else {
        // nếu là folder: chỉnh tên folder theo cấu trúc: host_folderName, lưu vào dir releases
        char *newFolderName = createNewFName(folderName, host, dir);

        downloadFolder(newFolderName, host, path);
    }
}


char *clientSocket::createRequest(char *host, char *path) {
    cout << "creating request" << endl;

    // Send an initial buffer
    int iResult;
    char *request = (char *)malloc(1024);
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: %s\r\n\r\n", path, host, "keep-alive");
    return request;
}

int clientSocket::sendRequest(char *request) {
    cout << "sending request" << endl;

    int iResult;
    // Send an initial buffer
    iResult = send( ConnectSocket, request, (int)strlen(request), 0 );  
    if (iResult == SOCKET_ERROR) {
        int err = WSAGetLastError();
        handleErrorReceiving(err);
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    cout << "Bytes Sent: " << iResult << endl;
    return 0;
}

void clientSocket::handleErrorReceiving(int err) {

    cout << "handling error  ! " << endl;
    cout << "Error code: " << err << endl;

    switch (err) {
        case WSAECONNREFUSED:
            cout << "Error: connection refused !" << endl;
            cout << "Can call connect again for the same socket ! " << endl;
            break;
        case WSAETIMEDOUT:
            cout << "Error: connection timed out !" << endl;
            cout << "Can call connect again for the same socket ! " << endl;
            break;
        case WSAENETUNREACH:
            cout << "Error: network is unreachable !" << endl;
            cout << "Can call connect again for the same socket ! " << endl;
            break;
        case WSAEWOULDBLOCK:
            cout << "Error: resource temporarily unavailable !" << endl;
            break;
        case WSAENETDOWN:
            cout << "Error: network is down ! " << endl;
            break;
        case WSAENETRESET:
            cout << "Error: the connection has been broken due to keep-alive activity that detected a failure while the operation was in progress !" << endl;
            break;
        case WSAECONNABORTED:
            cout << "Error: software caused connection abort !" << endl;
            break;
        case WSAECONNRESET:
            cout << "Error: connection reset by peer !" << endl;
            break;
        case WSAENOBUFS:
            cout << "Error: no buffer space available !" << endl;
            break;
        case WSAENOTCONN:
            cout << "Error: socket is not connected !" << endl;
            break;
        case WSAESHUTDOWN:
            cout << "Error: cannot send after socket shutdown !" << endl;
            break;
        case WSAEHOSTDOWN:
            cout << "Error: host is down !" << endl;
            break;
        case WSAHOST_NOT_FOUND:
            cout << "Error: host not found !" << endl;
            break;
    }
}

void clientSocket::downloadFile(char *fileName, char *host, char *path) {

    cout << "downloading file" << endl;

    char *request;
    request = createRequest(host, path);
    sendRequest(request);

    cout << "Host: " <<  host << endl;
    cout << "File: " << fileName << endl;
    cout << "Request: " << request << endl;

    int iResult;
    memset(recvbuf, '\0', DEFAULT_BUFLEN);
    iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);


    if (iResult == 0 || iResult == SOCKET_ERROR) {
        int err = WSAGetLastError();
        handleErrorReceiving(err);
        if (err == WSAEWOULDBLOCK) {
            Sleep(100);
            iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
            if (iResult == 0 || iResult == SOCKET_ERROR) {
                cout << "Error receiving data !" << endl;
                closesocket(ConnectSocket);
                WSACleanup();
                exit(225);
            }
            
        } else if (err == WSAETIMEDOUT || err == WSAENETUNREACH || err == WSAECONNREFUSED) {
            cout << "Connect to server again ! " << endl;
            connectToServer(host);
            downloadFile(fileName, host, path);
            return;
        } else {
            closesocket(ConnectSocket);
            WSACleanup();
            exit(225);
        }
    }

    // nếu chưa nhận hết header thì tiếp tục nhận *****
    if (strstr(recvbuf, "\r\n\r\n") == NULL) {
        cout << "header not complete" << endl;
        cout << "header: " << recvbuf << endl;
        cout << "error: " << WSAGetLastError() << endl;

    }

    cout << "Bytes received: " <<  iResult << endl;

    // khai báo biến để lưu header, body
    char *header = (char *)malloc(DEFAULT_BUFLEN);
    memset(header, 0, DEFAULT_BUFLEN);
    char *body;

    // tách header và body
    splitResponse(recvbuf, header, body);
    iResult = iResult - (body - recvbuf);
    memmove(recvbuf, body, iResult);

    // cout << "Header: " << strlen(header) << endl;
    // cout << "header: " << header << endl;
    // cout << "recvbuf: " << recvbuf << endl;

    // nếu response header không phải là 200 OK thì thoát
    if (strstr(header, "200 OK") == NULL) {
        cout << "Response header is not 200 OK\n";
        exit(1);
    }

    // nếu header có cả transfer encoding và content length thì content length sẽ bị ignore
    // tìm trong header xem có chunked hay không
    if (strstr(header, "chunked") != NULL) {
        downloadFileChunked(fileName, iResult);
    } else {
        // tìm trong header để lấy size content
        char *contentLength = (char *) malloc(1024);
        memset(contentLength, 0, 1024);

         // tìm vị trí của content-length
        char *s = strstr(header, "Content-Length: ");
        if (s != NULL) {
            // tách content-length
            strcpy(contentLength, s + 16);
            // tìm vị trí của \r\n
            char *t = strstr(contentLength, "\r\n");
            if (t != NULL) {
                strncpy(contentLength, contentLength, t - contentLength);
            }
        } else {
            s = strstr(header, "content-length: ");
            if (s != NULL) {
                // tách content-length
                strcpy(contentLength, s + 16);
                // tìm vị trí của \r\n
                char *t = strstr(contentLength, "\r\n");
                if (t != NULL) {
                    strncpy(contentLength, contentLength, t - contentLength);
                }
            }
        }
        // Chuyển content-length từ string sang int
        int length = atoi(contentLength);

        downloadFileCLength(fileName, iResult, length);
    }
}

int clientSocket::downloadFileCLength( char *fileName, int iResult, int length) {
    cout << "downloading file with content length" << endl;

    FILE *f = fopen(fileName, "wb");
    if (f == NULL)
    {
        cout << "Error opening file!\n";
        exit(1);
    }

    // mở file để ghi
    fwrite(recvbuf, 1, iResult, f);
    length -= iResult;

    // tiếp tục nhận dữ liệu
    while (length > 0) {
        memset(recvbuf, '\0', DEFAULT_BUFLEN); // xóa dữ liệu trong buffer
        iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
        if (iResult == SOCKET_ERROR || iResult == 0) {
            int err= WSAGetLastError();
            cout << "recv failed with error: " << err << endl;
            if (err == WSAEWOULDBLOCK) {  // currently no data available
                Sleep(100);  // wait and try again: 100ms
                iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
            } else {
                return 225;
            }
        }
        fwrite(recvbuf, 1, iResult, f);
        length -= iResult;
    }

    // đóng file
    fclose(f);
        
    return 0;
    
}

int clientSocket::downloadFileChunked( char *fileName, int iResult) {
    cout << "downloading file with chunked" << endl;

    FILE *f = fopen(fileName, "wb");
    if (f == NULL)
    {
        cout << "Error opening file!\n";
        exit(1);
    }

    // khai báo biến lưu chunk size và con trỏ tới chunk data
    char *chunkSize = (char *)malloc(1024);
    memset(chunkSize, '\0', 1024);
    char *chunkData = (char *)malloc(DEFAULT_BUFLEN);
    memset(chunkData, '\0', DEFAULT_BUFLEN);

    int data = 0;
    bool crlfShortage = false;
    int size = 0;

    while (size != -1) {
        do {
            if (size == 0) {
                // cout << "DATA: " << recvbuf << endl;
                // Nếu data != 0 :
                if (data != 0) {
                    cout << "crlfShortage: " << crlfShortage << endl;
                    if (crlfShortage) {
                        if (data == 1 ) {
                            recvbuf += 1;
                            iResult -= 1;
                        } else {
                            recvbuf += 2;
                            iResult -= 2;
                        }
                        crlfShortage = false;
                    } else {
                        strcat(chunkData, recvbuf);
                        memmove(recvbuf, chunkData, iResult + data);
                        iResult += data;
                        data = 0;
                    }
                }

                // tìm vị trí của \r\n
                char *t = strstr(recvbuf, "\r\n");
                if (t != NULL) {
                    memset(chunkSize, '\0', 1024);
                    strncpy(chunkSize, recvbuf, t - recvbuf);

                    // bỏ qua chunk extension cách bởi dấu ; nếu có
                    char *s = strstr(chunkSize, ";");
                    if (s != NULL) {
                        strncpy(chunkSize, chunkSize, s - chunkSize);
                    }
                    size = strtol(chunkSize, NULL, 16);
                    if (size == 0) {
                        size = -1;
                        cout << "\nsize == -1\n";
                        cout << "chunkSize: " << chunkSize << endl;
                        break;
                    }
                    cout << "size = " << size << endl;

                    t += 2;
                    
                    iResult = iResult - (t - recvbuf);
                    memmove(recvbuf, t, iResult);

                    if (iResult >= size) {
                        fwrite(recvbuf, 1, size, f);
                        iResult = iResult - size;
                        if (iResult >= 2) {
                            iResult = iResult - 2;
                            memmove(recvbuf, recvbuf + size + 2, iResult);
                        } else {
                            crlfShortage = true;
                            data = iResult;
                            iResult = 0;
                        }
                        size = 0;
                    } else {
                        fwrite(recvbuf, 1, iResult, f);
                        size -= iResult;
                        iResult = 0;
                    }
                } else { // nếu size = 0 mà không tìm thấy \r\n
                    cout << "line 335" << endl;
                    memset(chunkData, '\0', DEFAULT_BUFLEN);
                    memmove(chunkData, recvbuf, iResult);
                    data = iResult;
                    break;
                }
            } else { // size > 0
                if (iResult >= size) {
                    fwrite(recvbuf, 1, size, f);
                    iResult = iResult - size;
                    if (iResult >= 2) {
                        iResult = iResult - 2;
                        memmove(recvbuf, recvbuf + size + 2, iResult);
                    } else {
                        crlfShortage = true;
                        data = iResult;
                        iResult = 0;
                    }
                    size = 0;
                } else {
                    fwrite(recvbuf, 1, iResult, f);
                    size -= iResult;
                    iResult = 0;
                }
            }

        } while (iResult > 0 ); 

        if (size != -1) {
            memset(recvbuf, '\0', DEFAULT_BUFLEN);
            iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);

            if (iResult == SOCKET_ERROR || iResult == 0) {
                int err= WSAGetLastError();
                cout << "recv failed with error: \n" << err << endl;
                if (err == WSAEWOULDBLOCK) {  // currently no data available
                    Sleep(100);  // wait and try again: 100ms
                    iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
                } else {
                    return 225;
                }
            }

        }
    }
    cout << "line 372" << endl;
    // đóng file
    fclose(f);
    
    // giải phóng bộ nhớ
    free(chunkSize);
    free(chunkData);

    cout << "line 382" << endl;
    return 225;   
}

int clientSocket::downloadFolder(char *folderName, char *host, char *path)
{
    cout << "downloading folder" << endl;

    // tạo thư mục
    if (mkdir(folderName) == -1) 
    {
        cout << "Error creating directory!\n";
        exit(1);
    }

    // tải file index.html
    char *fileName = (char *)malloc(1024);
    memset(fileName, '\0', 1024);
    strcpy(fileName, folderName);
    strcat(fileName, "\\index.html");

    downloadFile(fileName, host, path);

    // lọc các link trong file index.html và lưu vào vector link
    FILE *f = fopen(fileName, "r");
    if (f == NULL)
    {
        cout << "Error opening file! \n";
        exit(1);
    }

    char *line = (char *)malloc(1024);
    memset(line, '\0', 1024);
    char ** links = (char **)malloc(1024);
    int linkCount = 0;

    // tìm trong file index.html các link có dạng <a href="link">...</a>
    while (fgets(line, 1024, f) != NULL) {
        char *t = strstr(line, "<a href=\"");
        if (t != NULL) {
            t += 9;
            char *s = strstr(t, "\">");
            if (s != NULL) {
                char *linkName = (char *)malloc(1024);
                memset(linkName, '\0', 1024);
                strncpy(linkName, t, s - t);
                
                // nếu link là file
                if (strstr(linkName, ".") != NULL) {
                    links[linkCount] = (char *)malloc(1024);
                    memset(links[linkCount], '\0', 1024);
                    strcpy(links[linkCount], linkName);
                    linkCount++;
                    cout << "link " << linkCount <<": " << linkName << endl;
                }
            }
        }
    }

    // multiple requests cho các requests tải các file trong folder
    multipleRequest(links, linkCount, host, path, folderName);

    //  đóng file
    fclose(f);
    
    return 225;
}


int clientSocket::multipleRequest(char ** links, int linkCount, char *host, char *path, char *folderName) {
        cout << "multiple request" << endl;

        // tạo request đến các trang trong links để tải file
        for (int i = 0; i < linkCount; i++) {
            char *fileName = (char *)malloc(1024);
            memset(fileName, '\0', 1024);
            strcpy(fileName, folderName);
            strcat(fileName, "\\");
            strcat(fileName, links[i]);

            // tạo đường dẫn tới file
            char *newPath = (char *)malloc(1024);
            memset(newPath, '\0', 1024);
            strcpy(newPath, path);
            strcat(newPath, links[i]);
            
            // Tính thời gian tải file
            int start = clock();
            downloadFile(fileName, host, newPath);
            int end = clock();
            cout << "Time to download: " << 1000 * (end - start)/CLOCKS_PER_SEC << "ms" << endl;
        }
 
}

void clientSocket::closeConnection() {
    cout << "closing connection" << endl;

    // shutdown the connection 
    int iResult = shutdown(ConnectSocket, SD_BOTH);
    if (iResult == SOCKET_ERROR) {
        cout << "shutdown failed with error: \n" << WSAGetLastError() << endl;
    }

    // close socket
    closesocket(ConnectSocket);
}


