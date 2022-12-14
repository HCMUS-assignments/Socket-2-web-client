#include "library.h"

clientSocket::clientSocket() {
    // cấp phát bộ nhớ 
    recvbuf = (char *)malloc(recvbuflen);
    if (recvbuf == NULL) {
        cout << "\nError: allocating memory" << endl;
        exit(1);    
    }

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        cout << "\nError: WSAStartup failed with error: "  << iResult << endl;
        exit(1);
    }
}

clientSocket::~clientSocket() {
    free(serverName);
    free(recvbuf);
    closeConnection();

    // cleanup
    WSACleanup();
    // cout << "destructured !" << endl;
}

void clientSocket::getServerName(char *host) {
    strcpy(serverName, host);
}

bool clientSocket::connectToServer() {
    // cout << "connecting to server" << endl;

    ConnectSocket = INVALID_SOCKET;

    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;    
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP; 

    // Resolve the server address and port
    iResult = getaddrinfo(serverName, DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) {

        int error = WSAGetLastError();

        if (error == WSAHOST_NOT_FOUND) {
            cout << "\nError: host not found" << endl;
        } else if (error == WSANO_DATA) {
            cout << "\nError: data record found" << endl;
        } else {
            cout << "\nError: getaddrinfo failed with error: " << error << endl;
        }
        WSACleanup();
        return 0;
    }

    // Attempt to connect to an address until one succeeds
    for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            cout << "\nError: create socket failed with error: " << WSAGetLastError() << endl;
            freeaddrinfo(result);
            WSACleanup();
            return 0;
        }  

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

    if (ConnectSocket == INVALID_SOCKET) {
        cout << "\nError: unable to connect to server!\n";
        WSACleanup();
        return 0;
    }

    // Đóng socket nếu client hoặc server mất kết nối đột ngột
    int nTimeout = 30000; // 30 seconds
    setsockopt(ConnectSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&nTimeout, sizeof(int));

    return 1;
    // cout << "Connected to server ! \n"; 
    
}

bool clientSocket::handleRequest(char *path, char *fileName, char *folderName, char *dir) {
    // cout << "handling request" << endl;
    textcolor(13);
    cout << "\nLoading ..." << endl;

    textcolor(12);

    // nếu là file 
    if (strlen(fileName) > 0) {
        // chỉnh tên file theo cấu trúc: host_fileName, lưu vào dir releases
        char *newFileName = (char *) malloc (100);
        createNewFName(fileName, serverName, dir, newFileName);

        clock_t start, end;
        start = clock();
        if (downloadFile(newFileName, path)) {
            end = clock();
            char *noctice = (char *) malloc (100);
            textcolor(10);
            sprintf(noctice, "\nDownloaded %s_%s in %.1f ms\n", serverName,fileName, (double)(end - start) / CLOCKS_PER_SEC * 1000);
            cout << noctice;
            textcolor(12);
            free(newFileName);
            return 1;
        } 
    } else {
        // nếu là folder: chỉnh tên folder theo cấu trúc: host_folderName, lưu vào dir releases
        char *newFolderName = (char *) malloc(100);
        createNewFName(folderName, serverName, dir, newFolderName);

        if (downloadFolder(newFolderName, path)) {
            free(newFolderName);
            return 1;
        }

    }
    return 0;
}


void clientSocket::createRequest( char *path, char *request) {
    // cout << "creating request" << endl;

    // Send an initial buffer
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: %s\r\n\r\n", path, serverName, "Keep-Alive");
}

bool clientSocket::sendRequest(char *request) {
    // cout << "sending request" << endl;

    // Send an initial buffer
    iResult = send( ConnectSocket, request, (int)strlen(request), 0 );  
    if (iResult == SOCKET_ERROR) {
        int err = WSAGetLastError();
        handleError(err);
        closesocket(ConnectSocket);
        WSACleanup();
        return 0;
    }
    return 1;
}

