#include <jni.h>
#include <string>
#include <android/log.h>
#include <srt/srtcore/srt.h>

#define  LOG_TAG    "SRTClient"

#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C"
JNIEXPORT jstring

JNICALL
Java_com_example_srttest_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {

    int yes = 1;
    int no = 0;

    int status = srt_startup();
    if (status != 0) {
        LOGD("%s(%d):Failed \n", __FUNCTION__, __LINE__);
    }

    int client_pollid = srt_epoll_create();
    if (client_pollid == SRT_ERROR) {
        LOGD("%s(%d):Failed \n", __FUNCTION__, __LINE__);
    }

    SRTSOCKET  m_client_sock = srt_socket(AF_INET, SOCK_DGRAM, 0);

    status = srt_setsockopt(m_client_sock, 0, SRTO_SNDSYN, &no, sizeof no); // for async connect
    if (status == SRT_ERROR) {
        LOGD("%s(%d):Failed \n", __FUNCTION__, __LINE__);
    }

    srt_setsockflag(m_client_sock, SRTO_SENDER, &yes, sizeof yes);
    if (status == SRT_ERROR) {
        LOGD("%s(%d):Failed \n", __FUNCTION__, __LINE__);
    }

    status = srt_setsockopt(m_client_sock, 0, SRTO_TSBPDMODE, &yes, sizeof yes);
    if (status == SRT_ERROR) {
        LOGD("%s(%d):Failed \n", __FUNCTION__, __LINE__);
    }

    int epoll_out = SRT_EPOLL_OUT;
    srt_epoll_add_usock(client_pollid, m_client_sock, &epoll_out);

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(9000);

    if (inet_pton(AF_INET, "192.168.1.45", &sa.sin_addr) != 1) {
        LOGD("%s(%d):Failed \n", __FUNCTION__, __LINE__);
        std::string hello = "inet_pton failed";
        srt_cleanup();
        return env->NewStringUTF(hello.c_str());
    }

    struct sockaddr* psa = (struct sockaddr*)&sa;

    LOGD("%s(%d):srt_connect\n", __FUNCTION__, __LINE__);

    status = srt_connect(m_client_sock, psa, sizeof sa);
    if (status == SRT_ERROR) {
        LOGD("%s(%d):Failed \n", __FUNCTION__, __LINE__);
        LOGD("srt_connect: %s\n", srt_getlasterror_str());
        std::string hello = "srt_connect failed";
        srt_cleanup();
        return env->NewStringUTF(hello.c_str());
    }

    // Socket readiness for connection is checked by polling on WRITE allowed sockets.

    int i = 0;
    for (i = 0; i < 1000; i++) {
        {
            int rlen = 2;
            SRTSOCKET read[2];

            int wlen = 2;
            SRTSOCKET write[2];

            status = srt_epoll_wait(client_pollid, read, &rlen,
                                    write, &wlen,
                                    (int64_t)-1, // -1 is set for debuging purpose.
                    // in case of production we need to set appropriate value
                                    0, 0, 0, 0);
            if (status == SRT_ERROR) {
                LOGD("%s(%d):Failed \n", __FUNCTION__, __LINE__);
            }

            if (rlen != 0) {
                LOGD("%s(%d):Failed \n", __FUNCTION__, __LINE__);
            }

            if (wlen != 1) {
                LOGD("%s(%d):Failed \n", __FUNCTION__, __LINE__);
            }

            if (write[0] != m_client_sock) {
                LOGD("%s(%d):Failed \n", __FUNCTION__, __LINE__);
            }
        }
        LOGD("Send packet #%d\n", i);
        char buffer[1316] = {1, 2, 3, 4};

        status = srt_sendmsg(m_client_sock,
                             buffer,
                             sizeof buffer,
                             -1, // infinit ttl
                             1); // in order must be set to true
        if (status == SRT_ERROR) {
            LOGD("%s(%d):Failed \n", __FUNCTION__, __LINE__);
            LOGD("srt_sendmsg: %s\n", srt_getlasterror_str());
        }
    }

    usleep(1000 * 1000);
    LOGD("%s(%d):usleep\n", __FUNCTION__, __LINE__);
    srt_epoll_release(client_pollid);
    srt_cleanup();

    LOGD("%s(%d):EXIT\n", __FUNCTION__, __LINE__);

    std::string hello = "Connect test done";
    return env->NewStringUTF(hello.c_str());
}
