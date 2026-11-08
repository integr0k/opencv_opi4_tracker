// opencv_opi4_tracker — YOLO-детекция на RTSP-потоке, вывод через MJPEG.
//
// Плата (Orange Pi 4 Pro, серверная ОС, без GUI):
//   ./main [rtsp_url] [порт] [модель.onnx]
//   ./main 'rtsp://192.168.1.82:8080/h264_opus.sdp' 8000 yolo11n.onnx
//
// ПК в той же сети (браузер или VLC), ничего ставить не нужно:
//   http://<ip-платы>:8000
//
// Считает плата, картинку смотрим на ПК: разметленные кадры отдаются
// как multipart/x-mixed-replace MJPEG поверх обычного HTTP — никаких
// GUI-библиотек на плате (highgui не используется) и ничего на стороне ПК.

#include <opencv2/core/version.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/dnn.hpp>

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kDefaultPort = 8000;
constexpr int kJpegQuality = 75;  // 0..100: меньше — меньше байт по сети

// Последний разметленный кадр (уже JPEG), общий для всех клиентов.
struct FrameBox {
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<uchar> jpeg;
    uint64_t seq = 0;
};

// Отключился ли клиент (неблокирующая проверка).
bool client_gone(int fd) {
    char c;
    ssize_t n = recv(fd, &c, 1, MSG_PEEK | MSG_DONTWAIT);
    if (n == 0) return true;  // EOF — клиент закрыл соединение
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return true;
    return false;
}

bool send_all(int fd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

// Поток одного клиента: отдаёт multipart/x-mixed-replace MJPEG.
void serve_client(int fd, FrameBox& box, std::atomic<int>& clients) {
    clients.fetch_add(1);

    // прочитать и выбросить HTTP-запрос (браузер шлёт GET)
    std::string req;
    char buf[1024];
    while (req.find("\r\n\r\n") == std::string::npos && req.size() < 8192) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) {
            clients.fetch_sub(1);
            close(fd);
            return;
        }
        req.append(buf, static_cast<size_t>(n));
    }

    static const char* kHeader =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n";
    if (!send_all(fd, kHeader, std::strlen(kHeader))) {
        clients.fetch_sub(1);
        close(fd);
        return;
    }

    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    uint64_t last_seq = 0;
    bool ok = true;
    while (ok) {
        std::vector<uchar> jpeg;
        {
            std::unique_lock<std::mutex> lk(box.mtx);
            // ждём новый кадр, но не дольше 2 с, чтобы замечать отключение
            if (!box.cv.wait_for(lk, std::chrono::seconds(2),
                                 [&] { return box.seq != last_seq; })) {
                if (client_gone(fd)) break;
                continue;
            }
            jpeg = box.jpeg;
            last_seq = box.seq;
        }
        const std::string part =
            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
            std::to_string(jpeg.size()) + "\r\n\r\n";
        ok = send_all(fd, part.data(), part.size()) &&
             send_all(fd, reinterpret_cast<const char*>(jpeg.data()), jpeg.size()) &&
             send_all(fd, "\r\n", 2);
    }

    clients.fetch_sub(1);
    close(fd);
}

// Кодировать и выставить последний кадр для клиентов.
void publish(FrameBox& box, const cv::Mat& annotated, std::atomic<int>& clients) {
    if (clients.load(std::memory_order_relaxed) == 0) return;  // никого — не тратим CPU
    std::vector<uchar> jpeg;
    if (!cv::imencode(".jpg", annotated, jpeg,
                      {cv::IMWRITE_JPEG_QUALITY, kJpegQuality}))
        return;
    {
        std::lock_guard<std::mutex> lk(box.mtx);
        box.jpeg = std::move(jpeg);
        ++box.seq;
    }
    box.cv.notify_all();
}