void clientSocket::handleError(int err) {

    // cout << "handling error  ! " << endl;
    textcolor(12);
    if (err != 0) {
        cout << "Error code: " << err << endl;
    }

    switch (err) {
        case WSAECONNREFUSED:
            cout << "Error: connection refused !" << endl;
            break;
        case WSAETIMEDOUT:
            cout << "Error: connection timed out !" << endl;
            break;
        case WSAENETUNREACH:
            cout << "Error: network is unreachable !" << endl;
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

bool clientSocket::downloadFile(char *fileName, char *path) {

    // tạo request và gửi đi
    char *request = (char *) malloc(1000);
    createRequest( path, request);
    sendRequest(request);

    memset(recvbuf, '\0', recvbuflen);
    iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);

    if (iResult <= 0) {
        int err = WSAGetLastError();
        handleError(err);
        if (iResult == 0) {
            // thử kết nối lại
            closeConnection();
            // cout << "Connect to server again ! " << endl;
            connectToServer();
            return downloadFile(fileName, path);
        }
    }

    // nếu chưa nhận hết header thì tiếp tục nhận *****
    if (strstr(recvbuf, "\r\n\r\n") == NULL) {
        // nhận tiếp vào temp và nối vào recvbuf
        char *temp = new char [recvbuflen];

        // tiếp tục nhận đến khi xong header
        while (strstr(recvbuf, "\r\n\r\n") == NULL) {
            int recvBytes = recv(ConnectSocket, temp, recvbuflen, 0);
            if (recvBytes <= 0) {
                int err = WSAGetLastError();
                handleError(err);
                if (recvBytes == 0) {
                    // Thử kết nối lại
                    closeConnection();
                    connectToServer();
                    return downloadFile(fileName, path);
                }
            }
            // lưu lại số bytes nhận được hỗ trợ phần tách header phía sau
            iResult += recvBytes;

            // cấp phát thêm bộ nhớ cho recvbuf để chứa dữ liệu mới nhận được
            recvbuf = (char *)realloc(recvbuf, iResult + 1);

            // nối dữ liệu mới nhận được vào recvbuf
            memmove(recvbuf + iResult - recvBytes, temp, recvBytes);

            // khởi tạo lại temp
            memset(temp, '\0', recvbuflen);
        }
        delete [] temp;
    }

    // khai báo biến để lưu header, body
    char *header ;
    char *body;

    // tách header và body
    splitResponse(recvbuf, header, body);
    int headerLength = body - recvbuf;
    iResult = iResult - headerLength;
    memmove(recvbuf, recvbuf + headerLength, iResult);

    // nếu response header không phải là 200 OK thì thoát
    if (strstr(header, "200 OK") == NULL) {
        // cout << "Response header is not 200 OK\n";
        cout << endl << header << endl;
        closeConnection();
        return 0;
    }

    // nếu response có connection close thì nhận xong đóng kết nối (áp dụng trong multi-request)
    if (strstr(header, "Connection: close") != NULL) {
        isKeepAlive = false;
    } else {
        isKeepAlive = true;
    }

    bool success = 1;

    // nếu header có cả transfer encoding và content length thì content length sẽ bị ignore
    // tìm trong header xem có chunked hay không
    if (strstr(header, "chunked") != NULL) {
        success = downloadFileChunked(fileName);
    } else {
        // tìm trong header để lấy size content
        char *contentLength = (char *) malloc (100);
        memset(contentLength, '\0', 100);

         // tìm vị trí của content-length
        char *s = strstr(header, "Content-Length: ");
        if (s != NULL) {
            // tách content-length
            strcpy(contentLength, s + 16);
            // tìm vị trí của \r\n
            char *t = strstr(contentLength, "\r\n");
            if (t != NULL) {
                *t = '\0';
            }
        } else {
            s = strstr(header, "content-length: ");
            if (s != NULL) {
                // tách content-length
                strcpy(contentLength, s + 16);
                // tìm vị trí của \r\n
                char *t = strstr(contentLength, "\r\n");
                if (t != NULL) {
                    *t = '\0';
                }
            }
        }
        // Chuyển content-length từ string sang int
        int length = atoi(contentLength);

        // giải phóng bộ nhớ
        free(contentLength);

        success = downloadFileCLength(fileName, length);
    }

    // giải phóng bộ nhớ
    free(header);
    free(request);

    if (success) {
        return 1;
    } else {
        return 0;
    }
}

bool clientSocket::downloadFileCLength( char *fileName, int length) {
    // cout << "downloading file with content length" << endl;

    FILE *f = fopen(fileName, "wb");
    if (f == NULL)
    {
        cout << "Error: opening file!\n";
        return 0;
    }

    // mở file để ghi
    fwrite(recvbuf, 1, iResult, f);
    length -= iResult;

    // tiếp tục nhận dữ liệu
    while (length > 0) {
        memset(recvbuf, '\0', recvbuflen); // xóa dữ liệu trong buffer
        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult <= 0) {
            int err= WSAGetLastError();
            handleError(err);
            return 0;
        }
        fwrite(recvbuf, 1, iResult, f);
        length -= iResult;

    }

    // đóng file
    fclose(f);

    return 1;
        
}

bool clientSocket::downloadFileChunked( char *fileName) {
    // cout << "downloading file with chunked" << endl;

    FILE *f = fopen(fileName, "wb");
    if (f == NULL)
    {
        cout << "Error: opening file!\n";
        return 0;
    }

    // biến chunksize để lưu size dạng chuỗi hexa của chunk
    char *chunkSize = (char *)malloc(1000);
    memset(chunkSize, '\0', 1000);

    // biến chunkdata để lưu dữ liệu thừa ở lần ghi file trước đó nếu có
    char *chunkData = (char *)malloc(1000);
    memset(chunkData, '\0', 1000);

    if (chunkData == NULL || chunkSize == NULL) {
        cout << "Error: malloc failed" << endl;
        return 0;
    }

    // số byte thừa trong chunkData
    int data = 0;

    // trường hợp ghi đủ chunk data nhưng chưa tìm thấy kí tự kết thúc chunk CRLF -> lưu lại để kiểm tra
    bool crlfShortage = false;

    // biến lưu trữ số bytes chunk cần ghi vào file
    int size = 0;

    while (size != -1) {
        do {
            if (size == 0) {  // đọc chunk size
                // Nếu data != 0 thì xảy ra 2 trường hợp:
                if (data != 0) {
                    // TH1: thiếu \r\n ở cuối chunk data trước đó
                    if (crlfShortage) {
                        iResult -= data;
                        memmove(recvbuf, recvbuf + data, iResult);
                        crlfShortage = false;
                        data = 0;

                    } else { 
                        // TH2: chưa tìm thấy cuối chuỗi chunk size ở lần đọc trước

                        // cấp phát thêm bộ nhớ cho chunk data lưu dữ liệu đọc được
                        chunkData = (char *)realloc(chunkData, data + iResult);
                        strncpy(chunkData + data, recvbuf, iResult);

                        // cấp phát thêm bộ nhớ cho recvbuf
                        recvbuf = (char *)realloc(recvbuf, recvbuflen + iResult);

                        // di chuyển dữ liệu sang 
                        memmove(recvbuf, chunkData, iResult + data);
                        iResult += data;
                        data = 0;
                        memset(chunkData, '\0', 1000);
                    }
                }

                // tìm vị trí của \r\n để tách chuỗi chunk size
                char *t = strstr(recvbuf, "\r\n");
                if (t != NULL) {
                    memset(chunkSize, '\0', 1000);
                    strncpy(chunkSize, recvbuf, t - recvbuf);

                    // bỏ qua chunk extension cách bởi dấu ; nếu có
                    char *s = strstr(chunkSize, ";");
                    if (s != NULL) {
                        strncpy(chunkSize, chunkSize, s - chunkSize);
                    }

                    // chuyển chunk size từ hex sang dec
                    size = strtol(chunkSize, NULL, 16);
                    if (size == 0) {
                        size = -1;
                        break;
                    }

                    // trở đến vị trí đầu tiên của chunk data
                    t += 2;
                    
                    // cập nhật lại iResult
                    iResult = iResult - (t - recvbuf);
                    memmove(recvbuf, t, iResult);
                    
                    // nếu iResult >= size thì ghi file và cập nhật lại size
                    if (iResult >= size) {
                        fwrite(recvbuf, 1, size, f);
                        iResult = iResult - size;

                        // nếu ghi đủ chunk data nhưng thiếu \r\n thì cập nhật lại crlfShortage
                        if (iResult >= 2) {
                            iResult = iResult - 2;
                            memmove(recvbuf, recvbuf + size + 2, iResult);
                            memset(recvbuf + iResult, '\0', recvbuflen - iResult);
                        } else {
                            crlfShortage = true;
                            data = iResult;     // lưu lại số byte còn (0 <= data <= 1 ) => CRLF thiếu (1 hoặc 2)
                            iResult = 0;
                        }
                        size = 0;
                    } else {
                        // nếu iResult < size thì ghi file số bytes lấy được và cập nhật lại size
                        fwrite(recvbuf, 1, iResult, f);
                        size -= iResult;
                        iResult = 0;
                    }
                } else { // nếu size = 0 mà chưa tìm thấy \r\n trong recvbuf để tách chunk size thì nhận thêm dữ liệu
                    memmove(chunkData, recvbuf, iResult);
                    data = iResult;
                    iResult = 0;
                }
            } else { // size > 0 : tức là chunk data còn thiếu
                if (iResult >= size) {
                    fwrite(recvbuf, 1, size, f);
                    iResult = iResult - size;
                    if (iResult >= 2) {
                        iResult = iResult - 2;
                        memmove(recvbuf, recvbuf + size + 2, iResult);
                        memset(recvbuf + iResult, '\0', recvbuflen - iResult);
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
            memset(recvbuf, '\0', recvbuflen);
            iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);

            if (iResult <= 0) {
                int err= WSAGetLastError();
                handleError(err);
                free(chunkData);
                free(chunkSize);
                return 0;
            }

        }
    }

    // đóng file
    fclose(f);
    
    // giải phóng bộ nhớ
    free(chunkSize);
    free(chunkData);

    return 1;

}

bool clientSocket::downloadFolder(char *folderName, char *path) {
    // cout << "downloading folder" << endl;

    // tạo thư mục
    mkdir(folderName);

    // tải file index.html
    char *fileName = (char *)malloc(1024);
    memset(fileName, '\0', 1024);
    strcpy(fileName, folderName);
    strcat(fileName, "\\index.html");

    if (!downloadFile(fileName, path)) return 0;

    // lọc các link trong file index.html và lưu vào vector link
    FILE *f = fopen(fileName, "r");
    if (f == NULL)
    {
        cout << "Error opening file! \n";
        return 0;
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
                }
            }
        }
    }

    // multiple requests cho các requests tải các file trong folder
    if (multipleRequest(links, linkCount, path, folderName)) {
        //  đóng file
        fclose(f);   
        return 1;
    } else {
        //  đóng file
        fclose(f);
        return 0;
    }
}


