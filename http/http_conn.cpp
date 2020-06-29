#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";     //服务器内部错误
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string, string> users;

//初始化数据库读表:先从数据库中读取用户名和密码放入map中
void http_conn::initmysql_result(connection_pool *connPool)
{
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
    auto it = users.begin();
    while(it != users.end()){
        //cout<< "用户名:  "<< it->first <<"密码: "<< it->second<<endl;
        it++;
    }
}



//对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;


    //边沿触发
    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    //开启oneshot模式
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}










int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

//关闭连接，关闭一个连接，客户总量减一
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        //printf("close %d\n", m_sockfd);  //关闭连接
        removefd(m_epollfd, m_sockfd); //从内核时间表删除描述符
        m_sockfd = -1;
        m_user_count--;
    }
}



//初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;

    //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;        //在这里进行赋值了
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

//初始化新接受的连接
//check_state默认为分析请求行状态
void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}





//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    //LT读取数据
    if (0 == m_TRIGMode)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        m_read_idx += bytes_read;

        if (bytes_read <= 0)
        {
            return false;
        }

        return true;
    }
    //ET读数据
    else
    {
        while (true)
        {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if (bytes_read == 0)
            {
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}




//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {

            if ((m_checked_idx + 1) == m_read_idx)      //没有读取到完整的一行,只读到了/r没有读到/n
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n') //读取到了完整的一行
            {
                //将每行数据末尾的\r\n重置为\0\0
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;         //一行读取完成
            }
            return LINE_BAD;        //表示语法错误
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}








//解析http请求行，获得请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{

    //在HTTP报文中，请求行用来说明请求类型,要访问的资源以及所使用的HTTP版本，其中各个部分之间通过\t或空格分隔。
    //请求行中最先含有空格和\t任一字符的位置并返回
    m_url = strpbrk(text, " \t");       //strpbrk检测两个字符串首个相同字符的位置
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)  {
        m_method = GET;
        //cout << "HTTP的请求方法是GET" <<endl;
    }

    else if (strcasecmp(method, "POST") == 0)
    {
        //cout << "HTTP的请求方法是POST" <<endl;
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST; //请求资源错误

    //继续跳过请求方法后面的空格  \t
    m_url += strspn(m_url, " \t");  //在m_url中匹配\t返回匹配长度

    //只支持HTTP1.1
    m_version = strpbrk(m_url, " \t"); //strpbrk检测两个字符串首个相同字符的位置 这个\t是HTTP/1.1前面的
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;


    //处理 请求资源 看看请求资源中有没有http://
    if (strncasecmp(m_url, "http://", 7) == 0)  //字符串相同返回0,如果s1大于s2返回大于0,s1小于s2返回小于0
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

   //处理请求资源,看看请求资源中有没有https://
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;

    //处理请求资源,请求资源的首个字母是/
    //当url为/时，显示判断界面
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html"); //将字符串"judge.html"连接到m_url后面
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;      //请求行接收完毕后,还要接收请求头部
}



//解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    //printf("%s\n",text);
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT; //解析头部
            return NO_REQUEST;      //支持长连接
        }
        return GET_REQUEST;
    }

    //匹配连接方式:长连接还是短连接
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;   //长连接
        }
    }
    //匹配
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");

        m_content_length = atol(text);
        //cout << "content_length的长度是: "  << m_content_length <<endl;
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;      //主机地址
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

//判断http请求是否被完整读入
//服务器端解析浏览器的请求报文,当解析为POST请求时,cgi标志设置为1,并将请求报文的消息体赋给m_string,进而提取出用户名和密码
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    //判断buffer中是否读取了消息体
    if (m_read_idx >= (m_content_length + m_checked_idx)) //说明正确包含了请求消息体
    {
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        //printf("%s\n",text);
        return GET_REQUEST;
    }
    return NO_REQUEST;
}




//从m_read_buf中读取,并处理请求报文 主状态机读取
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    //主状态机解析消息体切从状态为完整读取一行    或者      从状态机转移到LINE_OK,该条件涉及解析请求行和请求头部,```````````````````````并且完整读取一行
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        //返回的是一个char*字符串,m_read_buf+m_start_line,一次性读取
        text = get_line();      //从状态机读取数据,调用get_Line函数,通过m_stat_line将从状态机读取的数据间接赋给text
        m_start_line = m_checked_idx;



        //cout << "读取一行数据" <<endl;
        LOG_INFO("%s", text);

        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:  //状态机:处理完请求行,转向请求头部
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;

            //cout << "请求行处理完成" << endl;
            break;

        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                //cout << "开始处理请求" <<endl;
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;            //在完成消息体的解析之后,让Line_status修改为LINE_OPEN,以跳出循环
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;      //继续请求数据
}