// Все IPv4-адреса платы — чтобы показать готовые ссылки для браузера.
void print_stream_urls(int port) {
    ifaddrs* ifas = nullptr;
    if (getifaddrs(&ifas) != 0) return;
    for (ifaddrs* it = ifas; it; it = it->ifa_next) {
        if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) continue;
        if (it->ifa_flags & IFF_LOOPBACK) continue;
        char ip[INET_ADDRSTRLEN] = {0};
        auto* sa = reinterpret_cast<sockaddr_in*>(it->ifa_addr);
        if (!inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip))) continue;
        std::cout << "  http://" << ip << ":" << port << "\n";
    }
    freeifaddrs(ifas);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string url = argc > 1 ? argv[1] : "rtsp://192.168.1.82:8080/h264_opus.sdp";
    const int port = argc > 2 ? std::atoi(argv[2]) : kDefaultPort;
    const std::string model = argc > 3 ? argv[3] : "yolo11n.onnx";

    cv::dnn::Net net;
    try {
        net = cv::dnn::readNetFromONNX(model);
    } catch (const cv::Exception&) {
        std::cerr << "не удалось загрузить модель " << model << std::endl;
        return -1;
    }

    // --- MJPEG-сервер ------------------------------------------------------
    FrameBox box;
    std::atomic<int> clients{0};

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket");
        return -1;
    }
    int one = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        return -1;
    }
    listen(listener, 4);

    std::cout << "картинка для ПК (браузер/VLC):\n";
    print_stream_urls(port);
    std::cout << "источник: " << url << "\nмодель:   " << model << "\n";

    std::thread acceptor([&] {
        while (true) {
            int fd = accept(listener, nullptr, nullptr);
            if (fd < 0) continue;
            std::thread(serve_client, fd, std::ref(box), std::ref(clients)).detach();
        }
    });
    acceptor.detach();

    // --- захват RTSP + инференс --------------------------------------------
    const std::vector<int> cap_params = {
        cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 5000,
        cv::CAP_PROP_READ_TIMEOUT_MSEC, 5000,
    };

    while (true) {  // головной цикл: бесконечные переподключения
        cv::VideoCapture cam(url, cv::CAP_FFMPEG, cap_params);
        if (!cam.isOpened()) {
            std::cerr << "не удалось подключиться, повтор через 2 с" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        std::cout << "подключено" << std::endl;

        int64_t t0 = cv::getTickCount();
        int frames = 0;
        cv::Mat frame;
        while (true) {
            cam >> frame;
            if (frame.empty()) {
                std::cerr << "кадр не получен — переподключение" << std::endl;
                break;
            }

            cv::Mat blob;
            cv::dnn::blobFromImage(frame, blob, 1.0 / 255.0,
                                   cv::Size(640, 640), cv::Scalar(), true, false);
            net.setInput(blob);
            std::vector<cv::Mat> outputs;
            net.forward(outputs, net.getUnconnectedOutLayersNames());
            cv::Mat output = outputs[0];
            output = output.reshape(1, output.size[1]);
            cv::transpose(output, output);

            std::vector<cv::Rect> boxes;
            std::vector<float> confidences;
            std::vector<int> classIds;

            const float score_threshold = 0.25f;

            for (int i = 0; i < output.rows; ++i) {
                cv::Mat classes_scores = output.row(i).colRange(4, output.cols);
                cv::Point classIdPoint;
                double maxClassScore;
                cv::minMaxLoc(classes_scores, nullptr, &maxClassScore, nullptr, &classIdPoint);

                if (maxClassScore > score_threshold) {
                    float cx = output.at<float>(i, 0);
                    float cy = output.at<float>(i, 1);
                    float w = output.at<float>(i, 2);
                    float h = output.at<float>(i, 3);
                    int left = static_cast<int>((cx - w / 2) * frame.cols / 640.0);
                    int top = static_cast<int>((cy - h / 2) * frame.rows / 640.0);
                    int width = static_cast<int>(w * frame.cols / 640.0);
                    int height = static_cast<int>(h * frame.rows / 640.0);

                    boxes.push_back(cv::Rect(left, top, width, height));
                    confidences.push_back(static_cast<float>(maxClassScore));
                    classIds.push_back(classIdPoint.x);
                }
            }

            std::vector<int> indices;
            cv::dnn::NMSBoxes(boxes, confidences, score_threshold, 0.45, indices);

            for (int idx : indices) {
                cv::Rect box = boxes[idx];
                cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);
                std::string label = "ID: " + std::to_string(classIds[idx]);
                cv::putText(frame, label, cv::Point(box.x, box.y - 5),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
            }

            publish(box, frame, clients);

            if (++frames % 100 == 0) {
                double fps = 100.0 / ((cv::getTickCount() - t0) / cv::getTickFrequency());
                std::cout << "fps: " << fps << ", клиентов: " << clients.load() << std::endl;
                t0 = cv::getTickCount();
            }
        }
        cam.release();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