bool clientSocket::multipleRequest(char ** links, int linkCount, char *path, char *folderName) {
        // cout << "multiple request" << endl;
        double sumTime = 0;
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
            bool success = downloadFile(fileName, newPath);
            if (success == 0) {
                cout << "\nTải file thất bại !\n";
                return 0;
            }
            int end = clock();
            double time = (double)1000 * (end - start) / CLOCKS_PER_SEC;
            sumTime += time;

            // nếu response header không cho keep alive thì đóng kết nối, mở kết nối mới
            if (isKeepAlive == false) {
                closesocket(ConnectSocket);
                connectToServer();
            }
        }

        // tạo tên folder bỏ thư mục release để hiển thị ra màn hình console
        char *noticeStr = (char *)malloc(1024);
        char *folderName2 = (char *)malloc(1024);
        memset(folderName2, '\0', 1024);
        strcpy(folderName2, folderName + 9);
        sprintf(noticeStr, "\nDownloaded %d file of folder %s in %.1f ms", linkCount + 1, folderName2, sumTime);
        textcolor(10);
        cout << noticeStr << endl;
        textcolor(12);

        // giải phóng bộ nhớ
        free(noticeStr);
        free(folderName2);
        return 1;
}

void clientSocket::closeConnection() {
    // cout << "closing connection" << endl;

    // shutdown the connection 
    iResult = shutdown(ConnectSocket, SD_BOTH);
    if (iResult == SOCKET_ERROR) {
        int err= WSAGetLastError();
        if (err != 10038)
            cout << "Error code: " << WSAGetLastError() << endl;
    }

    // close socket
    closesocket(ConnectSocket);
}