http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root);  //将doc_root(存放文件的地址)放入其中
    //printf("%s\n",m_real_file);

    int len = strlen(doc_root);
    //printf("m_url:%s\n", m_url);
    const char *p = strrchr(m_url, '/'); //在m_url中查找字符'/'首次出现的位置

    //处理cgi post请求: 2是2CGISQL.cgi,要登录
    //3是3CGISQL,要注册
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来

        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i]; //用户注册的用户名
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i]; //用户注册的密码
        password[j] = '\0';

        //用户填写了用户名和密码之后,点击了注册
        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES("); //往数据库里插入新的用户名和密码
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            //在map中查找
            if (users.find(name) == users.end())
            {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert); //如果数据库中还没有建立这条连接,就创建一个,即向数据库中插入
                users.insert(pair<string, string>(name, password)); //还在内存中插入了
                m_lock.unlock();

                if (!res) {
                    //cout << "用户注册成功"<<endl;
                    strcpy(m_url, "/log.html");  //如果注册成功就跳转登录页面
                }

                else {
                     //cout << "用户注册失败"<<endl;
                     strcpy(m_url, "/registerError.html");  //注册失败就跳转注册失败页面
                    }

            }
            else {
                //cout << "用户名已经存在了" <<endl;
                strcpy(m_url, "/registerError.html");

            }

        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password) {
                //cout << "用户登录成功,发送欢迎页面"<<endl;
                strcpy(m_url, "/welcome.html");
            }


            else {
                //cout << "用户登录失败" <<endl;
                strcpy(m_url, "/logError.html");
            }

        }
    }

    if (*(p + 1) == '0') //新用户是0
    {
        //cout << "向用户发送注册页面" <<endl;
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }

    else if (*(p + 1) == '1')
    {
        //cout << "向用户发送登录页面" <<endl;
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }

    //请求的是图片
    else if (*(p + 1) == '5')
    {
        //cout <<"用户请求图片" <<endl;
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //请求的是视频
    else if (*(p + 1) == '6')
    {
        //cout <<"用户请求视频" <<endl;
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //请求的简历
    else if (*(p + 1) == '7')
    {

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/index.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else {
        //cout << "欢迎页面" <<endl;
         strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1); //把src所指向的字符串中以src地址开始的前n个字节复制到dest所指的数组中，并返回dest
         //printf("%s\n",m_real_file);
    }




    //返回没有资源的错误
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

     //判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
    if (!(m_file_stat.st_mode & S_IROTH))       //用户具有可读去权限
        return FORBIDDEN_REQUEST;

    //返回错误请求
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;


    int fd = open(m_real_file, O_RDONLY); //将文件映射到
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}



void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}


bool http_conn::write()
{
    int temp = 0;

    //需要发送的数据长度为0
    //表示响应报文为空,一般不会出现这种情况
    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        //将响应报文的状态行、消息头、空行和响应正文发送给浏览器端
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            //判断缓冲区是否满了
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }
        //若writev单次发送成功，更新byte_to_send和byte_have_send的大小，
        bytes_have_send += temp;
        bytes_to_send -= temp;
       // 由于报文消息报头较小，第一次传输后，需要更新m_iv[1].iov_base和iov_len，m_iv[0].iov_len置成0，只传输文件，不用传输响应消息头
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            //每次传输后都要更新下次传输的文件起始位置和长度
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }
        //判断条件，数据已全部发送完
        if (bytes_to_send <= 0)
        {
            unmap();  //取消内存映射
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode); // //在epoll树上重置EPOLLONESHOT事件
            //浏览器的请求为长连接
            if (m_linger)
            {
                init(); //重新初始化HTTP对象
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}



bool http_conn::add_response(const char *format, ...)
{
    //写入的数据大于写缓冲区的数据`
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    //定义可变参数列表
    va_list arg_list;

    //将变量arg_list初始化为传入参数
    va_start(arg_list, format);
    //将数据format从可变参数列表写入缓冲区写，返回写入数据的长度
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
   //如果写入的数据长度超过缓冲区剩余空间，则报错
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    //更新m_write_idx位置
    m_write_idx += len;
     //清空可变参列表
    va_end(arg_list);
    //cout << "向用户发送数据:";
    //printf("%s\n",m_write_buf);
    LOG_INFO("request:%s", m_write_buf);

    return true;
}

//添加状态行：http/1.1 状态码 状态消息
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

//添加消息抱头,具体的添加文本长度/连接状态和空行
bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}
//添加Content-Length，表示响应报文的长度
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
//添加文本类型，这里是html
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}

//添加连接状态，通知浏览器端是保持连接还是关闭
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
//添加空行
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
//添加文本content
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}


bool http_conn::process_write(HTTP_CODE ret)
{
    //cout << "向http返回数据" << endl;
    switch (ret)
    {
    case INTERNAL_ERROR:    //服务器内部错误
    {
        add_status_line(500, error_500_title); //返回500错误
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:  //返回404错误,客户端请求错误
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST: //文件权限错误
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        //如果请求的资源存在
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
             //第一个iovec指针指向响应报文缓冲区，长度指向m_write_idx
            m_iv[0].iov_base = m_write_buf; //声明两个
            m_iv[0].iov_len = m_write_idx;
            //第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
             //如果请求的资源大小为0，则返回空白html文件
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    //除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

void http_conn::process()
{
    //cout << "处理http请求" <<endl;
    HTTP_CODE read_ret = process_read();

    if (read_ret == NO_REQUEST) //如果需要继续接收请求
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);    //将事件重置为EPOLLONESHOT
        return;
    }

    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);   //将事件重置为EPOLLONESHOT
}
