#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,        //解析请求行
        CHECK_STATE_HEADER,                 //解析请求头
        CHECK_STATE_CONTENT                 //解析消息体
    };
    enum HTTP_CODE
    {
        NO_REQUEST,                         //表示请求数据不完整,需要继续接收请求数据
        GET_REQUEST,                        //获得了完整的HTTP请求
        BAD_REQUEST,                        //HTTP请求报文有语法错误
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,                     //服务器内部错误，该结果在主状态机逻辑switch的default下，一般不会触发
        CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,                        //完整读取一行
        LINE_BAD,                           //报文语法有误
        LINE_OPEN                           //读取的行不完整
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result(connection_pool *connPool);
    int timer_flag;
    int improv;


private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; };
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
    int m_state;  //读为0, 写为1

private:
    int m_sockfd;
    sockaddr_in m_address;


    char m_read_buf[READ_BUFFER_SIZE];      //存储读取的请求报文数据
    int m_read_idx;         //m_read_buf中数据最后一个字节的下一个位置
    int m_checked_idx;      //m_read_buf读取的位置m_checked_idx
    int m_start_line;       //m_read_buf中已经解析的字符个数


    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;


    CHECK_STATE m_check_state;
    METHOD m_method;


    //存储文件的名称
    char m_real_file[FILENAME_LEN];
    char *m_url;        //链接网址
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;


    char *m_file_address;           //读取服务器上的文件地址
    struct stat m_file_stat;
    struct iovec m_iv[2];           //io向量机制iovec
    int m_iv_count;
    int cgi;                         //是否启用的POST
    char *m_string;                 //存储请求头数据
    int bytes_to_send;              //剩余发送字节数
    int bytes_have_send;            //已发送字节数

    char *doc_root;

    map<string, string> m_users;        //用于存储数据库中的用户账号和密码
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif
